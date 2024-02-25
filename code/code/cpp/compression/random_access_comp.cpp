#include "gdal_priv.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "codec_collection.h"
#include <unistd.h>
#include <signal.h>
#include <unordered_map>
#include <cmath>
#include <algorithm>
#include <any>
#include <variant>
#include <fstream>
#include <iostream>
#include <string>
#include <sys/wait.h>
#include "remappings.h"
#include "util.h"
#include "transformations.h"

#define GDAL_DTYPE GDT_Int32

void remapAndTransformData(std::vector<int32_t>& blockData, const std::string& ordering, const std::string& transformation, const int blockSize) {
    // blockData is currently in row-major order. remap according to desired ordering
    if (ordering == "zigzag") {
        auto remappedBlockData = remapToZigzagOrder(blockData, blockSize);
        std::copy(remappedBlockData.begin(), remappedBlockData.end(), blockData.begin());
    }
    else if (ordering == "morton") {
        auto remappedBlockData = remapToMortonOrder(blockData, blockSize);
        std::copy(remappedBlockData.begin(), remappedBlockData.end(), blockData.begin());
    }

    // apply transformation if desired
    if (transformation == "threshold") {
        threshold(blockData, /* threshold_value */ avg(blockData));
    }
    else if (transformation == "smoothAndShift") {
        smoothAndShift(blockData);
    }
    else if (transformation == "indexBasedClassification") {
        indexBasedClassification(blockData, /* max_classes */ 8);
    }
    else if (transformation == "valueBasedClassification") {
        valueBasedClassification(blockData, /* num_classes */ 8);
    }
    else if (transformation == "valueShift") {
        valueShift(blockData, /* delta */ pow(2,23));
    }
}


/*
usage
std::vector<std::unique_ptr<StatefulIntegerCodec<int32_t>>> codecGrid = splitIntoFullBlocks(
    band, nXSize, nYSize, blockSize, numBlocks, std::move(experimentBaseCodec), min, transformation, ordering)
*/
std::vector<std::unique_ptr<StatefulIntegerCodec<int32_t>>> splitIntoFullBlocks(
    GDALRasterBand* band, int rasterWidth, int rasterHeight, int blockSize, int numBlocks,
    std::unique_ptr<StatefulIntegerCodec<int32_t>> baseCodec, const int32_t min, const std::string transformation, const std::string ordering) {
    
    std::vector<std::unique_ptr<StatefulIntegerCodec<int32_t>>> codecs(numBlocks);

    // Calculate full blocks that fit in the raster dimensions
    int blocksInWidth = rasterWidth / blockSize;
    int blocksInHeight = rasterHeight / blockSize;
    int totalFullBlocks = blocksInWidth * blocksInHeight;

    if (numBlocks > totalFullBlocks) {
        std::cerr << "Requested number of blocks exceeds the total number of full blocks available." << std::endl;
        return codecs; // Return empty list if requested more blocks than available
    }

    // Calculate interval to sample blocks evenly
    int sampleInterval = std::max(1, totalFullBlocks / numBlocks);

    for (int blockNum = 0, sampledBlocks = 0; sampledBlocks < numBlocks && blockNum < totalFullBlocks; blockNum += sampleInterval, sampledBlocks++) {
        int blockIndex = blockNum;
        int bx = (blockIndex % blocksInWidth) * blockSize;
        int by = (blockIndex / blocksInWidth) * blockSize;

        // Fetch and encode block data
        std::vector<int32_t> blockData(blockSize * blockSize);
        CPLErr err = band->RasterIO(GF_Read, bx, by, blockSize, blockSize, blockData.data(), blockSize, blockSize, GDT_Int32, 0, 0);
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
        std::unique_ptr<StatefulIntegerCodec<int32_t>> clonedCodec(baseCodec->cloneFresh());
        clonedCodec->allocEncoded(blockData.data(), blockData.size());
        clonedCodec->encodeArray(blockData.data(), blockData.size());
        codecs[sampledBlocks] = std::move(clonedCodec);
    }

    return codecs;
}

