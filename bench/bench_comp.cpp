#include <algorithm>
#include <chrono>
#include <limits>
#include <memory>
#include <format>
#include <iostream>
#include <numeric>
#include <ranges>
#include <type_traits>
#include <vector>

#include <CLI/CLI.hpp>

#include "bench_gdal_utils.h"
#include "bench_utils.h"
#include "codec_collection.h"
#include "codec_collection_uint16.h"
#include "codec_collection_uint8.h"
#include "gdal_priv.h"

enum class VerifyMode { None, Roundtrip, Sums };

// ── Benchmark + verification ────────────────────────────────────────────────

// Codecs that don't write decoded data (fused / direct_access).
static bool IsNonDecodingCodec(const std::string& name) {
  return name.find("fused") != std::string::npos ||
         name.find("direct_access") != std::string::npos;
}

// Standalone fused codecs that produce a sum in overflow slots.
// Composites ([+]_) are excluded: the fused codec sums the intermediate
// (delta/FOR/RLE-encoded) data, not the original.
static bool IsSumProducingCodec(const std::string& name) {
  return name.find("fused") != std::string::npos &&
         name.find("[+]_") == std::string::npos;
}

template <typename T>
static CodecStats BenchAndVerify(std::vector<T>& data,
                                 std::unique_ptr<StatefulIntegerCodec<T>>& codec,
                                 VerifyMode verify) {
  CodecStats stats;
  const std::string codecName = codec->name();

  codec->AllocEncoded(data.data(), data.size());
  auto startEncode = std::chrono::steady_clock::now();
  try {
    codec->EncodeArray(data.data(), data.size());
  } catch (const std::exception& e) {
    std::cerr << std::format("error encoding {}: {}", codecName, e.what()) << '\n';
    return stats;
  }
  auto endEncode = std::chrono::steady_clock::now();

  std::size_t numCodedValues = codec->EncodedNumValues();
  std::size_t sizeCodedValue = codec->EncodedSizeValue();

  std::vector<T> dataBack(data.size() + codec->GetOverflowSize(data.size()));
  auto startDecode = std::chrono::steady_clock::now();
  try {
    codec->DecodeArray(dataBack.data(), data.size());
  } catch (const std::exception& e) {
    std::cerr << std::format("error decoding {}: {}", codecName, e.what()) << '\n';
    return stats;
  }
  auto endDecode = std::chrono::steady_clock::now();

  // Roundtrip check — skip for codecs that don't write decoded data
  if (verify == VerifyMode::Roundtrip && !IsNonDecodingCodec(codecName)) {
    for (std::size_t i = 0; i < data.size(); i++) {
      if (data[i] != dataBack[i]) {
        std::cerr << std::format("ROUNDTRIP FAIL {}: i={} expected={} got={}",
                                 codecName, i, data[i], dataBack[i]) << '\n';
        codec = std::unique_ptr<StatefulIntegerCodec<T>>(codec->CloneFresh());
        return stats;
      }
    }
  }

  // Sum check — only for codecs that produce sums in overflow slots
  if (verify == VerifyMode::Sums && IsSumProducingCodec(codecName)) {
    uint32_t expected = 0;
    for (auto v : data)
      expected += static_cast<uint32_t>(v);

    uint32_t got;
    if constexpr (std::is_same_v<T, uint16_t>) {
      got = static_cast<uint32_t>(dataBack[data.size()]) |
            (static_cast<uint32_t>(dataBack[data.size() + 1]) << 16);
    } else {
      got = static_cast<uint32_t>(dataBack[data.size()]);
    }

    if (expected != got) {
      std::cerr << std::format("SUM FAIL {}: expected={} got={}",
                               codecName, expected, got) << '\n';
    }
  }

  stats.cf = static_cast<float>(numCodedValues * sizeCodedValue) /
             static_cast<float>(data.size() * sizeof(T));
  stats.bpi = static_cast<float>(numCodedValues * sizeCodedValue) /
              static_cast<float>(data.size());
  stats.tenc = static_cast<float>(
      std::chrono::duration_cast<std::chrono::nanoseconds>(endEncode - startEncode).count());
  stats.tdec = static_cast<float>(
      std::chrono::duration_cast<std::chrono::nanoseconds>(endDecode - startDecode).count());

  codec = std::unique_ptr<StatefulIntegerCodec<T>>(codec->CloneFresh());
  return stats;
}

