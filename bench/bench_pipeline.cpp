#include <algorithm>
#include <cassert>
#include <cstdlib>
#include <limits>
#include <memory>
#include <format>
#include <iostream>
#include <numeric>
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
BuildAllCodecsPipeline() {
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
                    Transformation transformation, Ordering ordering,
                    bool normalize, int32_t globalGCD) {
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
    if (normalize) {
      for (auto& v : blockData) {
        if (hasNoData && v == nodata32)
          v = 0;
        else
          v = static_cast<int32_t>(
              (static_cast<int64_t>(v) - static_cast<int64_t>(min)) / globalGCD);
      }
    } else if (min < 0) {
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
    int blockSize, int numBlocks, int numReps, int numSkip, int32_t min,
    bool hasNoData, int32_t nodata32,
    const BenchCombo& combo, AccessPattern accessPattern,
    StatefulIntegerCodec<int32_t>& baseCodec,
    StatefulIntegerCodec<int32_t>& accessCodec,
    bool normalize, int32_t globalGCD) {
  std::cout << "**BENCHMARK ACCESS**\n";
  std::cout << std::format("file={},band={},blocksize={},numblocks={},numreps={},numskip={},basecodec={},"
               "accesscodec={},ordering={},initialtransformation={},"
               "sampleaccesspattern={},accesstransformation={},normalize={},globalGCD={}",
               filePath, band->GetBand(), blockSize, numBlocks, numReps, numSkip,
               baseCodec.name(), accessCodec.name(),
               ToString(combo.ordering), ToString(combo.initTrans),
               ToString(accessPattern), ToString(combo.accessTrans), normalize, globalGCD) << '\n';

  RunningStats statsDec, statsTrans, statsEnc;
  RepMedian medDec, medTrans, medEnc;

  for (int rep = 0; rep < numReps; rep++) {
    std::unique_ptr<StatefulIntegerCodec<int32_t>> expBase(
        baseCodec.CloneFresh());
    std::unique_ptr<StatefulIntegerCodec<int32_t>> expAccess(
        accessCodec.CloneFresh());

    auto codecGrid =
        SplitIntoFullBlocks(band, nXSize, nYSize, blockSize, numBlocks,
                             std::move(expBase), min, hasNoData, nodata32,
                             combo.initTrans, combo.ordering, normalize, globalGCD);
    if (codecGrid.empty()) {
      std::cerr << "NO CODECS FORMING GRID.\n";
      return;
    }

    if (rep < numSkip) {
      RunningStats dummy1, dummy2, dummy3;
      BenchmarkAccess(codecGrid, std::move(expAccess), blockSize, accessPattern,
                      combo.accessTrans, dummy1, dummy2, dummy3);
    } else {
      double prevDec = statsDec.Total(), prevTrans = statsTrans.Total(), prevEnc = statsEnc.Total();
      BenchmarkAccess(codecGrid, std::move(expAccess), blockSize, accessPattern,
                      combo.accessTrans, statsDec, statsTrans, statsEnc);
      medDec.Push(statsDec.Total()   - prevDec,   codecGrid.size());
      medTrans.Push(statsTrans.Total() - prevTrans, codecGrid.size());
      medEnc.Push(statsEnc.Total()   - prevEnc,   codecGrid.size());
    }
  }

  std::cout << std::format("tottimedec:{},meantimedec:{},medtimedec:{},mintimedec:{},maxtimedec:{},vartimedec:{},"
               "tottimetrans:{},meantimetrans:{},medtimetrans:{},mintimetrans:{},maxtimetrans:{},vartimetrans:{},"
               "tottimeenc:{},meantimeenc:{},medtimeenc:{},mintimeenc:{},maxtimeenc:{},vartimeenc:{}",
               statsDec.Total(),   statsDec.mean,   medDec.Median(),   statsDec.Min(),   statsDec.Max(),   statsDec.Variance(),
               statsTrans.Total(), statsTrans.mean, medTrans.Median(), statsTrans.Min(), statsTrans.Max(), statsTrans.Variance(),
               statsEnc.Total(),   statsEnc.mean,   medEnc.Median(),   statsEnc.Min(),   statsEnc.Max(),   statsEnc.Variance()) << '\n';
}

static void RunAllBenchmarks(
    GDALRasterBand* band, int nXSize, int nYSize, const char* filePath,
    int blockSize, int numBlocks, int numReps, int numSkip, int32_t min,
    bool hasNoData, int32_t nodata32,
    std::vector<std::unique_ptr<StatefulIntegerCodec<int32_t>>>& baseCodecs,
    std::vector<std::unique_ptr<StatefulIntegerCodec<int32_t>>>& accessCodecs,
    const std::vector<std::string>& orderings,
    const std::vector<std::string>& initialTransformations,
    const std::vector<std::string>& accessTransformations,
    const std::vector<std::string>& sampleAccessPatterns,
    bool normalize, int32_t globalGCD) {
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
                            numBlocks, numReps, numSkip, min, hasNoData, nodata32,
                            combo, ParseAccessPattern(pattern), *baseCodec,
                            *accessCodec, normalize, globalGCD);
}

