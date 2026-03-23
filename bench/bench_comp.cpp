#include <algorithm>
#include <limits>
#include <memory>
#include <format>
#include <iostream>
#include <numeric>
#include <ranges>
#include <vector>

#include <CLI/CLI.hpp>

#include "bench_gdal_utils.h"
#include "bench_utils.h"
#include "codec_collection.h"
#include "codec_collection_uint16.h"
#include "gdal_priv.h"

enum class VerifyMode { None, Roundtrip, Sums };

// If `compositeName` matches a codec in the non-cascaded pool, returns that
// codec cascaded with all physical codecs. Otherwise returns the full
// non-cascaded pool (the "none" / default case).
static std::vector<std::unique_ptr<StatefulIntegerCodec<int32_t>>>
BuildCodecsForComposite(const std::string& compositeName) {
  auto pool = InitCodecs(/* nonCascaded */ true, nullptr);
  for (auto& codec : pool) {
    if (codec->name() == compositeName) {
      return InitCodecs(/* nonCascaded */ false,
                        std::unique_ptr<StatefulIntegerCodec<int32_t>>(
                            codec->CloneFresh()));
    }
  }
  return pool;
}

// ── Verification helpers ────────────────────────────────────────────────────

template <typename T>
static bool VerifyRoundtrip(const std::vector<T>& original,
                            const std::vector<T>& decoded,
                            const std::string& codecName) {
  for (std::size_t i = 0; i < original.size(); i++) {
    if (original[i] != decoded[i]) {
      std::cerr << std::format("ROUNDTRIP FAIL {}: i={} expected={} got={}",
                               codecName, i, original[i], decoded[i]) << '\n';
      return false;
    }
  }
  return true;
}

// For fused codecs that store a uint32 sum in overflow slots.
// uint16 codecs: sum stored in out[length] (lo16) and out[length+1] (hi16).
static bool VerifySumU16(const std::vector<uint16_t>& original,
                         const std::vector<uint16_t>& decoded,
                         const std::string& codecName) {
  uint32_t expected = 0;
  for (auto v : original)
    expected += v;
  uint32_t got = static_cast<uint32_t>(decoded[original.size()]) |
                 (static_cast<uint32_t>(decoded[original.size() + 1]) << 16);
  if (expected != got) {
    std::cerr << std::format("SUM FAIL {}: expected={} got={}",
                             codecName, expected, got) << '\n';
    return false;
  }
  return true;
}

// For int32 fused codecs: sum stored in out[length] as int32.
static bool VerifySum32(const std::vector<int32_t>& original,
                        const std::vector<int32_t>& decoded,
                        const std::string& codecName) {
  int64_t expected = 0;
  for (auto v : original)
    expected += v;
  int32_t got = decoded[original.size()];
  // Compare lower 32 bits
  if (static_cast<int32_t>(expected) != got) {
    std::cerr << std::format("SUM FAIL {}: expected={} got={}",
                             codecName, static_cast<int32_t>(expected), got) << '\n';
    return false;
  }
  return true;
}

// ── int32 pipeline ──────────────────────────────────────────────────────────

static std::vector<CodecStats> BenchmarkWindow(
    std::vector<int32_t>& windowData,
    std::vector<std::unique_ptr<StatefulIntegerCodec<int32_t>>>& codecs,
    VerifyMode verify) {
  std::vector<CodecStats> stats(codecs.size());
  std::ranges::transform(codecs, stats.begin(), [&](auto& codec) {
    auto s = BenchmarkOneCodec(windowData, codec);
    if (verify == VerifyMode::Sums) {
      // Re-encode+decode for sum verification (codec was already reset)
      auto c2 = std::unique_ptr<StatefulIntegerCodec<int32_t>>(codec->CloneFresh());
      c2->AllocEncoded(windowData.data(), windowData.size());
      c2->EncodeArray(windowData.data(), windowData.size());
      std::vector<int32_t> dec(windowData.size() + c2->GetOverflowSize(windowData.size()));
      c2->DecodeArray(dec.data(), windowData.size());
      VerifySum32(windowData, dec, c2->name());
    }
    return s;
  });
  return stats;
}