// operator: default: 'dec->access' | 'dec->access->enc'
void benchmarkAccess(const std::vector<std::unique_ptr<StatefulIntegerCodec<int32_t>>>& codecs, int blockSize, int numAccesses, const std::string& sampleAccessPattern, const std::string& readAccessPattern, const std::string& operatorStr) {
    srand(1);  // Seed the random number generator with 1

    std::vector<int32_t> decbuf;
    decbuf.resize(blockSize*blockSize + codecs[0]->getOverflowSize(blockSize*blockSize));
    
    // int32_t* decbuf = (int32_t*)malloc((blockSize*blockSize + codecs[0]->getOverflowSize(blockSize*blockSize)) * sizeof(int32_t));

    std::size_t totalNanos = 0;
    std::vector<std::size_t> times;

    bool reEnc = (operatorStr == "dec->access->enc");

    for (int i = 0; i < numAccesses; i++) {
        int blockIndex = rand() % (codecs.size()); // Ignore the last block as it's a different size
        if (sampleAccessPattern == "linear") {
            blockIndex = i % codecs.size();
        }

        auto& codec = codecs[blockIndex];

        /* decode */

        auto startDecode = std::chrono::high_resolution_clock::now();
        codec->decodeArray(decbuf.data(), blockSize*blockSize);
        auto endDecode = std::chrono::high_resolution_clock::now();
        auto decodeTime = std::chrono::duration_cast<std::chrono::nanoseconds>(endDecode - startDecode).count();

        /* perform read */

        std::size_t readTime = 0;
        if (readAccessPattern == "linear") {
            volatile int32_t dummy = 0; // Ensures reads aren't optimised away.
            auto startRead = std::chrono::high_resolution_clock::now();
            for (int bi = 0; bi < blockSize*blockSize; bi++) {
                dummy ^= decbuf[bi];
            }
            auto endRead = std::chrono::high_resolution_clock::now();
            readTime = std::chrono::duration_cast<std::chrono::nanoseconds>(endRead - startRead).count();
        }
        else if (readAccessPattern == "random") {
            volatile int32_t dummy = 0; // Ensures reads aren't optimised away.
            auto startRead = std::chrono::high_resolution_clock::now();
            for (int iti = 0; iti < blockSize*blockSize; iti++) {
                int bi = rand() % (blockSize*blockSize);
                dummy ^= decbuf[bi];
            }
            auto endRead = std::chrono::high_resolution_clock::now();
            readTime = std::chrono::duration_cast<std::chrono::nanoseconds>(endRead - startRead).count();
        }

        /* re-encode*/

        if (reEnc) {
            auto startEncode= std::chrono::high_resolution_clock::now();
            codec->allocEncoded(decbuf.data(), blockSize * blockSize);
            codec->encodeArray(decbuf.data(), blockSize*blockSize);
            auto endEncode = std::chrono::high_resolution_clock::now();
            auto encodeTime = std::chrono::duration_cast<std::chrono::nanoseconds>(endEncode - startEncode).count();

            totalNanos += decodeTime + encodeTime + readTime;
            times.push_back(decodeTime + encodeTime + readTime);
        }
        else {
            totalNanos += decodeTime + readTime;
            times.push_back(decodeTime + readTime);
        }
    }

    double meanTime = (double)totalNanos / numAccesses;
    double variance = 0.0;
    for (auto time : times) {
        variance += (time - meanTime) * (time - meanTime);
    }
    variance /= numAccesses;

    std::cout << "tottime:" << totalNanos;
    std::cout << ",meantime:" << meanTime;
    std::cout << ",vartime:" << variance << std::endl;
}

// Function to process a single block for min and unique values
void computeMinAndUniqueValuesForBlock(GDALRasterBand* band, std::string& ordering, std::string& transformation, int xOff, int yOff, int blockSize, int32_t& min, std::unordered_set<int32_t>& unique_values_set) {
    std::vector<int32_t> blockData(blockSize * blockSize);
    band->RasterIO(GF_Read, xOff, yOff, blockSize, blockSize, blockData.data(), blockSize, blockSize, GDT_Int32, 0, 0);

    for (auto& value : blockData) {
        min = std::min(min, value);
    }

    if (min < 0) {
        for (int i = 0; i < blockData.size(); i++) {
            blockData[i] += (-min);
        }
    }

    remapAndTransformData(blockData, ordering, transformation, blockSize);

    for (auto& value : blockData) {
        unique_values_set.insert(value);
    }
}