// ── int32 pipeline ──────────────────────────────────────────────────────────

static std::vector<CodecStats> BenchmarkWindow(
    std::vector<int32_t>& windowData,
    std::vector<std::unique_ptr<StatefulIntegerCodec<int32_t>>>& codecs,
    VerifyMode verify) {
  std::vector<CodecStats> stats(codecs.size());
  std::ranges::transform(codecs, stats.begin(), [&](auto& codec) {
    return BenchAndVerify(windowData, codec, verify);
  });
  return stats;
}

static void RunBenchConfig(
    GDALRasterBand* band, int rasterWidth, int rasterHeight,
    const std::string& filePath, int blockSize, int nBlocks, int32_t globalMin,
    bool hasNoData, int32_t nodata32,
    const std::string& compositeName, Ordering ordering, Transformation trans,
    float maxNodataPct, bool normalize, int32_t globalGCD,
    std::vector<std::unique_ptr<StatefulIntegerCodec<int32_t>>>& codecs,
    VerifyMode verify) {
  std::cout << std::format("**BENCHMARK**\nfile={},blockSize={},nBlocks={},composite={},"
               "ordering={},transformation={},normalize={},maxNodataPct={},globalMin={},globalGCD={}",
               filePath, blockSize, nBlocks, compositeName,
               ToString(ordering), ToString(trans),
               normalize ? "true" : "false", maxNodataPct, globalMin, globalGCD) << '\n';

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

    if (hasNoData) {
      int nodataCount = static_cast<int>(std::ranges::count(blockData, nodata32));
      if (nodataCount * 100.0f / static_cast<float>(blockData.size()) > maxNodataPct)
        continue;
    }

    if (normalize) {
      for (auto& v : blockData) {
        if (hasNoData && v == nodata32)
          v = 0;
        else
          v = static_cast<int32_t>(
              (static_cast<int64_t>(v) - static_cast<int64_t>(globalMin)) / globalGCD);
      }
    } else if (globalMin < 0) {
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
    std::cout << std::format("c:{},n:{},cfmean:{},cfvar:{},bpimean:{},bpivar:{},"
                 "tencmean:{},tencvar:{},tdecmean:{},tdecvar:{}",
                 ci, sv.size(), cfm, cfv, bpim, bpiv, tem, tev, tdm, tdv) << '\n';
  }
}

// ── uint16 pipeline ─────────────────────────────────────────────────────────

static std::vector<CodecStats> BenchmarkWindowU16(
    std::vector<uint16_t>& windowData,
    std::vector<std::unique_ptr<StatefulIntegerCodec<uint16_t>>>& codecs,
    VerifyMode verify) {
  std::vector<CodecStats> stats(codecs.size());
  std::ranges::transform(codecs, stats.begin(), [&](auto& codec) {
    return BenchAndVerify(windowData, codec, verify);
  });
  return stats;
}