// ── uint16 pipeline ─────────────────────────────────────────────────────────

static std::vector<std::unique_ptr<StatefulIntegerCodec<uint16_t>>>
SplitIntoFullBlocksU16(GDALRasterBand* band, int rasterWidth, int rasterHeight,
                       int blockSize, int numBlocks,
                       std::unique_ptr<StatefulIntegerCodec<uint16_t>> baseCodec,
                       int16_t minShift, bool hasNoData, int16_t nodata16,
                       uint16_t nodataU16,
                       bool normalize, uint16_t normMinU16, uint16_t normGCDU16) {
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

    if (normalize) {
      for (auto& v : blockData) {
        if (v == 0) continue;  // nodata sentinel, leave as 0
        v = static_cast<uint16_t>(
            (static_cast<uint32_t>(v) - static_cast<uint32_t>(normMinU16)) / normGCDU16);
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
    int blockSize, int numBlocks, int numReps, int numSkip, int16_t minShift,
    bool hasNoData, int16_t nodata16, uint16_t nodataU16,
    const BenchCombo& combo, AccessPattern accessPattern,
    StatefulIntegerCodec<uint16_t>& baseCodec,
    StatefulIntegerCodec<uint16_t>& accessCodec,
    bool normalize, uint16_t normMinU16, uint16_t normGCDU16) {
  std::cout << "**BENCHMARK ACCESS**\n";
  std::cout << std::format("file={},band={},blocksize={},numblocks={},numreps={},numskip={},basecodec={},"
               "accesscodec={},ordering={},initialtransformation={},"
               "sampleaccesspattern={},accesstransformation={},normalize={},normMinU16={},normGCDU16={}",
               filePath, band->GetBand(), blockSize, numBlocks, numReps, numSkip,
               baseCodec.name(), accessCodec.name(),
               ToString(combo.ordering), ToString(combo.initTrans),
               ToString(accessPattern), ToString(combo.accessTrans),
               normalize, normMinU16, normGCDU16) << '\n';

  RunningStats statsDec, statsTrans, statsEnc;
  RepMedian medDec, medTrans, medEnc;
  double meanCompRatio      = 0.0;
  double meanExceptPerBlock = -1.0;

  // For a non-mutating access transform (sum/min/max/...), BenchmarkAccessU16
  // never re-encodes (dataChange=false), so the encoded grid is IDENTICAL every
  // rep. Build+read+encode it ONCE and reuse across reps instead of re-reading +
  // re-encoding the whole n-block sample every rep (the dominant cost at large n,
  // ~13x at r=10 rs=3). Mutating transforms keep the per-rep rebuild.
  const bool staticGrid = !AccessTransformationMutatesData(combo.accessTrans);

  auto compute_cr = [&](const auto& grid) {
    const double uncompBytes =
        static_cast<double>(blockSize) * blockSize * sizeof(uint16_t);
    double sumRatio = 0.0, sumExcept = 0.0;
    for (const auto& c : grid) {
      sumRatio  += (c->EncodedNumValues() * c->EncodedSizeValue()) / uncompBytes;
      sumExcept += c->MeanExceptionsPerInnerBlock();
    }
    const double n = static_cast<double>(grid.size());
    meanCompRatio      = sumRatio  / n;
    meanExceptPerBlock = sumExcept / n;
  };

  std::vector<std::unique_ptr<StatefulIntegerCodec<uint16_t>>> sharedGrid;
  if (staticGrid) {
    std::unique_ptr<StatefulIntegerCodec<uint16_t>> expBase(baseCodec.CloneFresh());
    sharedGrid =
        SplitIntoFullBlocksU16(band, nXSize, nYSize, blockSize, numBlocks,
                               std::move(expBase), minShift, hasNoData, nodata16,
                               nodataU16, normalize, normMinU16, normGCDU16);
    if (sharedGrid.empty()) {
      std::cerr << "NO CODECS FORMING GRID.\n";
      return;
    }
    compute_cr(sharedGrid);
  }

  for (int rep = 0; rep < numReps; rep++) {
    std::vector<std::unique_ptr<StatefulIntegerCodec<uint16_t>>> localGrid;
    if (!staticGrid) {
      std::unique_ptr<StatefulIntegerCodec<uint16_t>> expBase(baseCodec.CloneFresh());
      localGrid =
          SplitIntoFullBlocksU16(band, nXSize, nYSize, blockSize, numBlocks,
                                 std::move(expBase), minShift, hasNoData, nodata16,
                                 nodataU16, normalize, normMinU16, normGCDU16);
      if (localGrid.empty()) {
        std::cerr << "NO CODECS FORMING GRID.\n";
        return;
      }
      if (rep == 0) compute_cr(localGrid);
    }
    auto& codecGrid = staticGrid ? sharedGrid : localGrid;

    std::unique_ptr<StatefulIntegerCodec<uint16_t>> expAccess(
        accessCodec.CloneFresh());
    if (rep < numSkip) {
      RunningStats dummy1, dummy2, dummy3;
      BenchmarkAccessU16(codecGrid, std::move(expAccess), blockSize, accessPattern,
                         combo.accessTrans, dummy1, dummy2, dummy3);
    } else {
      double prevDec = statsDec.Total(), prevTrans = statsTrans.Total(), prevEnc = statsEnc.Total();
      BenchmarkAccessU16(codecGrid, std::move(expAccess), blockSize, accessPattern,
                         combo.accessTrans, statsDec, statsTrans, statsEnc);
      medDec.Push(statsDec.Total()     - prevDec,   codecGrid.size());
      medTrans.Push(statsTrans.Total() - prevTrans, codecGrid.size());
      medEnc.Push(statsEnc.Total()     - prevEnc,   codecGrid.size());
    }
  }

  std::cout << std::format("tottimedec:{},meantimedec:{},medtimedec:{},mintimedec:{},maxtimedec:{},vartimedec:{},"
               "tottimetrans:{},meantimetrans:{},medtimetrans:{},mintimetrans:{},maxtimetrans:{},vartimetrans:{},"
               "tottimeenc:{},meantimeenc:{},medtimeenc:{},mintimeenc:{},maxtimeenc:{},vartimeenc:{},"
               "meancompratio:{:.6f},meanexceptperblock:{:.3f}",
               statsDec.Total(),   statsDec.mean,   medDec.Median(),   statsDec.Min(),   statsDec.Max(),   statsDec.Variance(),
               statsTrans.Total(), statsTrans.mean, medTrans.Median(), statsTrans.Min(), statsTrans.Max(), statsTrans.Variance(),
               statsEnc.Total(),   statsEnc.mean,   medEnc.Median(),   statsEnc.Min(),   statsEnc.Max(),   statsEnc.Variance(),
               meanCompRatio, meanExceptPerBlock) << '\n';
}

static void RunAllBenchmarksU16(
    GDALRasterBand* band, int nXSize, int nYSize, const char* filePath,
    int blockSize, int numBlocks, int numReps, int numSkip, int16_t minShift,
    bool hasNoData, int16_t nodata16, uint16_t nodataU16,
    std::vector<std::unique_ptr<StatefulIntegerCodec<uint16_t>>>& baseCodecs,
    std::vector<std::unique_ptr<StatefulIntegerCodec<uint16_t>>>& accessCodecs,
    const std::vector<std::string>& orderings,
    const std::vector<std::string>& initialTransformations,
    const std::vector<std::string>& accessTransformations,
    const std::vector<std::string>& sampleAccessPatterns,
    bool normalize, uint16_t normMinU16, uint16_t normGCDU16) {
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
                              numBlocks, numReps, numSkip, minShift,
                              hasNoData, nodata16, nodataU16, combo,
                              ParseAccessPattern(pattern), *baseCodec,
                              *accessCodec, normalize, normMinU16, normGCDU16);
}

int main(int argc, char* argv[]) {
  CLI::App app{"Benchmark codec access-pattern performance on a GeoTIFF raster"};

  std::string filePath;
  int blockSize{}, numBlocks{}, numReps{}, numSkip{0};
  std::vector<std::string> initialCodecNames = {"all"};
  std::vector<std::string> accessCodecNames = {"all"};
  std::vector<std::string> orderings = {"default"};
  std::vector<std::string> initialTransformations = {"none"};
  std::vector<std::string> sampleAccessPatterns = {"linear"};
  std::vector<std::string> accessTransformations = {"linearXOR"};
  bool forceInt32 = false;
  bool traceSums = false;
  bool normalize = false;
  int threshold = 0;

  app.add_option("file", filePath, "GeoTIFF file path")->required();
  app.add_option("--blocksize,-b", blockSize, "Block side length in pixels")
      ->required();
  app.add_option("--numblocks,-n", numBlocks, "Number of blocks to sample")
      ->required();
  app.add_option("--numreps,-r", numReps, "Repetitions per combination")
      ->required();
  app.add_option("--rs", numSkip, "Warm-up reps to skip in statistics (default: 0)");
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
                 "linearSumFused|linearMin|linearMax|linearCountGtFxP|linearCountGtFP|"
                 "linearSumRecipDiv|linearSumRecipNR|"
                 "randomXOR|randomSum|Threshold|SmoothAndShift|"
                 "IndexBasedClassification|ValueBasedClassification|ValueShift");
  app.add_option("--threshold", threshold,
                 "Threshold T for linearCountGtFxP / linearCountGtFP (default: 0)");
  app.add_flag("--force-int32", forceInt32, "Force int32 pipeline for any raster type");
  app.add_flag("--trace-sums", traceSums, "Print per-block sums for verification");
  app.add_flag("--normalize", normalize,
               "Normalize blocks: subtract global min, divide by global GCD");

  CLI11_PARSE(app, argc, argv);
  gTraceSums = traceSums;
  kCountThreshold = static_cast<uint16_t>(std::clamp(threshold, 0, 65535));

  srand(1);  // rand() is used in random access patterns; seed before benchmarking.

  GDALAllRegister();
  GDALSetCacheMax(64 * 1024 * 1024);  // 64 MB — prevents GDAL cache inflating RSS
  GDALDataset* dataset =
      static_cast<GDALDataset*>(GDALOpen(filePath.c_str(), GA_ReadOnly));
  if (dataset == nullptr) {
    std::cerr << std::format("Failed to open file: {}", filePath) << '\n';
    return 1;
  }

  int nBands = dataset->GetRasterCount();
  for (int bandIdx = 1; bandIdx <= nBands; bandIdx++) {
  GDALRasterBand* band = dataset->GetRasterBand(bandIdx);
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
    uint16_t normMinU16 = 0;
    uint16_t normGCDU16 = 1;

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
      // normMinU16 = 0: post-shift non-nodata values have min 0

      if (normalize && min16 < std::numeric_limits<int16_t>::max()) {
        uint32_t gcdAcc = 0;
        for (auto& offset : SampleBlockOffsets(nXSize / blockSize,
                                                nYSize / blockSize, blockSize,
                                                numBlocks)) {
          std::vector<int16_t> tmp(blockSize * blockSize);
          band->RasterIO(GF_Read, offset.x, offset.y, blockSize, blockSize,
                         tmp.data(), blockSize, blockSize, GDT_Int16, 0, 0);
          for (auto v : tmp)
            if (!hasNoData || v != nodata16)
              gcdAcc = std::gcd(gcdAcc,
                  static_cast<uint32_t>(static_cast<int32_t>(v) - minShift));
        }
        normGCDU16 = gcdAcc > 0 ? static_cast<uint16_t>(gcdAcc) : 1;
      }
    } else if (normalize) {
      // UInt16: scan for normMin then normGCD
      uint16_t minU16 = std::numeric_limits<uint16_t>::max();
      for (auto& offset : SampleBlockOffsets(nXSize / blockSize,
                                              nYSize / blockSize, blockSize,
                                              numBlocks)) {
        std::vector<uint16_t> tmp(blockSize * blockSize);
        band->RasterIO(GF_Read, offset.x, offset.y, blockSize, blockSize,
                       tmp.data(), blockSize, blockSize, GDT_UInt16, 0, 0);
        for (auto v : tmp)
          if (!hasNoData || v != nodataU16)
            minU16 = std::min(minU16, v);
      }
      if (minU16 < std::numeric_limits<uint16_t>::max()) {
        normMinU16 = minU16;
        uint32_t gcdAcc = 0;
        for (auto& offset : SampleBlockOffsets(nXSize / blockSize,
                                                nYSize / blockSize, blockSize,
                                                numBlocks)) {
          std::vector<uint16_t> tmp(blockSize * blockSize);
          band->RasterIO(GF_Read, offset.x, offset.y, blockSize, blockSize,
                         tmp.data(), blockSize, blockSize, GDT_UInt16, 0, 0);
          for (auto v : tmp)
            if (!hasNoData || v != nodataU16)
              gcdAcc = std::gcd(gcdAcc,
                  static_cast<uint32_t>(v) - static_cast<uint32_t>(normMinU16));
        }
        normGCDU16 = gcdAcc > 0 ? static_cast<uint16_t>(gcdAcc) : 1;
      }
    }

    auto allCodecsU16_initial = BuildAllCodecsU16();
    auto allCodecsU16_access = BuildAllCodecsU16();
    auto baseCodecs =
        SelectCodecsByName(allCodecsU16_initial, initialCodecNames);
    auto accessCodecs =
        SelectCodecsByName(allCodecsU16_access, accessCodecNames);

    RunAllBenchmarksU16(band, nXSize, nYSize, filePath.c_str(), blockSize,
                        numBlocks, numReps, numSkip, minShift, hasNoData != 0, nodata16,
                        nodataU16, baseCodecs, accessCodecs, orderings,
                        initialTransformations, accessTransformations,
                        sampleAccessPatterns, normalize, normMinU16, normGCDU16);
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

    int32_t globalGCD = 1;
    if (normalize && min < std::numeric_limits<int32_t>::max()) {
      uint64_t gcdAcc = 0;
      for (auto& offset : SampleBlockOffsets(nXSize / blockSize,
                                              nYSize / blockSize, blockSize,
                                              numBlocks)) {
        std::vector<int32_t> tmp(blockSize * blockSize);
        band->RasterIO(GF_Read, offset.x, offset.y, blockSize, blockSize,
                       tmp.data(), blockSize, blockSize, GDT_Int32, 0, 0);
        for (auto v : tmp)
          if (!hasNoData32 || v != nodata32)
            gcdAcc = std::gcd(gcdAcc, static_cast<uint64_t>(
                                  static_cast<int64_t>(v) - static_cast<int64_t>(min)));
      }
      globalGCD = gcdAcc > 0 ? static_cast<int32_t>(gcdAcc) : 1;
    }

    auto allCodecs_initial = BuildAllCodecsPipeline();
    auto allCodecs_access = BuildAllCodecsPipeline();
    auto baseCodecs = SelectCodecsByName(allCodecs_initial, initialCodecNames);
    auto accessCodecs = SelectCodecsByName(allCodecs_access, accessCodecNames);

    RunAllBenchmarks(band, nXSize, nYSize, filePath.c_str(), blockSize,
                     numBlocks, numReps, numSkip, min, hasNoData32 != 0, nodata32,
                     baseCodecs, accessCodecs, orderings,
                     initialTransformations, accessTransformations,
                     sampleAccessPatterns, normalize, globalGCD);
  }

  } // bandIdx loop
  GDALClose(dataset);
  return 0;
}
