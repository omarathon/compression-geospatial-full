#include <algorithm>
#include <iostream>
#include <limits>
#include <memory>
#include <vector>

#include <CLI/CLI.hpp>

#include "bench_gdal_utils.h"
#include "bench_utils.h"
#include "codec_collection.h"
#include "gdal_priv.h"

// ─── Codec helpers ────────────────────────────────────────────────────────────

// If `compositeName` matches a codec in the non-cascaded pool, returns that
// codec cascaded with all physical codecs.  Otherwise returns the full
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

static std::vector<CodecStats> BenchmarkWindow(
    std::vector<int32_t>& windowData,
    std::vector<std::unique_ptr<StatefulIntegerCodec<int32_t>>>& codecs) {
  std::vector<CodecStats> stats(codecs.size());
  for (int ci = 0; ci < static_cast<int>(codecs.size()); ci++) {
    stats[ci] = BenchmarkOneCodec(windowData, codecs[ci]);
  }
  return stats;
}

// ─── Per-configuration benchmark ─────────────────────────────────────────────

static void RunBenchConfig(
    GDALRasterBand* band, int rasterWidth, int rasterHeight,
    const std::string& filePath, int blockSize, int nBlocks, int32_t globalMin,
    const std::string& compositeName, Ordering orderingEnum,
    const std::string& ordering, Transformation transEnum,
    const std::string& transformation,
    std::vector<std::unique_ptr<StatefulIntegerCodec<int32_t>>>& codecs) {
  std::cout << "**BENCHMARK**\nfile=" << filePath
            << ",blockSize=" << blockSize << ",nBlocks=" << nBlocks
            << ",composite=" << compositeName << ",ordering=" << ordering
            << ",transformation=" << transformation << std::endl;

  std::cout << "*CODECS:*" << std::endl;
  for (int ci = 0; ci < static_cast<int>(codecs.size()); ci++) {
    std::cout << ci << "=" << codecs[ci]->name() << std::endl;
  }
  std::cout << "*ENDCODECS*" << std::endl;

  int blocksInWidth = rasterWidth / blockSize;
  int blocksInHeight = rasterHeight / blockSize;

  std::vector<std::vector<CodecStats>> codecWindowStats(codecs.size());

  for (auto& offset :
       SampleBlockOffsets(blocksInWidth, blocksInHeight, blockSize, nBlocks)) {
    auto blockData = ReadGeoTiffBlock(band, offset.x, offset.y, blockSize,
                                      rasterWidth, rasterHeight);
    if (static_cast<int>(blockData.size()) != blockSize * blockSize) continue;
    if (globalMin < 0) {
      for (auto& v : blockData) v += (-globalMin);
    }
    RemapAndTransform(blockData, orderingEnum, transEnum, blockSize);
    auto blockStats = BenchmarkWindow(blockData, codecs);
    for (int ci = 0; ci < static_cast<int>(codecs.size()); ci++) {
      codecWindowStats[ci].push_back(blockStats[ci]);
    }
  }

  for (int ci = 0; ci < static_cast<int>(codecs.size()); ci++) {
    std::cout << "c:" << ci;
    auto& sv = codecWindowStats[ci];
    std::vector<float> cfs, bpis, tencs, tdecs;
    for (auto& s : sv) {
      cfs.push_back(s.cf);
      bpis.push_back(s.bpi);
      tencs.push_back(s.tenc);
      tdecs.push_back(s.tdec);
    }
    float cfm = Mean(cfs), cfv = Variance(cfs, cfm);
    float bpim = Mean(bpis), bpiv = Variance(bpis, bpim);
    float tem = Mean(tencs), tev = Variance(tencs, tem);
    float tdm = Mean(tdecs), tdv = Variance(tdecs, tdm);
    std::cout << ",cfmean:" << cfm << ",cfvar:" << cfv
              << ",bpimean:" << bpim << ",bpivar:" << bpiv
              << ",tencmean:" << tem << ",tencvar:" << tev
              << ",tdecmean:" << tdm << ",tdecvar:" << tdv << std::endl;
  }
}

// ─── Main ─────────────────────────────────────────────────────────────────────

int main(int argc, char** argv) {
  CLI::App app{
      "Benchmark codec compression ratio and speed on a GeoTIFF raster"};

  std::string filePath;
  int blockSize{}, nBlocks{};
  std::vector<std::string> orderings = {"default"};
  std::vector<std::string> compositeNames = {"none"};
  std::vector<std::string> transformations = {"none"};

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

  CLI11_PARSE(app, argc, argv);

  GDALAllRegister();
  GDALDataset* dataset =
      (GDALDataset*)GDALOpen(filePath.c_str(), GA_ReadOnly);
  if (!dataset) {
    std::cerr << "Failed to open file: " << filePath << std::endl;
    return 1;
  }

  GDALRasterBand* band = dataset->GetRasterBand(1);
  int rasterWidth = band->GetXSize();
  int rasterHeight = band->GetYSize();

  // Compute the global minimum across the entire raster (all full blocks).
  int32_t globalMin = std::numeric_limits<int32_t>::max();
  for (int y = 0; y < rasterHeight / blockSize; ++y) {
    for (int x = 0; x < rasterWidth / blockSize; ++x) {
      ComputeMinForBlock(band, x * blockSize, y * blockSize, blockSize,
                         globalMin);
    }
  }

  for (auto& compositeName : compositeNames) {
    auto codecs = BuildCodecsForComposite(compositeName);
    for (auto& ordering : orderings) {
      Ordering orderingEnum = ParseOrdering(ordering);
      for (auto& transformation : transformations) {
        Transformation transEnum = ParseTransformation(transformation);
        try {
          RunBenchConfig(band, rasterWidth, rasterHeight, filePath, blockSize,
                         nBlocks, globalMin, compositeName, orderingEnum,
                         ordering, transEnum, transformation, codecs);
        } catch (const std::exception& e) {
          std::cout << " ERROR see cerr " << std::endl;
          std::cerr << "Error: " << e.what() << std::endl;
        }
      }
    }
  }

  GDALClose(dataset);
  return 0;
}