int main(int argc, char* argv[]) {
    if (argc < 11) {
        std::cout << "Usage: " << argv[0] << " <file path> <block size> <num blocks> <num block samples> <codec names | 'all'>[] <block ordering {default: row-major | 'zigzag' | 'morton'}>[] <transformation {default: none | 'threshold' | 'smoothAndShift' | 'indexBasedClassification' | 'valueBasedClassification' | 'valueShift'}>[] <sample access pattern {default: 'random' | 'linear'}>[] <read access pattern {default: 'none' | 'linear' | 'random'}>[] <operator {default: 'dec->access' | 'dec->access->enc'}>[]";
        return 1;
    }

    const char* filePath = argv[1];
    int blockSize = atoi(argv[2]);
    int numBlocks = atoi(argv[3]);
    int numBlockSamples = atoi(argv[4]);
    std::vector<std::string> codecNames = parseCommaDelimited(std::string(argv[5]));
    // std::vector<std::string> reencCodecNames = parseCommaDelimited(std::string(argv[6]));
    std::vector<std::string> orderings = parseCommaDelimited(std::string(argv[6]));
    std::vector<std::string> transformations = parseCommaDelimited(std::string(argv[7]));
    std::vector<std::string> sampleAccessPatterns = parseCommaDelimited(std::string(argv[8]));
    std::vector<std::string> readAccessPatterns = parseCommaDelimited(std::string(argv[9]));
    std::vector<std::string> operatorStrings = parseCommaDelimited(std::string(argv[10]));

    srand(1); // Seed the random number generator

    GDALAllRegister();

    GDALDataset* dataset = (GDALDataset*) GDALOpen(filePath, GA_ReadOnly);
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
        for (auto& transformation : transformations) {

            int32_t min = std::numeric_limits<int32_t>::max();
            std::unordered_set<int32_t> unique_values_set;

            // Process each block to compute min and unique values
            for (int y = 0; y < blocksInHeight; ++y) {
                for (int x = 0; x < blocksInWidth; ++x) {
                    int xOff = x * blockSize;
                    int yOff = y * blockSize;
                    computeMinAndUniqueValuesForBlock(band, ordering, transformation, xOff, yOff, blockSize, min, unique_values_set);
                }
            }

            // Convert unordered_set to vector for unique values
            std::vector<int32_t> unique_values(unique_values_set.begin(), unique_values_set.end());

            std::any dict;
            std::vector<int32_t> reverseDict;

            auto createAndAddCodecs = [&](auto dummy, const std::vector<int32_t>& unique_values) {
                using dict_type = decltype(dummy);
                std::unordered_map<int32_t, dict_type> localDict;
                reverseDict.resize(unique_values.size());

                dict_type index = 0;
                for (int32_t value : unique_values) {
                    localDict[value] = index;
                    reverseDict[index] = value;
                    ++index;
                }

                dict = std::move(localDict); // Store the dictionary in std::any

                auto base_codecs = initCodecs(std::any_cast<std::unordered_map<int32_t, dict_type>&>(dict), reverseDict, 
                                                /* nonCascaded */ true, /* cascadeCodec */ nullptr);

                auto cascadeDictCodec = std::make_unique<DictCodecAVX2<dict_type>>(std::any_cast<std::unordered_map<int32_t, dict_type>&>(dict), reverseDict);
                auto cascadeDeltaCodec = std::make_unique<DeltaCodecAVX512>();
                auto cascadeRleCodec = std::make_unique<RLECodecAVX512>();
                auto cascadeForCodec = std::make_unique<FORCodecAVX512>();

                for (auto& cascade : initCodecs(std::any_cast<std::unordered_map<int32_t, dict_type>&>(dict), reverseDict, 
                                                /* nonCascaded */ false, /* cascadeCodec */ std::move(cascadeDictCodec))) {
                    base_codecs.push_back(std::move(cascade));
                }
                for (auto& cascade : initCodecs(std::any_cast<std::unordered_map<int32_t, dict_type>&>(dict), reverseDict, 
                                                /* nonCascaded */ false, /* cascadeCodec */ std::move(cascadeDeltaCodec))) {
                    base_codecs.push_back(std::move(cascade));
                }
                for (auto& cascade : initCodecs(std::any_cast<std::unordered_map<int32_t, dict_type>&>(dict), reverseDict, 
                                                /* nonCascaded */ false, /* cascadeCodec */ std::move(cascadeRleCodec))) {
                    base_codecs.push_back(std::move(cascade));
                }
                for (auto& cascade : initCodecs(std::any_cast<std::unordered_map<int32_t, dict_type>&>(dict), reverseDict, 
                                                /* nonCascaded */ false, /* cascadeCodec */ std::move(cascadeForCodec))) {
                    base_codecs.push_back(std::move(cascade));
                }

                return base_codecs;
            };

            std::cout << "num_unique_values = " << unique_values.size() << std::endl;

            std::vector<std::unique_ptr<StatefulIntegerCodec<int32_t>>> allCodecs;
            allCodecs = createAndAddCodecs(uint32_t{}, unique_values);

            // Select the codecs with the specified names
            std::vector<std::unique_ptr<StatefulIntegerCodec<int32_t>>> baseCodecs;
            for (auto& codec : allCodecs) {
                if (codecNames.size() == 1 && (codecNames[0] == "all" || codecNames[0] == "*")) {
                    std::unique_ptr<StatefulIntegerCodec<int32_t>> newCodec(codec->cloneFresh());
                    baseCodecs.push_back(std::move(newCodec));
                    continue;
                }
                for (auto& codecName : codecNames) {
                    if (codecName == codec->name()) {
                        std::unique_ptr<StatefulIntegerCodec<int32_t>> newCodec(codec->cloneFresh());
                        baseCodecs.push_back(std::move(newCodec));
                        break;
                    }
                }
            }

            allCodecs.clear();
            allCodecs.shrink_to_fit();

            for (auto& baseCodec : baseCodecs) {
                assert(baseCodec);
                std::string baseCodecName = baseCodec->name();
                
                for (auto& sampleAccessPattern : sampleAccessPatterns) {
                    for (auto& readAccessPattern : readAccessPatterns) {
                        for (auto& operatorStr : operatorStrings) {

                            std::cout << "**BENCHMARK ACCESS**" << std::endl;
                            std::cout << "file=" << filePath << ",blocksize=" << blockSize << ",numblocks=" << numBlocks << ",numblocksamples=" << numBlockSamples << ",basecodec=" << baseCodecName << ",ordering=" << ordering << ",transformation=" << transformation << ",sampleaccesspattern=" << sampleAccessPattern << ",readaccesspattern=" << readAccessPattern << ",operator=" << operatorStr << std::endl;

                            std::unique_ptr<StatefulIntegerCodec<int32_t>> experimentBaseCodec(baseCodec->cloneFresh());

                            std::vector<std::unique_ptr<StatefulIntegerCodec<int32_t>>> codecGrid = splitIntoFullBlocks(
                                band, nXSize, nYSize, blockSize, numBlocks, std::move(experimentBaseCodec), min, transformation, ordering);

                            if (codecGrid.size() == 0) {
                                std::cerr << "NO CODECS FORMING GRID." << std::endl;
                                GDALClose(dataset);
                                return 1;
                            }
                                
                            // Perform random accesses - part to be measured
                            // to do: pass readAccessPattern and operatorStr
                            benchmarkAccess(codecGrid, blockSize, numBlockSamples, sampleAccessPattern, readAccessPattern, operatorStr);
                        }
                    }
                }
            }
        }
    }

    GDALClose(dataset);
    return 0;
}