#include <algorithm>
#include <cassert>
#include <cstdlib>
#include <limits>
#include <memory>
#include <format>
#include <iostream>
#include <random>
#include <string>
#include <type_traits>
#include <vector>

#include <CLI/CLI.hpp>

#include "bench_gdal_utils.h"
#include "bench_utils.h"
#include "codec_collection.h"
#include "codec_collection_uint16.h"
#include "direct_codec.h"
#include "direct_codec_uint16.h"
#include "gdal_priv.h"

static bool gTraceSums = false;

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
                    int32_t min, bool hasNoData, int32_t nodata32,
                    Transformation transformation, Ordering ordering) {
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
    if (min < 0) {
      int32_t shift = -min;
      for (auto& v : blockData) {
        if (hasNoData && v == nodata32)
          v = 0;
        else
          v += shift;
      }
    } else if (hasNoData) {
      for (auto& v : blockData)
        if (v == nodata32) v = 0;
    }
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
      if (gTraceSums) std::cout << std::format("TRACE block={} sum={}", blockIndex, kLinearSumSink) << '\n';
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
    bool hasNoData, int32_t nodata32,
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
                             std::move(expBase), min, hasNoData, nodata32,
                             combo.initTrans, combo.ordering);
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
    bool hasNoData, int32_t nodata32,
    std::vector<std::unique_ptr<StatefulIntegerCodec<int32_t>>>& baseCodecs,
    std::vector<std::unique_ptr<StatefulIntegerCodec<int32_t>>>& accessCodecs,
    const std::vector<std::string>& orderings,
    const std::vector<std::string>& initialTransformations,
    const std::vector<std::string>& accessTransformations,
    const std::vector<std::string>& sampleAccessPatterns) {
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
                            numBlocks, numReps, min, hasNoData, nodata32,
                            combo, ParseAccessPattern(pattern), *baseCodec,
                            *accessCodec);
}

// ── uint16 pipeline ─────────────────────────────────────────────────────────

static std::vector<std::unique_ptr<StatefulIntegerCodec<uint16_t>>>
SplitIntoFullBlocksU16(GDALRasterBand* band, int rasterWidth, int rasterHeight,
                       int blockSize, int numBlocks,
                       std::unique_ptr<StatefulIntegerCodec<uint16_t>> baseCodec,
                       int16_t minShift, bool hasNoData, int16_t nodata16,
                       uint16_t nodataU16) {
  int blocksInWidth = rasterWidth / blockSize;
  int blocksInHeight = rasterHeight / blockSize;

  std::vector<std::unique_ptr<StatefulIntegerCodec<uint16_t>>> codecs;
  codecs.reserve(numBlocks);

  for (auto& offset : SampleBlockOffsets(blocksInWidth, blocksInHeight,
                                          blockSize, numBlocks)) {
    std::vector<uint16_t> blockData(blockSize * blockSize);
    if (minShift < 0) {
      // Read as int16, replace nodata with 0, shift to uint16
      std::vector<int16_t> signed_buf(blockSize * blockSize);
      CPLErr err =
          band->RasterIO(GF_Read, offset.x, offset.y, blockSize, blockSize,
                         signed_buf.data(), blockSize, blockSize, GDT_Int16, 0, 0);
      if (err != CE_None)
        throw std::runtime_error("Error reading raster block data");
      int32_t shift = -static_cast<int32_t>(minShift);
      for (size_t i = 0; i < blockData.size(); i++) {
        if (hasNoData && signed_buf[i] == nodata16)
          blockData[i] = 0;  // nodata → 0 contribution to sum
        else
          blockData[i] = static_cast<uint16_t>(static_cast<int32_t>(signed_buf[i]) + shift);
      }
    } else {
      CPLErr err =
          band->RasterIO(GF_Read, offset.x, offset.y, blockSize, blockSize,
                         blockData.data(), blockSize, blockSize, GDT_UInt16, 0, 0);
      if (err != CE_None)
        throw std::runtime_error("Error reading raster block data");
      if (hasNoData) {
        for (auto& v : blockData)
          if (v == nodataU16) v = 0;
      }
    }

    std::unique_ptr<StatefulIntegerCodec<uint16_t>> cloned(
        baseCodec->CloneFresh());
    cloned->AllocEncoded(blockData.data(), blockData.size());
    cloned->EncodeArray(blockData.data(), blockData.size());
    codecs.push_back(std::move(cloned));
  }

  return codecs;
}

