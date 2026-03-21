#include <algorithm>
#include <chrono>
#include <cmath>
#include <iostream>
#include <memory>
#include <vector>

#include "codec_collection.h"
#include "gdal_priv.h"
#include "remappings.h"
#include "transformations.h"
#include "util.h"

struct CodecStats {
  float cf = 0;
  float bpi = 0;
  float tenc = 0;
  float tdec = 0;
};

void remapAndTransformData(std::vector<int32_t>& blockData,
                           const std::string& ordering,
                           const std::string& transformation,
                           const int blockSize) {
  // blockData is currently in row-major order. remap according to desired
  // ordering
  if (ordering == "zigzag") {
    auto remappedBlockData = RemapToZigzagOrder(blockData, blockSize);
    std::copy(remappedBlockData.begin(), remappedBlockData.end(),
              blockData.begin());
  } else if (ordering == "morton") {
    auto remappedBlockData = RemapToMortonOrder(blockData, blockSize);
    std::copy(remappedBlockData.begin(), remappedBlockData.end(),
              blockData.begin());
  }

  // apply transformation if desired
  if (transformation == "Threshold") {
    Threshold(blockData, /* threshold_value */ Avg(blockData));
  } else if (transformation == "SmoothAndShift") {
    SmoothAndShift(blockData);
  } else if (transformation == "IndexBasedClassification") {
    IndexBasedClassification(blockData, /* max_classes */ 8);
  } else if (transformation == "ValueBasedClassification") {
    ValueBasedClassification(blockData, /* num_classes */ 8);
  } else if (transformation == "ValueShift") {
    ValueShift(blockData, /* delta */ pow(2, 23));
  }
}

// Function to benchmark a window of data
std::vector<CodecStats> benchmarkWindow(
    std::vector<int32_t>& windowData,
    std::vector<std::unique_ptr<StatefulIntegerCodec<int32_t>>>& codecs) {
  std::vector<CodecStats> allCodecStats(codecs.size());

  for (int ci = 0; ci < codecs.size(); ci++) {
    auto& codec = codecs[ci];

    CodecStats stats;

    codec->AllocEncoded(windowData.data(), windowData.size());

    auto startEncode = std::chrono::high_resolution_clock::now();
    try {
      codec->EncodeArray(windowData.data(), windowData.size());
    } catch (const std::exception& error) {
      std::cout << " ERROR see cerr " << std::endl;
      std::cerr << "error encoding " << codec->name() << ": " << error.what()
                << std::endl;
      continue;
    }
    auto endEncode = std::chrono::high_resolution_clock::now();

    size_t numCodedValues = codec->EncodedNumValues();
    size_t sizeCodedValue = codec->EncodedSizeValue();

    std::vector<int32_t> windowDataBack(
        windowData.size() + codec->GetOverflowSize(windowData.size()));
    auto startDecode = std::chrono::high_resolution_clock::now();
    try {
      codec->DecodeArray(windowDataBack.data(), windowData.size());
    } catch (const std::exception& error) {
      std::cout << " ERROR see cerr " << std::endl;
      std::cerr << "error decoding " << codec->name() << ": " << error.what()
                << std::endl;
      continue;
    }
    auto endDecode = std::chrono::high_resolution_clock::now();

    // Verify
    bool good = true;
    for (int i = 0; i < windowData.size(); i++) {
      if (windowData[i] != windowDataBack[i]) {
        std::cout << " ERROR see cerr " << std::endl;
        std::cerr << "in!=out " << codec->name() << "(" << "i=" << i << ":o"
                  << windowData[i] << "b" << windowDataBack[i] << ","
                  << "len=" << windowData.size() << ")" << std::endl;
        good = false;
        break;
      }
    }

    if (!good) {
      continue;
    }

    // Calculate compression factor
    auto compressionFactor = (float)(numCodedValues * sizeCodedValue) /
                             (float)(windowData.size() * sizeof(int32_t));
    auto bitsPerInt =
        (float)(numCodedValues * sizeCodedValue) / (float)windowData.size();
    // std::cout << ",cf:" << compressionFactor << ",bpi:" << bitsPerInt;
    stats.cf = compressionFactor;
    stats.bpi = bitsPerInt;

    // Calculate time taken
    auto compressTime = std::chrono::duration_cast<std::chrono::nanoseconds>(
                            endEncode - startEncode)
                            .count();
    auto decompressTime = std::chrono::duration_cast<std::chrono::nanoseconds>(
                              endDecode - startDecode)
                              .count();
    // std::cout << ",tenc:" << compressTime << ",tdec:" << decompressTime <<
    // std::endl;
    stats.tenc = compressTime;
    stats.tdec = decompressTime;

    // Reset codec.
    codecs[ci] =
        std::unique_ptr<StatefulIntegerCodec<int32_t>>(codec->CloneFresh());

    allCodecStats[ci] = stats;
  }

  return allCodecStats;
}

