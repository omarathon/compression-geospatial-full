#include <algorithm>
#include <cassert>
#include <cstdlib>
#include <limits>
#include <memory>
#include <format>
#include <iostream>
#include <random>
#include <string>
#include <vector>

#include <CLI/CLI.hpp>

#include "bench_gdal_utils.h"
#include "bench_utils.h"
#include "codec_collection.h"
#include "direct_codec.h"
#include "gdal_priv.h"

static std::vector<std::unique_ptr<StatefulIntegerCodec<int32_t>>>
BuildAllCodecs() {
  auto pool = InitCodecs(/* nonCascaded */ true, nullptr);
  for (auto& c :
       InitCodecs(/* nonCascaded */ false, std::make_unique<DeltaCodec>()))
    pool.push_back(std::move(c));
  for (auto& c :
       InitCodecs(/* nonCascaded */ false, std::make_unique<RLECodec>()))
    pool.push_back(std::move(c));
  for (auto& c :
       InitCodecs(/* nonCascaded */ false, std::make_unique<FORCodec>()))
    pool.push_back(std::move(c));
  pool.push_back(std::make_unique<DirectAccessCodec>());
  return pool;
}

static std::vector<std::unique_ptr<StatefulIntegerCodec<int32_t>>>
SplitIntoFullBlocks(GDALRasterBand* band, int rasterWidth, int rasterHeight,
                    int blockSize, int numBlocks,
                    std::unique_ptr<StatefulIntegerCodec<int32_t>> baseCodec,
                    int32_t min, Transformation transformation,
                    Ordering ordering) {
  int blocksInWidth = rasterWidth / blockSize;
  int blocksInHeight = rasterHeight / blockSize;

  std::vector<std::unique_ptr<StatefulIntegerCodec<int32_t>>> codecs;
  codecs.reserve(numBlocks);

  for (auto& offset : SampleBlockOffsets(blocksInWidth, blocksInHeight,
                                          blockSize, numBlocks)) {
    std::vector<int32_t> blockData(blockSize * blockSize);
    CPLErr err =
        band->RasterIO(GF_Read, offset.x, offset.y, blockSize, blockSize,
                       blockData.data(), blockSize, blockSize, GDT_Int32, 0, 0);
    if (err != CE_None)
      throw std::runtime_error("Error reading raster block data");
    if (min < 0)
      for (auto& v : blockData) v += (-min);
    RemapAndTransform(blockData, ordering, transformation, blockSize);

    std::unique_ptr<StatefulIntegerCodec<int32_t>> cloned(
        baseCodec->CloneFresh());
    cloned->AllocEncoded(blockData.data(), blockData.size());
    cloned->EncodeArray(blockData.data(), blockData.size());
    codecs.push_back(std::move(cloned));
  }

  return codecs;
}

static void BenchmarkAccess(
    std::vector<std::unique_ptr<StatefulIntegerCodec<int32_t>>>& codecs,
    std::unique_ptr<StatefulIntegerCodec<int32_t>> accessCodec, int blockSize,
    AccessPattern accessPattern, AccessTransformation accessTransformation,
    RunningStats& statsDec, RunningStats& statsTrans, RunningStats& statsEnc) {
  srand(1);

  bool isDirectAccess = (codecs[0]->name() == "custom_direct_access");
  bool isDirectReenc = (accessCodec->name() == "custom_direct_access");
  bool dataChange = AccessTransformationMutatesData(accessTransformation);

  std::vector<int32_t> decbuf(blockSize * blockSize +
                               codecs[0]->GetOverflowSize(blockSize * blockSize));

  std::vector<std::size_t> accessIndexes(codecs.size());
  std::iota(accessIndexes.begin(), accessIndexes.end(), 0);
  if (accessPattern != AccessPattern::Linear) {
    std::default_random_engine engine(1);
    std::shuffle(accessIndexes.begin(), accessIndexes.end(), engine);
  }

  for (std::size_t i = 0; i < codecs.size(); i++) {
    std::size_t blockIndex = accessIndexes[i];
    auto& codec = codecs[blockIndex];

    auto benchblock = [&](std::vector<int32_t>& buf) {
      std::size_t decodeTime = 0;
      if (!isDirectAccess) {
        auto t0 = std::chrono::steady_clock::now();
        codec->DecodeArray(buf.data(), blockSize * blockSize);
        auto t1 = std::chrono::steady_clock::now();
        decodeTime = std::chrono::duration_cast<std::chrono::nanoseconds>(t1 - t0).count();
      }
      statsDec.Update(decodeTime);

      std::size_t transTime =
          ApplyAccessTransformation(buf, accessTransformation, blockSize);
      statsTrans.Update(transTime);

      if (dataChange) {
        std::unique_ptr<StatefulIntegerCodec<int32_t>> reenc(
            accessCodec->CloneFresh());
        if (isDirectReenc) {
          statsEnc.Update(0);
          reenc->AllocEncoded(buf.data(), blockSize * blockSize);
          reenc->EncodeArray(buf.data(), blockSize * blockSize);
        } else {
          reenc->AllocEncoded(buf.data(), blockSize * blockSize);
          auto t0 = std::chrono::steady_clock::now();
          reenc->EncodeArray(buf.data(), blockSize * blockSize);
          auto t1 = std::chrono::steady_clock::now();
          statsEnc.Update(
              std::chrono::duration_cast<std::chrono::nanoseconds>(t1 - t0)
                  .count());
        }
        codecs[blockIndex] = std::move(reenc);
      }
    };

    if (!isDirectAccess)
      benchblock(decbuf);
    else
      benchblock(codec->GetEncoded());
  }
}