static void BenchmarkAccessU16(
    std::vector<std::unique_ptr<StatefulIntegerCodec<uint16_t>>>& codecs,
    std::unique_ptr<StatefulIntegerCodec<uint16_t>> accessCodec, int blockSize,
    AccessPattern accessPattern, AccessTransformation accessTransformation,
    RunningStats& statsDec, RunningStats& statsTrans, RunningStats& statsEnc) {
  srand(1);

  bool isDirectAccess = (codecs[0]->name() == "custom_direct_access");
  bool isDirectReenc = (accessCodec->name() == "custom_direct_access");
  bool dataChange = AccessTransformationMutatesData(accessTransformation);

  std::vector<uint16_t> decbuf(blockSize * blockSize +
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

    auto benchblock = [&](std::vector<uint16_t>& buf) {
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
      if (gTraceSums) std::cout << std::format("TRACE block={} sum={}", blockIndex, kLinearSumSink) << '\n';
      statsTrans.Update(transTime);

      if (dataChange) {
        std::unique_ptr<StatefulIntegerCodec<uint16_t>> reenc(
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

static void RunOneCombinationU16(
    GDALRasterBand* band, int nXSize, int nYSize, const char* filePath,
    int blockSize, int numBlocks, int numReps, int16_t minShift,
    bool hasNoData, int16_t nodata16, uint16_t nodataU16,
    const BenchCombo& combo, AccessPattern accessPattern,
    StatefulIntegerCodec<uint16_t>& baseCodec,
    StatefulIntegerCodec<uint16_t>& accessCodec) {
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
    std::unique_ptr<StatefulIntegerCodec<uint16_t>> expBase(
        baseCodec.CloneFresh());
    std::unique_ptr<StatefulIntegerCodec<uint16_t>> expAccess(
        accessCodec.CloneFresh());

    auto codecGrid =
        SplitIntoFullBlocksU16(band, nXSize, nYSize, blockSize, numBlocks,
                               std::move(expBase), minShift, hasNoData, nodata16,
                               nodataU16);
    if (codecGrid.empty()) {
      std::cerr << "NO CODECS FORMING GRID.\n";
      return;
    }

    BenchmarkAccessU16(codecGrid, std::move(expAccess), blockSize, accessPattern,
                       combo.accessTrans, statsDec, statsTrans, statsEnc);
  }

  std::cout << std::format("tottimedec:{},meantimedec:{},vartimedec:{},"
               "tottimetrans:{},meantimetrans:{},vartimetrans:{},"
               "tottimeenc:{},meantimeenc:{},vartimeenc:{}",
               statsDec.Total(),  statsDec.mean,  statsDec.Variance(),
               statsTrans.Total(), statsTrans.mean, statsTrans.Variance(),
               statsEnc.Total(),  statsEnc.mean,  statsEnc.Variance()) << '\n';
}

static void RunAllBenchmarksU16(
    GDALRasterBand* band, int nXSize, int nYSize, const char* filePath,
    int blockSize, int numBlocks, int numReps, int16_t minShift,
    bool hasNoData, int16_t nodata16, uint16_t nodataU16,
    std::vector<std::unique_ptr<StatefulIntegerCodec<uint16_t>>>& baseCodecs,
    std::vector<std::unique_ptr<StatefulIntegerCodec<uint16_t>>>& accessCodecs,
    const std::vector<std::string>& orderings,
    const std::vector<std::string>& initialTransformations,
    const std::vector<std::string>& accessTransformations,
    const std::vector<std::string>& sampleAccessPatterns) {
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
          RunOneCombinationU16(band, nXSize, nYSize, filePath, blockSize,
                              numBlocks, numReps, minShift,
                              hasNoData, nodata16, nodataU16, combo,
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
  bool forceInt32 = false;
  bool traceSums = false;

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
  app.add_flag("--force-int32", forceInt32, "Force int32 pipeline for any raster type");
  app.add_flag("--trace-sums", traceSums, "Print per-block sums for verification");

  CLI11_PARSE(app, argc, argv);
  gTraceSums = traceSums;

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
  GDALDataType dt = band->GetRasterDataType();

  if (!forceInt32 && (dt == GDT_UInt16 || dt == GDT_Int16 || dt == GDT_Byte)) {
    // ── uint16 / int16 path ───────────────────────────────────────────────
    int hasNoData = 0;
    double rawNoData = band->GetNoDataValue(&hasNoData);
    // If the nodata value doesn't fit in the raster's data type, it can
    // never occur in the data — treat as "no nodata".
    if (hasNoData) {
      if (dt == GDT_Int16 &&
          (rawNoData < -32768.0 || rawNoData > 32767.0))
        hasNoData = 0;
      else if ((dt == GDT_UInt16 || dt == GDT_Byte) &&
               (rawNoData < 0.0 || rawNoData > 65535.0))
        hasNoData = 0;
    }
    int16_t nodata16 = hasNoData ? static_cast<int16_t>(rawNoData) : 0;
    uint16_t nodataU16 = hasNoData ? static_cast<uint16_t>(rawNoData) : 0;

    int16_t minShift = 0;
    if (dt == GDT_Int16) {
      // Compute min excluding nodata
      int16_t min16 = std::numeric_limits<int16_t>::max();
      for (auto& offset : SampleBlockOffsets(nXSize / blockSize,
                                              nYSize / blockSize, blockSize,
                                              numBlocks)) {
        std::vector<int16_t> tmp(blockSize * blockSize);
        band->RasterIO(GF_Read, offset.x, offset.y, blockSize, blockSize,
                       tmp.data(), blockSize, blockSize, GDT_Int16, 0, 0);
        for (auto v : tmp)
          if (!hasNoData || v != nodata16)
            min16 = std::min(min16, v);
      }
      minShift = min16;
    }

    auto allCodecsU16_initial = BuildAllCodecsU16();
    auto allCodecsU16_access = BuildAllCodecsU16();
    auto baseCodecs =
        SelectCodecsByName(allCodecsU16_initial, initialCodecNames);
    auto accessCodecs =
        SelectCodecsByName(allCodecsU16_access, accessCodecNames);

    RunAllBenchmarksU16(band, nXSize, nYSize, filePath.c_str(), blockSize,
                        numBlocks, numReps, minShift, hasNoData != 0, nodata16,
                        nodataU16, baseCodecs, accessCodecs, orderings,
                        initialTransformations, accessTransformations,
                        sampleAccessPatterns);
  } else {
    // ── int32 path (existing) ─────────────────────────────────────────────
    int hasNoData32 = 0;
    double rawNoData32 = band->GetNoDataValue(&hasNoData32);
    int32_t nodata32 = hasNoData32 ? static_cast<int32_t>(rawNoData32) : 0;

    int32_t min = std::numeric_limits<int32_t>::max();
    for (auto& offset : SampleBlockOffsets(nXSize / blockSize,
                                            nYSize / blockSize, blockSize,
                                            numBlocks)) {
      std::vector<int32_t> tmp(blockSize * blockSize);
      band->RasterIO(GF_Read, offset.x, offset.y, blockSize, blockSize,
                     tmp.data(), blockSize, blockSize, GDT_Int32, 0, 0);
      for (auto v : tmp)
        if (!hasNoData32 || v != nodata32)
          min = std::min(min, v);
    }

    auto allCodecs_initial = BuildAllCodecs();
    auto allCodecs_access = BuildAllCodecs();
    auto baseCodecs = SelectCodecsByName(allCodecs_initial, initialCodecNames);
    auto accessCodecs = SelectCodecsByName(allCodecs_access, accessCodecNames);

    RunAllBenchmarks(band, nXSize, nYSize, filePath.c_str(), blockSize,
                     numBlocks, numReps, min, hasNoData32 != 0, nodata32,
                     baseCodecs, accessCodecs, orderings,
                     initialTransformations, accessTransformations,
                     sampleAccessPatterns);
  }

  GDALClose(dataset);
  return 0;
}