std::vector<int32_t> readGeoTiffBlock(GDALRasterBand* band, int xOff, int yOff,
                                      int blockSize, int rasterWidth,
                                      int rasterHeight) {
  int blockWidth = std::min(blockSize, rasterWidth - xOff);
  int blockHeight = std::min(blockSize, rasterHeight - yOff);

  std::vector<int32_t> blockData(blockWidth * blockHeight);
  band->RasterIO(GF_Read, xOff, yOff, blockWidth, blockHeight, blockData.data(),
                 blockWidth, blockHeight, GDT_Int32, 0, 0);

  return blockData;
}

// Function to process a single block for min value
void computeMinForBlock(GDALRasterBand* band, int xOff, int yOff,
                        int blockSize, int32_t& min) {
  std::vector<int32_t> blockData(blockSize * blockSize);
  band->RasterIO(GF_Read, xOff, yOff, blockSize, blockSize, blockData.data(),
                 blockSize, blockSize, GDT_Int32, 0, 0);

  for (auto& value : blockData) {
    min = std::min(min, value);
  }
}

// BEHAVIOUR: if composite codec matches the name of a codec, then we will only
// benchmark composites of that codec + physical codecs. if composite codec does
// not match, then we benchmark all non-cascades (both logical & physical
// algorithms)
int main(int argc, char** argv) {
  if (argc != 7) {
    std::cerr << "usage: " << argv[0]
              << " <GeoTIFF file path> <block size> <num blocks> <block "
                 "ordering {default: row-major | 'zigzag' | 'morton'}>[] "
                 "<composite codec>[] <transformation {default: none | "
                 "'Threshold' | 'SmoothAndShift' | 'IndexBasedClassification' "
                 "| 'ValueBasedClassification' | 'ValueShift'}>[]"
              << std::endl;
    return 1;
  }

  GDALAllRegister();
  const char* filename = argv[1];
  int blockSize = std::stoi(argv[2]);
  int nBlocks = std::stoi(argv[3]);
  std::vector<std::string> orderings =
      ParseCommaDelimited(std::string(argv[4]));
  std::vector<std::string> composite_codec_names =
      ParseCommaDelimited(std::string(argv[5]));
  std::vector<std::string> transformations =
      ParseCommaDelimited(std::string(argv[6]));

  GDALDataset* dataset = (GDALDataset*)GDALOpen(filename, GA_ReadOnly);
  if (!dataset) {
    std::cerr << "Failed to open file: " << filename << std::endl;
    return 1;
  }

  GDALRasterBand* band = dataset->GetRasterBand(1);
  int rasterWidth = band->GetXSize();
  int rasterHeight = band->GetYSize();
  int totalPixels = rasterWidth * rasterHeight;

  /*
   * DONE PREPROCESSING
   */

  for (auto& composite_codec_name : composite_codec_names) {
    for (auto& ordering : orderings) {
      for (auto& transformation : transformations) {
        /* BEGIN SEARCH */

        try {
          std::cout << "**BENCHMARK**\nfile=" << argv[1]
                    << ",blockSize=" << blockSize << ",nBlocks=" << nBlocks
                    << ",composite=" << composite_codec_name
                    << ",ordering=" << ordering
                    << ",transformation=" << transformation << std::endl;

          int blocksInWidth = rasterWidth / blockSize;
          int blocksInHeight = rasterHeight / blockSize;

          int32_t min = std::numeric_limits<int32_t>::max();

          for (int y = 0; y < blocksInHeight; ++y) {
            for (int x = 0; x < blocksInWidth; ++x) {
              int xOff = x * blockSize;
              int yOff = y * blockSize;
              computeMinForBlock(band, xOff, yOff, blockSize, min);
            }
          }

          // Initialize codecs, optionally with a cascade.
          auto all_codecs = InitCodecs(/* nonCascaded */ true, nullptr);

          std::unique_ptr<StatefulIntegerCodec<int32_t>> baseCodec;
          for (auto& codec : all_codecs) {
            if (codec->name() == composite_codec_name) {
              baseCodec = std::move(codec);
              break;
            }
          }

          std::vector<std::unique_ptr<StatefulIntegerCodec<int32_t>>> codecs;
          if (baseCodec) {
            codecs = InitCodecs(/* nonCascaded */ false, std::move(baseCodec));
          } else {
            codecs = std::move(all_codecs);
          }

          std::cout << "*CODECS:*" << std::endl;
          for (int ci = 0; ci < codecs.size(); ci++) {
            std::cout << ci << "=" << codecs[ci]->name() << std::endl;
          }
          std::cout << "*ENDCODECS*" << std::endl;

          // Calculate the total number of blocks that fit in the raster
          int totalBlocks = blocksInWidth * blocksInHeight;

          // Calculate spacing between blocks to process
          int blockSpacing =
              nBlocks == 0 ? 0 : std::max(1, totalBlocks / nBlocks);

          // Counter for processed blocks
          int processedBlocks = 0;
          int windowIndex = 0;

          // Benchmark each window
          std::vector<std::vector<CodecStats>> codecWindowStats(codecs.size());
          for (int by = 0; by < blocksInHeight && processedBlocks < nBlocks;
               by++) {
            for (int bx = 0; bx < blocksInWidth && processedBlocks < nBlocks;
                 bx++) {
              // Check if the current block index matches the spacing criteria
              if ((by * blocksInWidth + bx) % blockSpacing != 0) continue;

              int xOff = bx * blockSize;
              int yOff = by * blockSize;

              std::vector<int32_t> blockData = readGeoTiffBlock(
                  band, xOff, yOff, blockSize, rasterWidth, rasterHeight);
              if (blockData.size() != blockSize * blockSize) {
                continue;
              }

              // Normalise window to all positive
              if (min < 0) {
                for (int i = 0; i < blockData.size(); i++) {
                  blockData[i] += (-min);
                }
              }

              remapAndTransformData(blockData, ordering, transformation,
                                    blockSize);

              std::vector<CodecStats> allCodecStats =
                  benchmarkWindow(blockData, codecs);
              for (int ci = 0; ci < codecs.size(); ci++) {
                codecWindowStats[ci].push_back(allCodecStats[ci]);
              }

              processedBlocks++;  // Increment the count of processed blocks
            }
          }

          for (int ci = 0; ci < codecs.size(); ci++) {
            std::cout << "c:" << ci;

            std::vector<CodecStats>& codecStatsAcrossWindows =
                codecWindowStats[ci];
            std::vector<float> cfs;
            std::vector<float> bpis;
            std::vector<float> tencs;
            std::vector<float> tdecs;
            for (CodecStats& stats : codecStatsAcrossWindows) {
              cfs.push_back(stats.cf);
              bpis.push_back(stats.bpi);
              tencs.push_back(stats.tenc);
              tdecs.push_back(stats.tdec);
            }
            float cfmean = Mean(cfs);
            float cfvar = Variance(cfs, cfmean);
            float bpimean = Mean(bpis);
            float bpivar = Variance(bpis, bpimean);
            float tencmean = Mean(tencs);
            float tencvar = Variance(tencs, tencmean);
            float tdecmean = Mean(tdecs);
            float tdecvar = Variance(tdecs, tdecmean);
            std::cout << ",cfmean:" << cfmean << ",cfvar:" << cfvar
                      << ",bpimean:" << bpimean << ",bpivar:" << bpivar
                      << ",tencmean:" << tencmean << ",tencvar:" << tencvar
                      << ",tdecmean:" << tdecmean << ",tdecvar:" << tdecvar
                      << std::endl;
          }
        } catch (const std::exception& e) {
          std::cout << " ERROR see cerr " << std::endl;
          std::cerr << "Error for bench search: " << e.what() << std::endl;
          continue;
        }

        /* END SEARCH */
      }

      /* END SEARCH */
    }

    /* END SEARCH */
  }

  GDALClose(dataset);
  return 0;
}