// One (ordering × initTrans × accessTrans) combination.
struct BenchCombo {
  Ordering ordering;
  Transformation initTrans;
  AccessTransformation accessTrans;
};

static void RunOneCombination(
    GDALRasterBand* band, int nXSize, int nYSize, const char* filePath,
    int blockSize, int numBlocks, int numReps, int32_t min,
    const BenchCombo& combo, AccessPattern accessPattern,
    StatefulIntegerCodec<int32_t>& baseCodec,
    StatefulIntegerCodec<int32_t>& accessCodec) {
  std::cout << "**BENCHMARK ACCESS**\n";
  std::cout << std::format("file={},blocksize={},numblocks={},numreps={},basecodec={},"
               "accesscodec={},ordering={},initialtransformation={},"
               "sampleaccesspattern={},accesstransformation={}",
               filePath, blockSize, numBlocks, numReps,
               baseCodec.name(), accessCodec.name(),
               ToString(combo.ordering), ToString(combo.initTrans),
               ToString(accessPattern), ToString(combo.accessTrans)) << '\n';

  RunningStats statsDec, statsTrans, statsEnc;

  for (int rep = 0; rep < numReps; rep++) {
    std::unique_ptr<StatefulIntegerCodec<int32_t>> expBase(
        baseCodec.CloneFresh());
    std::unique_ptr<StatefulIntegerCodec<int32_t>> expAccess(
        accessCodec.CloneFresh());

    auto codecGrid =
        SplitIntoFullBlocks(band, nXSize, nYSize, blockSize, numBlocks,
                             std::move(expBase), min, combo.initTrans,
                             combo.ordering);
    if (codecGrid.empty()) {
      std::cerr << "NO CODECS FORMING GRID.\n";
      return;
    }

    BenchmarkAccess(codecGrid, std::move(expAccess), blockSize, accessPattern,
                    combo.accessTrans, statsDec, statsTrans, statsEnc);
  }

  std::cout << std::format("tottimedec:{},meantimedec:{},vartimedec:{},"
               "tottimetrans:{},meantimetrans:{},vartimetrans:{},"
               "tottimeenc:{},meantimeenc:{},vartimeenc:{}",
               statsDec.Total(),  statsDec.mean,  statsDec.Variance(),
               statsTrans.Total(), statsTrans.mean, statsTrans.Variance(),
               statsEnc.Total(),  statsEnc.mean,  statsEnc.Variance()) << '\n';
}