static void RunBenchConfigU16(
    GDALRasterBand* band, int rasterWidth, int rasterHeight,
    const std::string& filePath, int blockSize, int nBlocks,
    int16_t minShift, bool hasNoData, int16_t nodata16, uint16_t nodataU16,
    Ordering ordering,
    float maxNodataPct, bool normalize, uint16_t normMinU16, uint16_t normGCDU16,
    std::vector<std::unique_ptr<StatefulIntegerCodec<uint16_t>>>& codecs,
    VerifyMode verify) {
  std::cout << std::format("**BENCHMARK**\nfile={},blockSize={},nBlocks={},composite=none,"
               "ordering={},transformation=none,normalize={},maxNodataPct={},minShift={},normMinU16={},normGCDU16={}",
               filePath, blockSize, nBlocks, ToString(ordering),
               normalize ? "true" : "false", maxNodataPct, minShift, normMinU16, normGCDU16) << '\n';

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
    int nodataCount = 0;

    if (minShift < 0 || (hasNoData && nodata16 < 0)) {
      std::vector<int16_t> signed_buf(blockSize * blockSize);
      band->RasterIO(GF_Read, offset.x, offset.y, blockSize, blockSize,
                     signed_buf.data(), blockSize, blockSize, GDT_Int16, 0, 0);
      int32_t shift = -static_cast<int32_t>(minShift);
      for (size_t i = 0; i < blockData.size(); i++) {
        if (hasNoData && signed_buf[i] == nodata16) {
          nodataCount++;
          blockData[i] = 0;
        } else {
          blockData[i] = static_cast<uint16_t>(static_cast<int32_t>(signed_buf[i]) + shift);
        }
      }
    } else {
      band->RasterIO(GF_Read, offset.x, offset.y, blockSize, blockSize,
                     blockData.data(), blockSize, blockSize, GDT_UInt16, 0, 0);
      if (hasNoData) {
        for (auto& v : blockData) {
          if (v == nodataU16) { nodataCount++; v = 0; }
        }
      }
    }

    if (hasNoData &&
        nodataCount * 100.0f / static_cast<float>(blockData.size()) > maxNodataPct)
      continue;

    if (normalize) {
      for (auto& v : blockData) {
        if (v == 0) continue; // nodata sentinel, leave as 0
        v = static_cast<uint16_t>(
            (static_cast<uint32_t>(v) - static_cast<uint32_t>(normMinU16)) / normGCDU16);
      }
    }

    ApplyOrdering(blockData, ordering, blockSize);

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
    std::cout << std::format("c:{},n:{},cfmean:{},cfvar:{},bpimean:{},bpivar:{},"
                 "tencmean:{},tencvar:{},tdecmean:{},tdecvar:{}",
                 ci, sv.size(), cfm, cfv, bpim, bpiv, tem, tev, tdm, tdv) << '\n';
  }
}

// ── uint8 pipeline ──────────────────────────────────────────────────────────

static std::vector<CodecStats> BenchmarkWindowU8(
    std::vector<uint8_t>& windowData,
    std::vector<std::unique_ptr<StatefulIntegerCodec<uint8_t>>>& codecs,
    VerifyMode verify) {
  std::vector<CodecStats> stats(codecs.size());
  std::ranges::transform(codecs, stats.begin(), [&](auto& codec) {
    return BenchAndVerify(windowData, codec, verify);
  });
  return stats;
}

