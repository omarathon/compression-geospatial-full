#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#include <algorithm>
#include <cmath>
#include <fstream>
#include <iostream>
#include <random>
#include <string>

#include "codec_collection.h"
#include "codecs/direct_codec.h"
#include "gdal_priv.h"
#include "remappings.h"
#include "transformations.h"
#include "util.h"

#define GDAL_DTYPE GDT_Int32

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

// 'linearXOR' | 'linearSum' | 'randomXOR' | 'randomSum'
std::size_t applyAccessTransformation(std::vector<int32_t>& blockData,
                                      const std::string& transformation,
                                      std::size_t blockSize) {
  auto startRead = std::chrono::high_resolution_clock::now();
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
  } else if (transformation == "linearSum") {
    volatile int64_t dummy = 0;  // Ensures reads aren't optimised away.
    for (int bi = 0; bi < blockSize * blockSize; bi++) {
      dummy += blockData[bi];
    }
  } else if (transformation == "randomXOR") {
    volatile int32_t dummy = 0;  // Ensures reads aren't optimised away.
    std::vector<int> bis(blockSize * blockSize, 0);
    for (int iti = 0; iti < blockSize * blockSize; iti++) {
      int bi = rand() % (blockSize * blockSize);
      bis[iti] = bi;
    }
    startRead = std::chrono::high_resolution_clock::now();
    for (int iti = 0; iti < blockSize * blockSize; iti++) {
      int bi = bis[iti];
      dummy ^= blockData[bi];
    }
  } else if (transformation == "randomSum") {
    volatile int64_t dummy = 0;  // Ensures reads aren't optimised away.
    std::vector<int> bis(blockSize * blockSize, 0);
    for (int iti = 0; iti < blockSize * blockSize; iti++) {
      int bi = rand() % (blockSize * blockSize);
      bis[iti] = bi;
    }
    startRead = std::chrono::high_resolution_clock::now();
    for (int iti = 0; iti < blockSize * blockSize; iti++) {
      int bi = bis[iti];
      dummy += blockData[bi];
    }
  } else {                       // includes linearXOR
    volatile int32_t dummy = 0;  // Ensures reads aren't optimised away.
    for (int bi = 0; bi < blockSize * blockSize; bi++) {
      dummy ^= blockData[bi];
    }
  }
  auto endRead = std::chrono::high_resolution_clock::now();
  return std::chrono::duration_cast<std::chrono::nanoseconds>(endRead -
                                                              startRead)
      .count();
}

/*
usage
std::vector<std::unique_ptr<StatefulIntegerCodec<int32_t>>> codecGrid =
splitIntoFullBlocks( band, nXSize, nYSize, blockSize, numBlocks,
std::move(experimentBaseCodec), min, transformation, ordering)
*/
std::vector<std::unique_ptr<StatefulIntegerCodec<int32_t>>> splitIntoFullBlocks(
    GDALRasterBand* band, int rasterWidth, int rasterHeight, int blockSize,
    int numBlocks, std::unique_ptr<StatefulIntegerCodec<int32_t>> baseCodec,
    const int32_t min, const std::string transformation,
    const std::string ordering) {
  std::vector<std::unique_ptr<StatefulIntegerCodec<int32_t>>> codecs(numBlocks);

  // Calculate full blocks that fit in the raster dimensions
  int blocksInWidth = rasterWidth / blockSize;
  int blocksInHeight = rasterHeight / blockSize;
  int totalFullBlocks = blocksInWidth * blocksInHeight;

  // Calculate interval to sample blocks evenly
  int sampleInterval = std::max(1, totalFullBlocks / numBlocks);

  for (int blockNum = 0, sampledBlocks = 0; sampledBlocks < numBlocks;
       blockNum += sampleInterval, sampledBlocks++) {
    int blockIndex = blockNum % totalFullBlocks;
    int bx = (blockIndex % blocksInWidth) * blockSize;
    int by = (blockIndex / blocksInWidth) * blockSize;

    // Fetch and encode block data
    std::vector<int32_t> blockData(blockSize * blockSize);
    CPLErr err =
        band->RasterIO(GF_Read, bx, by, blockSize, blockSize, blockData.data(),
                       blockSize, blockSize, GDT_Int32, 0, 0);
    if (err != CE_None) {
      throw std::runtime_error("Error reading raster block data");
    }

    // Normalise block data
    if (min < 0) {
      for (int i = 0; i < blockData.size(); i++) {
        blockData[i] += (-min);
      }
    }

    remapAndTransformData(blockData, ordering, transformation, blockSize);

    // Compress block data and store
    std::unique_ptr<StatefulIntegerCodec<int32_t>> clonedCodec(
        baseCodec->CloneFresh());
    clonedCodec->AllocEncoded(blockData.data(), blockData.size());
    clonedCodec->EncodeArray(blockData.data(), blockData.size());
    codecs[sampledBlocks] = std::move(clonedCodec);
  }

  return codecs;
}