static void RunAllBenchmarks(
    GDALRasterBand* band, int nXSize, int nYSize, const char* filePath,
    int blockSize, int numBlocks, int numReps, int32_t min,
    std::vector<std::unique_ptr<StatefulIntegerCodec<int32_t>>>& baseCodecs,
    std::vector<std::unique_ptr<StatefulIntegerCodec<int32_t>>>& accessCodecs,
    const std::vector<std::string>& orderings,
    const std::vector<std::string>& initialTransformations,
    const std::vector<std::string>& accessTransformations,
    const std::vector<std::string>& sampleAccessPatterns) {
  // Build flat combo list so strings are parsed once, not per-iteration.
  std::vector<BenchCombo> combos;
  for (auto& o : orderings)
    for (auto& it : initialTransformations)
      for (auto& at : accessTransformations)
        combos.push_back({ParseOrdering(o), ParseTransformation(it),
                           ParseAccessTransformation(at)});

  for (auto& combo : combos)
    for (auto& baseCodec : baseCodecs)
      for (auto& accessCodec : accessCodecs)
        for (auto& pattern : sampleAccessPatterns)
          RunOneCombination(band, nXSize, nYSize, filePath, blockSize,
                            numBlocks, numReps, min, combo,
                            ParseAccessPattern(pattern), *baseCodec,
                            *accessCodec);
}

int main(int argc, char* argv[]) {
  CLI::App app{"Benchmark codec access-pattern performance on a GeoTIFF raster"};

  std::string filePath;
  int blockSize{}, numBlocks{}, numReps{};
  std::vector<std::string> initialCodecNames = {"all"};
  std::vector<std::string> accessCodecNames = {"all"};
  std::vector<std::string> orderings = {"default"};
  std::vector<std::string> initialTransformations = {"none"};
  std::vector<std::string> sampleAccessPatterns = {"linear"};
  std::vector<std::string> accessTransformations = {"linearXOR"};

  app.add_option("file", filePath, "GeoTIFF file path")->required();
  app.add_option("--blocksize,-b", blockSize, "Block side length in pixels")
      ->required();
  app.add_option("--numblocks,-n", numBlocks, "Number of blocks to sample")
      ->required();
  app.add_option("--numreps,-r", numReps, "Repetitions per combination")
      ->required();
  app.add_option("--icodec", initialCodecNames,
                 "Initial codec name(s), or 'all'");
  app.add_option("--acodec", accessCodecNames,
                 "Access codec name(s), or 'all'");
  app.add_option("--ordering", orderings,
                 "Block ordering(s): default|zigzag|morton");
  app.add_option("--itrans", initialTransformations,
                 "Initial transformation(s): none|Threshold|SmoothAndShift|"
                 "IndexBasedClassification|ValueBasedClassification|ValueShift");
  app.add_option("--pattern", sampleAccessPatterns,
                 "Access pattern(s): linear|random");
  app.add_option("--atrans", accessTransformations,
                 "Access transformation(s): linearXOR|linearSum|linearSumSimd|"
                 "linearSumFused|randomXOR|randomSum|Threshold|SmoothAndShift|"
                 "IndexBasedClassification|ValueBasedClassification|ValueShift");

  CLI11_PARSE(app, argc, argv);

  srand(1);  // rand() is used in random access patterns; seed before benchmarking.

  GDALAllRegister();
  GDALSetCacheMax(64 * 1024 * 1024);  // 64 MB — prevents GDAL cache inflating RSS
  GDALDataset* dataset =
      static_cast<GDALDataset*>(GDALOpen(filePath.c_str(), GA_ReadOnly));
  if (dataset == nullptr) {
    std::cerr << std::format("Failed to open file: {}", filePath) << '\n';
    return 1;
  }

  GDALRasterBand* band = dataset->GetRasterBand(1);
  int nXSize = band->GetXSize();
  int nYSize = band->GetYSize();

  int32_t min = std::numeric_limits<int32_t>::max();
  for (auto& offset : SampleBlockOffsets(nXSize / blockSize, nYSize / blockSize,
                                          blockSize, numBlocks))
    ComputeMinForBlock(band, offset.x, offset.y, blockSize, min);

  auto allCodecs_initial = BuildAllCodecs();
  auto allCodecs_access = BuildAllCodecs();
  auto baseCodecs = SelectCodecsByName(allCodecs_initial, initialCodecNames);
  auto accessCodecs = SelectCodecsByName(allCodecs_access, accessCodecNames);

  RunAllBenchmarks(band, nXSize, nYSize, filePath.c_str(), blockSize, numBlocks,
                   numReps, min, baseCodecs, accessCodecs, orderings,
                   initialTransformations, accessTransformations,
                   sampleAccessPatterns);

  GDALClose(dataset);
  return 0;
}