static void RunBenchConfig(
    GDALRasterBand* band, int rasterWidth, int rasterHeight,
    const std::string& filePath, int blockSize, int nBlocks, int32_t globalMin,
    bool hasNoData, int32_t nodata32,
    const std::string& compositeName, Ordering ordering, Transformation trans,
    std::vector<std::unique_ptr<StatefulIntegerCodec<int32_t>>>& codecs,
    VerifyMode verify) {
  std::cout << std::format("**BENCHMARK**\nfile={},blockSize={},nBlocks={},composite={},"
               "ordering={},transformation={}",
               filePath, blockSize, nBlocks, compositeName,
               ToString(ordering), ToString(trans)) << '\n';

  std::cout << "*CODECS:*\n";
  for (std::size_t ci = 0; ci < codecs.size(); ++ci)
    std::cout << std::format("{}={}", ci, codecs[ci]->name()) << '\n';
  std::cout << "*ENDCODECS*\n";

  int blocksInWidth = rasterWidth / blockSize;
  int blocksInHeight = rasterHeight / blockSize;

  std::vector<std::vector<CodecStats>> codecWindowStats(codecs.size());

  for (auto& offset :
       SampleBlockOffsets(blocksInWidth, blocksInHeight, blockSize, nBlocks)) {
    auto blockData = ReadGeoTiffBlock(band, offset.x, offset.y, blockSize,
                                      rasterWidth, rasterHeight);
    if (static_cast<int>(blockData.size()) != blockSize * blockSize) continue;
    if (globalMin < 0) {
      int32_t shift = -globalMin;
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
    RemapAndTransform(blockData, ordering, trans, blockSize);
    auto blockStats = BenchmarkWindow(blockData, codecs, verify);
    for (std::size_t ci = 0; ci < codecs.size(); ++ci)
      codecWindowStats[ci].push_back(blockStats[ci]);
  }

  for (std::size_t ci = 0; ci < codecs.size(); ++ci) {
    auto& sv = codecWindowStats[ci];
    std::vector<float> cfs, bpis, tencs, tdecs;
    cfs.reserve(sv.size());
    bpis.reserve(sv.size());
    tencs.reserve(sv.size());
    tdecs.reserve(sv.size());
    std::ranges::transform(sv, std::back_inserter(cfs),   &CodecStats::cf);
    std::ranges::transform(sv, std::back_inserter(bpis),  &CodecStats::bpi);
    std::ranges::transform(sv, std::back_inserter(tencs), &CodecStats::tenc);
    std::ranges::transform(sv, std::back_inserter(tdecs), &CodecStats::tdec);
    float cfm = Mean(cfs),   cfv  = Variance(cfs,   cfm);
    float bpim = Mean(bpis), bpiv = Variance(bpis,  bpim);
    float tem  = Mean(tencs), tev = Variance(tencs,  tem);
    float tdm  = Mean(tdecs), tdv = Variance(tdecs,  tdm);
    std::cout << std::format("c:{},cfmean:{},cfvar:{},bpimean:{},bpivar:{},"
                 "tencmean:{},tencvar:{},tdecmean:{},tdecvar:{}",
                 ci, cfm, cfv, bpim, bpiv, tem, tev, tdm, tdv) << '\n';
  }
}

// ── uint16 pipeline ─────────────────────────────────────────────────────────

static std::vector<CodecStats> BenchmarkWindowU16(
    std::vector<uint16_t>& windowData,
    std::vector<std::unique_ptr<StatefulIntegerCodec<uint16_t>>>& codecs,
    VerifyMode verify) {
  std::vector<CodecStats> stats(codecs.size());
  std::ranges::transform(codecs, stats.begin(), [&](auto& codec) {
    auto s = BenchmarkOneCodec(windowData, codec);
    if (verify == VerifyMode::Sums) {
      auto c2 = std::unique_ptr<StatefulIntegerCodec<uint16_t>>(codec->CloneFresh());
      c2->AllocEncoded(windowData.data(), windowData.size());
      c2->EncodeArray(windowData.data(), windowData.size());
      std::vector<uint16_t> dec(windowData.size() + c2->GetOverflowSize(windowData.size()));
      c2->DecodeArray(dec.data(), windowData.size());
      VerifySumU16(windowData, dec, c2->name());
    }
    return s;
  });
  return stats;
}

static void RunBenchConfigU16(
    GDALRasterBand* band, int rasterWidth, int rasterHeight,
    const std::string& filePath, int blockSize, int nBlocks,
    int16_t minShift, bool hasNoData, int16_t nodata16, uint16_t nodataU16,
    std::vector<std::unique_ptr<StatefulIntegerCodec<uint16_t>>>& codecs,
    VerifyMode verify) {
  std::cout << std::format("**BENCHMARK**\nfile={},blockSize={},nBlocks={},composite=none,"
               "ordering=default,transformation=none",
               filePath, blockSize, nBlocks) << '\n';

  std::cout << "*CODECS:*\n";
  for (std::size_t ci = 0; ci < codecs.size(); ++ci)
    std::cout << std::format("{}={}", ci, codecs[ci]->name()) << '\n';
  std::cout << "*ENDCODECS*\n";

  int blocksInWidth = rasterWidth / blockSize;
  int blocksInHeight = rasterHeight / blockSize;

  std::vector<std::vector<CodecStats>> codecWindowStats(codecs.size());

  for (auto& offset :
       SampleBlockOffsets(blocksInWidth, blocksInHeight, blockSize, nBlocks)) {
    std::vector<uint16_t> blockData(blockSize * blockSize);
    if (minShift < 0) {
      std::vector<int16_t> signed_buf(blockSize * blockSize);
      band->RasterIO(GF_Read, offset.x, offset.y, blockSize, blockSize,
                     signed_buf.data(), blockSize, blockSize, GDT_Int16, 0, 0);
      int32_t shift = -static_cast<int32_t>(minShift);
      for (size_t i = 0; i < blockData.size(); i++) {
        if (hasNoData && signed_buf[i] == nodata16)
          blockData[i] = 0;
        else
          blockData[i] = static_cast<uint16_t>(static_cast<int32_t>(signed_buf[i]) + shift);
      }
    } else {
      band->RasterIO(GF_Read, offset.x, offset.y, blockSize, blockSize,
                     blockData.data(), blockSize, blockSize, GDT_UInt16, 0, 0);
      if (hasNoData) {
        for (auto& v : blockData)
          if (v == nodataU16) v = 0;
      }
    }

    auto blockStats = BenchmarkWindowU16(blockData, codecs, verify);
    for (std::size_t ci = 0; ci < codecs.size(); ++ci)
      codecWindowStats[ci].push_back(blockStats[ci]);
  }

  for (std::size_t ci = 0; ci < codecs.size(); ++ci) {
    auto& sv = codecWindowStats[ci];
    std::vector<float> cfs, bpis, tencs, tdecs;
    cfs.reserve(sv.size());
    bpis.reserve(sv.size());
    tencs.reserve(sv.size());
    tdecs.reserve(sv.size());
    std::ranges::transform(sv, std::back_inserter(cfs),   &CodecStats::cf);
    std::ranges::transform(sv, std::back_inserter(bpis),  &CodecStats::bpi);
    std::ranges::transform(sv, std::back_inserter(tencs), &CodecStats::tenc);
    std::ranges::transform(sv, std::back_inserter(tdecs), &CodecStats::tdec);
    float cfm = Mean(cfs),   cfv  = Variance(cfs,   cfm);
    float bpim = Mean(bpis), bpiv = Variance(bpis,  bpim);
    float tem  = Mean(tencs), tev = Variance(tencs,  tem);
    float tdm  = Mean(tdecs), tdv = Variance(tdecs,  tdm);
    std::cout << std::format("c:{},cfmean:{},cfvar:{},bpimean:{},bpivar:{},"
                 "tencmean:{},tencvar:{},tdecmean:{},tdecvar:{}",
                 ci, cfm, cfv, bpim, bpiv, tem, tev, tdm, tdv) << '\n';
  }
}

int main(int argc, char** argv) {
  CLI::App app{
      "Benchmark codec compression ratio and speed on a GeoTIFF raster"};

  std::string filePath;
  int blockSize{}, nBlocks{};
  std::vector<std::string> orderings = {"default"};
  std::vector<std::string> compositeNames = {"none"};
  std::vector<std::string> transformations = {"none"};
  bool checkRoundtrip = false;
  bool checkSums = false;

  app.add_option("file", filePath, "GeoTIFF file path")->required();
  app.add_option("--blocksize,-b", blockSize, "Block side length in pixels")
      ->required();
  app.add_option("--numblocks,-n", nBlocks, "Number of blocks to sample")
      ->required();
  app.add_option("--ordering", orderings,
                 "Block ordering(s): default|zigzag|morton");
  app.add_option(
      "--composite", compositeNames,
      "Cascade codec name(s), or 'none' for all non-cascaded codecs");
  app.add_option("--trans", transformations,
                 "Transformation(s): none|Threshold|SmoothAndShift|"
                 "IndexBasedClassification|ValueBasedClassification|ValueShift");
  app.add_flag("--check-roundtrip", checkRoundtrip,
               "Verify element-wise decode round-trip (non-fused codecs)");
  app.add_flag("--check-sums", checkSums,
               "Verify fused decode sum matches scalar sum");

  CLI11_PARSE(app, argc, argv);

  VerifyMode verify = VerifyMode::None;
  if (checkRoundtrip) verify = VerifyMode::Roundtrip;
  if (checkSums) verify = VerifyMode::Sums;

  GDALAllRegister();
  GDALDataset* dataset =
      static_cast<GDALDataset*>(GDALOpen(filePath.c_str(), GA_ReadOnly));
  if (!dataset) {
    std::cerr << std::format("Failed to open file: {}", filePath) << '\n';
    return 1;
  }

  GDALRasterBand* band = dataset->GetRasterBand(1);
  int rasterWidth = band->GetXSize();
  int rasterHeight = band->GetYSize();
  GDALDataType dt = band->GetRasterDataType();

  // Nodata setup
  int hasNoDataInt = 0;
  double rawNoData = band->GetNoDataValue(&hasNoDataInt);
  bool hasNoData = hasNoDataInt != 0;

  if (dt == GDT_UInt16 || dt == GDT_Int16 || dt == GDT_Byte) {
    // ── uint16 path ──────────────────────────────────────────────────────
    // Validate nodata fits in the data type
    if (hasNoData) {
      if (dt == GDT_Int16 &&
          (rawNoData < -32768.0 || rawNoData > 32767.0))
        hasNoData = false;
      else if ((dt == GDT_UInt16 || dt == GDT_Byte) &&
               (rawNoData < 0.0 || rawNoData > 65535.0))
        hasNoData = false;
    }
    int16_t nodata16 = hasNoData ? static_cast<int16_t>(rawNoData) : 0;
    uint16_t nodataU16 = hasNoData ? static_cast<uint16_t>(rawNoData) : 0;

    int16_t minShift = 0;
    if (dt == GDT_Int16) {
      int16_t min16 = std::numeric_limits<int16_t>::max();
      for (int y = 0; y < rasterHeight / blockSize; ++y)
        for (int x = 0; x < rasterWidth / blockSize; ++x) {
          std::vector<int16_t> tmp(blockSize * blockSize);
          band->RasterIO(GF_Read, x * blockSize, y * blockSize,
                         blockSize, blockSize, tmp.data(),
                         blockSize, blockSize, GDT_Int16, 0, 0);
          for (auto v : tmp)
            if (!hasNoData || v != nodata16)
              min16 = std::min(min16, v);
        }
      minShift = min16;
    }

    auto codecs = BuildAllCodecsU16();
    try {
      RunBenchConfigU16(band, rasterWidth, rasterHeight, filePath, blockSize,
                        nBlocks, minShift, hasNoData, nodata16, nodataU16,
                        codecs, verify);
    } catch (const std::exception& e) {
      std::cout << " ERROR see cerr\n";
      std::cerr << std::format("Error: {}", e.what()) << '\n';
    }
  } else {
    // ── int32 path ───────────────────────────────────────────────────────
    int32_t nodata32 = hasNoData ? static_cast<int32_t>(rawNoData) : 0;

    int32_t globalMin = std::numeric_limits<int32_t>::max();
    for (int y = 0; y < rasterHeight / blockSize; ++y)
      for (int x = 0; x < rasterWidth / blockSize; ++x) {
        std::vector<int32_t> tmp(blockSize * blockSize);
        band->RasterIO(GF_Read, x * blockSize, y * blockSize,
                       blockSize, blockSize, tmp.data(),
                       blockSize, blockSize, GDT_Int32, 0, 0);
        for (auto v : tmp)
          if (!hasNoData || v != nodata32)
            globalMin = std::min(globalMin, v);
      }

    for (auto& compositeName : compositeNames) {
      auto codecs = BuildCodecsForComposite(compositeName);
      for (auto& ordering : orderings) {
        Ordering orderingEnum = ParseOrdering(ordering);
        for (auto& transformation : transformations) {
          Transformation transEnum = ParseTransformation(transformation);
          try {
            RunBenchConfig(band, rasterWidth, rasterHeight, filePath, blockSize,
                           nBlocks, globalMin, hasNoData, nodata32,
                           compositeName, orderingEnum, transEnum, codecs,
                           verify);
          } catch (const std::exception& e) {
            std::cout << " ERROR see cerr\n";
            std::cerr << std::format("Error: {}", e.what()) << '\n';
          }
        }
      }
    }
  }

  GDALClose(dataset);
  return 0;
}