// access transformation {default: 'linearXOR' | 'linearSum' | 'randomXOR' |
// 'randomSum' | 'Threshold' | 'SmoothAndShift' | 'IndexBasedClassification' |
// 'ValueBasedClassification' | 'ValueShift'}
void benchmarkAccess(
    std::vector<std::unique_ptr<StatefulIntegerCodec<int32_t>>>& codecs,
    std::unique_ptr<StatefulIntegerCodec<int32_t>> accessCodec, int blockSize,
    const std::string& sampleAccessPattern,
    const std::string& accessTransformation, std::vector<std::size_t>& timesDec,
    std::vector<std::size_t>& timesAccessTransformation,
    std::vector<std::size_t>& timesEnc) {
  srand(1);

  bool isDirectAccess = (codecs[0]->name() == "custom_direct_access");
  bool isDirectReenc = (accessCodec->name() == "custom_direct_access");

  std::vector<int32_t> decbuf;
  decbuf.resize(blockSize * blockSize +
                codecs[0]->GetOverflowSize(blockSize * blockSize));

  bool dataChange = accessTransformation == "Threshold" ||
                    accessTransformation == "SmoothAndShift" ||
                    accessTransformation == "IndexBasedClassification" ||
                    accessTransformation == "ValueBasedClassification" ||
                    accessTransformation == "ValueShift";

  // access indexes
  std::vector<std::size_t> accessIndexes;
  for (int i = 0; i < codecs.size(); i++) {
    accessIndexes.push_back(i);
  }
  if (sampleAccessPattern != "linear") {
    // random
    std::default_random_engine engine(1);
    std::shuffle(accessIndexes.begin(), accessIndexes.end(), engine);
  }

  for (int i = 0; i < codecs.size(); i++) {
    int blockIndex = accessIndexes[i];
    auto& codec = codecs[blockIndex];

    auto benchblock = [&](std::vector<int32_t>& decbuf) {
      /* decode */
      std::size_t decodeTime = 0;
      if (!isDirectAccess) {
        auto startDecode = std::chrono::high_resolution_clock::now();
        codec->DecodeArray(decbuf.data(), blockSize * blockSize);
        auto endDecode = std::chrono::high_resolution_clock::now();
        decodeTime = std::chrono::duration_cast<std::chrono::nanoseconds>(
                         endDecode - startDecode)
                         .count();
      }

      timesDec.push_back(decodeTime);

      /* perform transformation */
      std::size_t accessTransformationTime =
          applyAccessTransformation(decbuf, accessTransformation, blockSize);
      timesAccessTransformation.push_back(accessTransformationTime);

      if (dataChange) {
        /* re-encode with new codec */
        std::unique_ptr<StatefulIntegerCodec<int32_t>> clonedAccessCodec(
            accessCodec->CloneFresh());
        std::unique_ptr<StatefulIntegerCodec<int32_t>>& reencCodec =
            clonedAccessCodec;

        if (isDirectReenc) {
          timesEnc.push_back(0);
          reencCodec->AllocEncoded(decbuf.data(), blockSize * blockSize);
          reencCodec->EncodeArray(decbuf.data(), blockSize * blockSize);
        } else {
          reencCodec->AllocEncoded(decbuf.data(), blockSize * blockSize);
          auto startEncode = std::chrono::high_resolution_clock::now();
          reencCodec->EncodeArray(decbuf.data(), blockSize * blockSize);
          auto endEncode = std::chrono::high_resolution_clock::now();
          auto encodeTime =
              std::chrono::duration_cast<std::chrono::nanoseconds>(endEncode -
                                                                   startEncode)
                  .count();
          timesEnc.push_back(encodeTime);
        }

        codecs[blockIndex] = std::move(clonedAccessCodec);
      }
    };

    if (!isDirectAccess) {
      benchblock(decbuf);
    } else {
      // special case for direct access codec
      benchblock(codec->GetEncoded());
    }
  }
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

int main(int argc, char* argv[]) {
  if (argc < 11) {
    std::cout
        << "Usage: " << argv[0]
        << " <file path> <block size> <num blocks> <num reps> <initial codec "
           "names | 'all'>[] <access codec names | 'all'>[] <block ordering "
           "{default: row-major | 'zigzag' | 'morton'}>[] <initial "
           "transformation {default: none | 'Threshold' | 'SmoothAndShift' | "
           "'IndexBasedClassification' | 'ValueBasedClassification' | "
           "'ValueShift'}>[] <sample access pattern {default: 'random' | "
           "'linear'}>[] <access transformation {default: 'linearXOR' | "
           "'linearSum' | 'randomXOR' | 'randomSum' | 'Threshold' | "
           "'SmoothAndShift' | 'IndexBasedClassification' | "
           "'ValueBasedClassification' | 'ValueShift'}>[]";
    return 1;
  }

  const char* filePath = argv[1];
  int blockSize = atoi(argv[2]);
  int numBlocks = atoi(argv[3]);
  int numReps = atoi(argv[4]);
  std::vector<std::string> initialCodecNames =
      ParseBarDelimited(std::string(argv[5]));
  std::vector<std::string> accessCodecNames =
      ParseBarDelimited(std::string(argv[6]));
  std::vector<std::string> orderings =
      ParseCommaDelimited(std::string(argv[7]));
  std::vector<std::string> initialTransformations =
      ParseCommaDelimited(std::string(argv[8]));
  std::vector<std::string> sampleAccessPatterns =
      ParseCommaDelimited(std::string(argv[9]));
  std::vector<std::string> accessTransformations =
      ParseCommaDelimited(std::string(argv[10]));

  srand(1);  // Seed the random number generator

  GDALAllRegister();

  GDALDataset* dataset = (GDALDataset*)GDALOpen(filePath, GA_ReadOnly);
  if (dataset == NULL) {
    std::cout << "Failed to open file.\n";
    return 1;
  }

  GDALRasterBand* band = dataset->GetRasterBand(1);
  int nXSize = band->GetXSize();
  int nYSize = band->GetYSize();

  int blocksInWidth = nXSize / blockSize;
  int blocksInHeight = nYSize / blockSize;

  /* DONE PREPROCESSING */

  for (auto& ordering : orderings) {
    for (auto& initialTransformation : initialTransformations) {
      for (auto& accessTransformation : accessTransformations) {
        int32_t min = std::numeric_limits<int32_t>::max();

        int totalFullBlocks = blocksInWidth * blocksInHeight;
        int sampleInterval = std::max(1, totalFullBlocks / numBlocks);

        for (int blockNum = 0, sampledBlocks = 0; sampledBlocks < numBlocks;
             blockNum += sampleInterval, sampledBlocks++) {
          int blockIndex = blockNum % totalFullBlocks;
          int xOff = (blockIndex % blocksInWidth) * blockSize;
          int yOff = (blockIndex / blocksInWidth) * blockSize;
          computeMinForBlock(band, xOff, yOff, blockSize, min);
        }

        // Build all codecs with cascades for delta, rle, and for.
        auto buildAllCodecs = [&]() {
          auto base_codecs = InitCodecs(/* nonCascaded */ true, nullptr);

          for (auto& cascade :
               InitCodecs(/* nonCascaded */ false,
                           std::make_unique<DeltaCodecAVX512>())) {
            base_codecs.push_back(std::move(cascade));
          }
          for (auto& cascade :
               InitCodecs(/* nonCascaded */ false,
                           std::make_unique<RLECodecAVX512>())) {
            base_codecs.push_back(std::move(cascade));
          }
          for (auto& cascade :
               InitCodecs(/* nonCascaded */ false,
                           std::make_unique<FORCodecAVX512>())) {
            base_codecs.push_back(std::move(cascade));
          }

          base_codecs.push_back(std::make_unique<DirectAccessCodec>());
          return base_codecs;
        };

        std::vector<std::unique_ptr<StatefulIntegerCodec<int32_t>>>
            allCodecs_initial = buildAllCodecs();
        std::vector<std::unique_ptr<StatefulIntegerCodec<int32_t>>>
            allCodecs_access = buildAllCodecs();

        // Select the codecs with the specified names
        std::vector<std::unique_ptr<StatefulIntegerCodec<int32_t>>> baseCodecs;
        std::vector<std::unique_ptr<StatefulIntegerCodec<int32_t>>>
            accessCodecs;
        for (auto& codec : allCodecs_initial) {
          if (initialCodecNames.size() == 1 &&
              (initialCodecNames[0] == "all" || initialCodecNames[0] == "*")) {
            std::unique_ptr<StatefulIntegerCodec<int32_t>> newCodec(
                codec->CloneFresh());
            baseCodecs.push_back(std::move(newCodec));
            continue;
          }
          for (auto& codecName : initialCodecNames) {
            if (codecName == codec->name()) {
              std::unique_ptr<StatefulIntegerCodec<int32_t>> newCodec(
                  codec->CloneFresh());
              baseCodecs.push_back(std::move(newCodec));
              break;
            }
          }
        }
        for (auto& codec : allCodecs_access) {
          if (accessCodecNames.size() == 1 &&
              (accessCodecNames[0] == "all" || accessCodecNames[0] == "*")) {
            std::unique_ptr<StatefulIntegerCodec<int32_t>> newCodec(
                codec->CloneFresh());
            accessCodecs.push_back(std::move(newCodec));
            continue;
          }
          for (auto& codecName : accessCodecNames) {
            if (codecName == codec->name()) {
              std::unique_ptr<StatefulIntegerCodec<int32_t>> newCodec(
                  codec->CloneFresh());
              accessCodecs.push_back(std::move(newCodec));
              break;
            }
          }
        }

        allCodecs_initial.clear();
        allCodecs_initial.shrink_to_fit();
        allCodecs_access.clear();
        allCodecs_access.shrink_to_fit();

        for (auto& baseCodec : baseCodecs) {
          for (auto& accessCodec : accessCodecs) {
            assert(baseCodec);
            assert(accessCodec);
            std::string baseCodecName = baseCodec->name();
            std::string accessCodecName = accessCodec->name();

            for (auto& sampleAccessPattern : sampleAccessPatterns) {
              std::cout << "**BENCHMARK ACCESS**" << std::endl;
              std::cout << "file=" << filePath << ",blocksize=" << blockSize
                        << ",numblocks=" << numBlocks << ",numreps=" << numReps
                        << ",basecodec=" << baseCodecName
                        << ",accesscodec=" << accessCodecName
                        << ",ordering=" << ordering
                        << ",initialtransformation=" << initialTransformation
                        << ",sampleaccesspattern=" << sampleAccessPattern
                        << ",accesstransformation=" << accessTransformation
                        << std::endl;

              std::vector<std::size_t> timesDec;
              std::vector<std::size_t> timesAccessTransformation;
              std::vector<std::size_t> timesEnc;

              for (int rep = 0; rep < numReps; rep++) {
                std::unique_ptr<StatefulIntegerCodec<int32_t>>
                    experimentBaseCodec(baseCodec->CloneFresh());
                std::unique_ptr<StatefulIntegerCodec<int32_t>>
                    experimentAccessCodec(accessCodec->CloneFresh());

                std::vector<std::unique_ptr<StatefulIntegerCodec<int32_t>>>
                    codecGrid = splitIntoFullBlocks(
                        band, nXSize, nYSize, blockSize, numBlocks,
                        std::move(experimentBaseCodec), min,
                        initialTransformation, ordering);

                if (codecGrid.size() == 0) {
                  std::cerr << "NO CODECS FORMING GRID." << std::endl;
                  GDALClose(dataset);
                  return 1;
                }

                // Perform accesses - part to be measured
                benchmarkAccess(codecGrid, std::move(experimentAccessCodec),
                                blockSize, sampleAccessPattern,
                                accessTransformation, timesDec,
                                timesAccessTransformation, timesEnc);
              }

              std::cout << "tottimedec:" << Sum(timesDec);
              std::cout << ",meantimedec:" << Mean(timesDec);
              std::cout << ",vartimedec:" << Variance(timesDec, Mean(timesDec));

              std::cout << ",tottimetrans:" << Sum(timesAccessTransformation);
              std::cout << ",meantimetrans:" << Mean(timesAccessTransformation);
              std::cout << ",vartimetrans:"
                        << Variance(timesAccessTransformation,
                                    Mean(timesAccessTransformation));

              std::cout << ",tottimeenc:" << Sum(timesEnc);
              std::cout << ",meantimeenc:" << Mean(timesEnc);
              std::cout << ",vartimeenc:" << Variance(timesEnc, Mean(timesEnc))
                        << std::endl;
            }
          }
        }
      }
    }
  }

  GDALClose(dataset);
  return 0;
}