static void RunBenchConfigU8(
    GDALRasterBand* band, int rasterWidth, int rasterHeight,
    const std::string& filePath, int blockSize, int nBlocks,
    bool hasNoData, uint8_t nodata8,
    float maxNodataPct, Ordering ordering,
    std::vector<std::unique_ptr<StatefulIntegerCodec<uint8_t>>>& codecs,
    VerifyMode verify) {
  std::cout << std::format("**BENCHMARK**\nfile={},blockSize={},nBlocks={},composite=none,"
               "ordering={},transformation=none,normalize=false,maxNodataPct={}",
               filePath, blockSize, nBlocks, ToString(ordering), maxNodataPct) << '\n';

  std::cout << "*CODECS:*\n";
  for (std::size_t ci = 0; ci < codecs.size(); ++ci)
    std::cout << std::format("{}={}", ci, codecs[ci]->name()) << '\n';
  std::cout << "*ENDCODECS*\n";

  int blocksInWidth  = rasterWidth  / blockSize;
  int blocksInHeight = rasterHeight / blockSize;

  std::vector<std::vector<CodecStats>> codecWindowStats(codecs.size());

  for (auto& offset :
       SampleBlockOffsets(blocksInWidth, blocksInHeight, blockSize, nBlocks)) {
    std::vector<uint8_t> blockData(blockSize * blockSize);
    band->RasterIO(GF_Read, offset.x, offset.y, blockSize, blockSize,
                   blockData.data(), blockSize, blockSize, GDT_Byte, 0, 0);

    if (hasNoData) {
      int nodataCount = static_cast<int>(std::ranges::count(blockData, nodata8));
      if (nodataCount * 100.0f / static_cast<float>(blockData.size()) > maxNodataPct)
        continue;
      for (auto& v : blockData)
        if (v == nodata8) v = 0;
    }

    if (static_cast<int>(blockData.size()) != blockSize * blockSize) continue;

    ApplyOrdering(blockData, ordering, blockSize);
    auto blockStats = BenchmarkWindowU8(blockData, codecs, verify);
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
    std::cout << std::format("c:{},n:{},cfmean:{},cfvar:{},bpimean:{},bpivar:{},"
                 "tencmean:{},tencvar:{},tdecmean:{},tdecvar:{}",
                 ci, sv.size(), cfm, cfv, bpim, bpiv, tem, tev, tdm, tdv) << '\n';
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
  float maxNodataPct = 100.0f;
  bool normalize = false;

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
  app.add_option("--max-nodata-pct", maxNodataPct,
                 "Skip blocks whose nodata pixel % exceeds this (0-100, default: 100)");
  app.add_flag("--normalize", normalize,
               "Normalize blocks: subtract global min, divide by global GCD");

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

  if (dt == GDT_Byte) {
    // ── uint8 path (categorical data) ───────────────────────────────────
    if (hasNoData && (rawNoData < 0.0 || rawNoData > 255.0))
      hasNoData = false;
    uint8_t nodata8 = hasNoData ? static_cast<uint8_t>(rawNoData) : 0;

    for (auto& ordering : orderings) {
      Ordering orderingEnum = ParseOrdering(ordering);
      auto codecs = BuildAllCodecsU8();
      try {
        RunBenchConfigU8(band, rasterWidth, rasterHeight, filePath, blockSize,
                         nBlocks, hasNoData, nodata8, maxNodataPct, orderingEnum,
                         codecs, verify);
      } catch (const std::exception& e) {
        std::cout << " ERROR see cerr\n";
        std::cerr << std::format("Error: {}", e.what()) << '\n';
      }
    }
  } else if (dt == GDT_UInt16 || dt == GDT_Int16) {
    // ── uint16 path ──────────────────────────────────────────────────────
    // Validate nodata fits in the data type
    if (hasNoData) {
      if (dt == GDT_Int16 &&
          (rawNoData < -32768.0 || rawNoData > 32767.0))
        hasNoData = false;
      else if (dt == GDT_UInt16 &&
               (rawNoData < 0.0 || rawNoData > 65535.0))
        hasNoData = false;
    }
    int16_t nodata16 = hasNoData ? static_cast<int16_t>(rawNoData) : 0;
    uint16_t nodataU16 = hasNoData ? static_cast<uint16_t>(rawNoData) : 0;

    int16_t minShift = 0;
    uint16_t normMinU16 = 0;
    uint16_t normGCDU16 = 1;

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
      // normMinU16 = 0: post-shift non-nodata values have min 0

      if (normalize && min16 < std::numeric_limits<int16_t>::max()) {
        uint32_t gcdAcc = 0;
        for (int y = 0; y < rasterHeight / blockSize; ++y)
          for (int x = 0; x < rasterWidth / blockSize; ++x) {
            std::vector<int16_t> tmp(blockSize * blockSize);
            band->RasterIO(GF_Read, x * blockSize, y * blockSize,
                           blockSize, blockSize, tmp.data(),
                           blockSize, blockSize, GDT_Int16, 0, 0);
            for (auto v : tmp)
              if (!hasNoData || v != nodata16)
                gcdAcc = std::gcd(gcdAcc,
                    static_cast<uint32_t>(static_cast<int32_t>(v) - minShift));
          }
        normGCDU16 = gcdAcc > 0 ? static_cast<uint16_t>(gcdAcc) : 1;
      }
    } else if (normalize) {
      // UInt16 or Byte: scan for normMin then normGCD
      uint16_t minU16 = std::numeric_limits<uint16_t>::max();
      for (int y = 0; y < rasterHeight / blockSize; ++y)
        for (int x = 0; x < rasterWidth / blockSize; ++x) {
          std::vector<uint16_t> tmp(blockSize * blockSize);
          band->RasterIO(GF_Read, x * blockSize, y * blockSize,
                         blockSize, blockSize, tmp.data(),
                         blockSize, blockSize, GDT_UInt16, 0, 0);
          for (auto v : tmp)
            if (!hasNoData || v != nodataU16)
              minU16 = std::min(minU16, v);
        }
      if (minU16 < std::numeric_limits<uint16_t>::max()) {
        normMinU16 = minU16;
        uint32_t gcdAcc = 0;
        for (int y = 0; y < rasterHeight / blockSize; ++y)
          for (int x = 0; x < rasterWidth / blockSize; ++x) {
            std::vector<uint16_t> tmp(blockSize * blockSize);
            band->RasterIO(GF_Read, x * blockSize, y * blockSize,
                           blockSize, blockSize, tmp.data(),
                           blockSize, blockSize, GDT_UInt16, 0, 0);
            for (auto v : tmp)
              if (!hasNoData || v != nodataU16)
                gcdAcc = std::gcd(gcdAcc,
                    static_cast<uint32_t>(v) - static_cast<uint32_t>(normMinU16));
          }
        normGCDU16 = gcdAcc > 0 ? static_cast<uint16_t>(gcdAcc) : 1;
      }
    }

    for (auto& ordering : orderings) {
      Ordering orderingEnum = ParseOrdering(ordering);
      auto codecs = BuildAllCodecsU16();
      try {
        RunBenchConfigU16(band, rasterWidth, rasterHeight, filePath, blockSize,
                          nBlocks, minShift, hasNoData, nodata16, nodataU16,
                          orderingEnum, maxNodataPct, normalize, normMinU16,
                          normGCDU16, codecs, verify);
      } catch (const std::exception& e) {
        std::cout << " ERROR see cerr\n";
        std::cerr << std::format("Error: {}", e.what()) << '\n';
      }
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

    int32_t globalGCD = 1;
    if (normalize && globalMin < std::numeric_limits<int32_t>::max()) {
      uint64_t gcdAcc = 0;
      for (int y = 0; y < rasterHeight / blockSize; ++y)
        for (int x = 0; x < rasterWidth / blockSize; ++x) {
          std::vector<int32_t> tmp(blockSize * blockSize);
          band->RasterIO(GF_Read, x * blockSize, y * blockSize,
                         blockSize, blockSize, tmp.data(),
                         blockSize, blockSize, GDT_Int32, 0, 0);
          for (auto v : tmp)
            if (!hasNoData || v != nodata32)
              gcdAcc = std::gcd(gcdAcc, static_cast<uint64_t>(
                                    static_cast<int64_t>(v) - static_cast<int64_t>(globalMin)));
        }
      globalGCD = gcdAcc > 0 ? static_cast<int32_t>(gcdAcc) : 1;
    }

    // for (auto& compositeName : compositeNames) {
      for (auto& ordering : orderings) {
        auto codecs = BuildAllCodecs();
        Ordering orderingEnum = ParseOrdering(ordering);
        for (auto& transformation : transformations) {
          Transformation transEnum = ParseTransformation(transformation);
          try {
            RunBenchConfig(band, rasterWidth, rasterHeight, filePath, blockSize,
                           nBlocks, globalMin, hasNoData, nodata32,
                           "", orderingEnum, transEnum,
                           maxNodataPct, normalize, globalGCD, codecs, verify);
          } catch (const std::exception& e) {
            std::cout << " ERROR see cerr\n";
            std::cerr << std::format("Error: {}", e.what()) << '\n';
          }
        }
      }
    // }
  }

  GDALClose(dataset);
  return 0;
}
