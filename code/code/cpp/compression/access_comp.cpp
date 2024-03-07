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
#include <random>
#include <any>
#include <variant>
#include <fstream>
#include <iostream>
#include <string>
#include <sys/wait.h>
#include "remappings.h"
#include "util.h"
#include "transformations.h"
#include "codecs/direct_codec.h"

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

// 'linearXOR' | 'linearSum' | 'randomXOR' | 'randomSum'
std::size_t applyAccessTransformation(std::vector<int32_t>& blockData, const std::string& transformation, std::size_t blockSize) {
    auto startRead = std::chrono::high_resolution_clock::now();
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
    else if (transformation == "linearSum") {
        volatile int32_t dummy = 0; // Ensures reads aren't optimised away.
        for (int bi = 0; bi < blockSize*blockSize; bi++) {
            dummy += blockData[bi];
        }
    }
    else if (transformation == "randomXOR") {
        volatile int32_t dummy = 0; // Ensures reads aren't optimised away.
        std::vector<int> bis(blockSize*blockSize,0);
        for (int iti = 0; iti < blockSize*blockSize; iti++) {
            int bi = rand() % (blockSize*blockSize);
            bis[iti] = bi;
        }
        startRead = std::chrono::high_resolution_clock::now();
        for (int iti = 0; iti < blockSize*blockSize; iti++) {
            int bi = bis[iti];
            dummy ^= blockData[bi];
        }
    }
    else if (transformation == "randomSum") {
        volatile int32_t dummy = 0; // Ensures reads aren't optimised away.
        std::vector<int> bis(blockSize*blockSize,0);
        for (int iti = 0; iti < blockSize*blockSize; iti++) {
            int bi = rand() % (blockSize*blockSize);
            bis[iti] = bi;
        }
        startRead = std::chrono::high_resolution_clock::now();
        for (int iti = 0; iti < blockSize*blockSize; iti++) {
            int bi = bis[iti];
            dummy += blockData[bi];
        }
    }
    else { // includes linearXOR
        volatile int32_t dummy = 0; // Ensures reads aren't optimised away.
        for (int bi = 0; bi < blockSize*blockSize; bi++) {
            dummy ^= blockData[bi];
        }
    }
    auto endRead = std::chrono::high_resolution_clock::now();
    return std::chrono::duration_cast<std::chrono::nanoseconds>(endRead - startRead).count();
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

    // Calculate interval to sample blocks evenly
    int sampleInterval = std::max(1, totalFullBlocks / numBlocks);

    for (int blockNum = 0, sampledBlocks = 0; sampledBlocks < numBlocks; blockNum += sampleInterval, sampledBlocks++) {
        int blockIndex = blockNum % totalFullBlocks;
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

// access transformation {default: 'linearXOR' | 'linearSum' | 'randomXOR' | 'randomSum' | 'threshold' | 'smoothAndShift' | 'indexBasedClassification' | 'valueBasedClassification' | 'valueShift'}
void benchmarkAccess(const std::vector<std::unique_ptr<StatefulIntegerCodec<int32_t>>>& codecs, int blockSize, const std::string& sampleAccessPattern, const std::string& accessTransformation, 
                     std::vector<std::size_t>& timesDec, std::vector<std::size_t>& timesAccessTransformation, std::vector<std::size_t>& timesEnc) {
    srand(1);  // Seed the random number generator with 1

    bool isDirectAccess = (codecs[0]->name() == "custom_direct_access");

    std::vector<int32_t> decbuf;
    decbuf.resize(blockSize*blockSize + codecs[0]->getOverflowSize(blockSize*blockSize));
    
    // int32_t* decbuf = (int32_t*)malloc((blockSize*blockSize + codecs[0]->getOverflowSize(blockSize*blockSize)) * sizeof(int32_t));

    bool dataChange = 
           accessTransformation == "threshold" 
        || accessTransformation == "smoothAndShift" 
        || accessTransformation == "indexBasedClassification" 
        || accessTransformation == "valueBasedClassification" 
        || accessTransformation == "valueShift";

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
                codec->decodeArray(decbuf.data(), blockSize*blockSize);
                auto endDecode = std::chrono::high_resolution_clock::now();
                decodeTime = std::chrono::duration_cast<std::chrono::nanoseconds>(endDecode - startDecode).count();
            }

            timesDec.push_back(decodeTime);

            /* perform transformation */
            std::size_t accessTransformationTime = applyAccessTransformation(decbuf, accessTransformation, blockSize);
            timesAccessTransformation.push_back(accessTransformationTime);

            /* re-encode if not direct access and the data didn't change */
            if (isDirectAccess || dataChange) {
                timesEnc.push_back(0);
            }
            else {
                auto startEncode= std::chrono::high_resolution_clock::now();
                codec->allocEncoded(decbuf.data(), blockSize * blockSize);
                codec->encodeArray(decbuf.data(), blockSize*blockSize);
                auto endEncode = std::chrono::high_resolution_clock::now();
                auto encodeTime = std::chrono::duration_cast<std::chrono::nanoseconds>(endEncode - startEncode).count();

                timesEnc.push_back(encodeTime);
            }
        };

        if (!isDirectAccess) {
            benchblock(decbuf);
        }
        else {
            // special case for direct access codec
            benchblock(codec->getEncoded());
        }
    }
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
    if (argc < 10) {
        std::cout << "Usage: " << argv[0] << " <file path> <block size> <num blocks> <num reps> <codec names | 'all'>[] <block ordering {default: row-major | 'zigzag' | 'morton'}>[] <initial transformation {default: none | 'threshold' | 'smoothAndShift' | 'indexBasedClassification' | 'valueBasedClassification' | 'valueShift'}>[] <sample access pattern {default: 'random' | 'linear'}>[] <access transformation {default: 'linearXOR' | 'linearSum' | 'randomXOR' | 'randomSum' | 'threshold' | 'smoothAndShift' | 'indexBasedClassification' | 'valueBasedClassification' | 'valueShift'}>[]";
        return 1;
    }

    const char* filePath = argv[1];
    int blockSize = atoi(argv[2]);
    int numBlocks = atoi(argv[3]);
    int numReps = atoi(argv[4]);
    std::vector<std::string> codecNames = parseCommaDelimited(std::string(argv[5]));
    std::vector<std::string> orderings = parseCommaDelimited(std::string(argv[6]));
    std::vector<std::string> initialTransformations = parseCommaDelimited(std::string(argv[7]));
    std::vector<std::string> sampleAccessPatterns = parseCommaDelimited(std::string(argv[8]));
    std::vector<std::string> accessTransformations = parseCommaDelimited(std::string(argv[9]));

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
        for (auto& initialTransformation : initialTransformations) {

            int32_t min = std::numeric_limits<int32_t>::max();
            std::unordered_set<int32_t> unique_values_set;

            // Process each block to compute min and unique values
            for (int y = 0; y < blocksInHeight; ++y) {
                for (int x = 0; x < blocksInWidth; ++x) {
                    int xOff = x * blockSize;
                    int yOff = y * blockSize;
                    computeMinAndUniqueValuesForBlock(band, ordering, initialTransformation, xOff, yOff, blockSize, min, unique_values_set);
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

                auto directAccessCodec = std::make_unique<DirectAccessCodec>();
                base_codecs.push_back(std::move(directAccessCodec));

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
                    for (auto& accessTransformation : accessTransformations) {
                        if (accessTransformation == initialTransformation) {
                            std::cout << "Skipping double transformation." << std::endl;
                            continue;
                        }

                        std::cout << "**BENCHMARK ACCESS**" << std::endl;
                        std::cout << "file=" << filePath << ",blocksize=" << blockSize << ",numblocks=" << numBlocks << ",numreps=" << numReps << ",basecodec=" << baseCodecName << ",ordering=" << ordering << ",initialtransformation=" << initialTransformation << ",sampleaccesspattern=" << sampleAccessPattern << ",accesstransformation=" << accessTransformation <<  std::endl;

                        std::vector<std::size_t> timesDec;
                        std::vector<std::size_t> timesAccessTransformation;
                        std::vector<std::size_t> timesEnc;

                        for (int rep = 0; rep < numReps; rep++) {
                            std::unique_ptr<StatefulIntegerCodec<int32_t>> experimentBaseCodec(baseCodec->cloneFresh());

                            std::vector<std::unique_ptr<StatefulIntegerCodec<int32_t>>> codecGrid = splitIntoFullBlocks(
                                band, nXSize, nYSize, blockSize, numBlocks, std::move(experimentBaseCodec), min, initialTransformation, ordering);

                            if (codecGrid.size() == 0) {
                                std::cerr << "NO CODECS FORMING GRID." << std::endl;
                                GDALClose(dataset);
                                return 1;
                            }
                                
                            // Perform accesses - part to be measured
                            benchmarkAccess(codecGrid, blockSize, sampleAccessPattern, accessTransformation, timesDec, timesAccessTransformation, timesEnc);
                        }

                        std::cout << "tottimedec:" << sum(timesDec);
                        std::cout << ",meantimedec:" << mean(timesDec);
                        std::cout << ",vartimedec:" << variance(timesDec, mean(timesDec));

                        std::cout << ",tottimetrans:" << sum(timesAccessTransformation);
                        std::cout << ",meantimetrans:" << mean(timesAccessTransformation);
                        std::cout << ",vartimetrans:" << variance(timesAccessTransformation, mean(timesAccessTransformation));

                        std::cout << ",tottimeenc:" << sum(timesEnc);
                        std::cout << ",meantimeenc:" << mean(timesEnc);
                        std::cout << ",vartimeenc:" << variance(timesEnc, mean(timesEnc)) << std::endl;
                    }
                }
            }
        }
    }

    GDALClose(dataset);
    return 0;
}