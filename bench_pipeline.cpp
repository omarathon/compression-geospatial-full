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
#include <nmmintrin.h>  // SSE4.2

#define GDAL_DTYPE GDT_Int32

static int dummySum;

void remapAndTransformData(std::vector<int32_t>& blockData, const std::string& ordering, const std::string& transformation, const int blockSize) {
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
std::size_t applyAccessTransformation(std::vector<int32_t>& blockData, const std::string& transformation, std::size_t blockSize, bool isSumOptimised) {
    dummySum = 0;
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
        if (isSumOptimised) {
            dummySum = (static_cast<uint64_t>(blockData[blockSize*blockSize + 1]) << 32) | blockData[blockSize*blockSize];
            
        }
        else {
            int total = blockSize * blockSize;
            int i = 0;

            __m128i vsum = _mm_setzero_si128();

            for (; i + 4 <= total; i += 4) {
                __m128i v = _mm_loadu_si128((const __m128i*)&blockData[i]);
                vsum = _mm_add_epi32(vsum, v);
            }

            // horizontal reduce
            vsum = _mm_hadd_epi32(vsum, vsum);
            vsum = _mm_hadd_epi32(vsum, vsum);
            dummySum += _mm_cvtsi128_si32(vsum);

            // tail
            for (; i < total; ++i)
                dummySum += blockData[i];
        }
        // std::cout << "sum: " << dummySum << std::endl;
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
        if (isSumOptimised) {
            volatile uint64_t sum = 0;
            sum = (static_cast<uint64_t>(blockData[blockSize*blockSize + 1]) << 32) | blockData[blockSize*blockSize];
        }
        else {
            volatile int64_t dummy = 0; // Ensures reads aren't optimised away.
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

    int blocksInWidth = rasterWidth / blockSize;
    int blocksInHeight = rasterHeight / blockSize;
    int totalFullBlocks = blocksInWidth * blocksInHeight;

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

bool isCodecSumOptimised(const std::string& str) {
    if (str == "simdcomp") {
        return true;
    }
    else if (str == "FastPFor_SIMDPFor+VariableByte") {
        return true;
    }
    else if (str.substr(0, 24) == "[+]_custom_rle_vecavx512") {
        return true;
    }
    return false;
}

// access transformation {default: 'linearXOR' | 'linearSum' | 'randomXOR' | 'randomSum' | 'threshold' | 'smoothAndShift' | 'indexBasedClassification' | 'valueBasedClassification' | 'valueShift'}
void benchmarkAccess(std::vector<std::unique_ptr<StatefulIntegerCodec<int32_t>>>& codecs, std::unique_ptr<StatefulIntegerCodec<int32_t>> accessCodec, int blockSize, const std::string& sampleAccessPattern, const std::string& accessTransformation, 
                     std::size_t& nDec, double& meanTDec, std::size_t& nTrans, double& meanTTrans) {
    srand(1);  // Seed the random number generator with 1

    bool isDirectAccess = (codecs[0]->name() == "custom_direct_access");
    bool isDirectReenc = (accessCodec->name() == "custom_direct_access");

    bool isSumOptimised = isCodecSumOptimised(codecs[0]->name());

    std::vector<int32_t> decbuf;
    decbuf.resize(blockSize*blockSize + codecs[0]->getOverflowSize(blockSize*blockSize));

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

            ++nDec; 
            meanTDec += (decodeTime - meanTDec) / nDec;

            /* perform transformation */
            std::size_t accessTransformationTime = applyAccessTransformation(decbuf, accessTransformation, blockSize, isSumOptimised);
            // timesAccessTransformation.push_back(accessTransformationTime);

            ++nTrans; 
            meanTTrans += (accessTransformationTime - meanTTrans) / nTrans;

            if (dataChange) {
                /* re-encode with new codec */
                std::unique_ptr<StatefulIntegerCodec<int32_t>> clonedAccessCodec(accessCodec->cloneFresh());
                std::unique_ptr<StatefulIntegerCodec<int32_t>>& reencCodec = clonedAccessCodec;

                if (isDirectReenc) {
                    // timesEnc.push_back(0);
                    reencCodec->allocEncoded(decbuf.data(), blockSize*blockSize);
                    reencCodec->encodeArray(decbuf.data(), blockSize*blockSize);
                }
                else {
                    reencCodec->allocEncoded(decbuf.data(), blockSize * blockSize);
                    auto startEncode= std::chrono::high_resolution_clock::now();
                    reencCodec->encodeArray(decbuf.data(), blockSize*blockSize);
                    auto endEncode = std::chrono::high_resolution_clock::now();
                    auto encodeTime = std::chrono::duration_cast<std::chrono::nanoseconds>(endEncode - startEncode).count();
                    // timesEnc.push_back(encodeTime);
                }

                codecs[blockIndex] = std::move(clonedAccessCodec);
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
void computeMinAndUniqueValuesForBlock(GDALRasterBand* band, std::string& ordering, std::string& initialTransformation, std::string& accessTransformation, int xOff, int yOff, int blockSize, int32_t& min, std::unordered_set<int32_t>& unique_values_set_initial, std::unordered_set<int32_t>& unique_values_set_access) {
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

    remapAndTransformData(blockData, ordering, initialTransformation, blockSize);

    for (auto& value : blockData) {
        unique_values_set_initial.insert(value);
    }
    
    bool dataChange = 
           accessTransformation == "threshold" 
        || accessTransformation == "smoothAndShift" 
        || accessTransformation == "indexBasedClassification" 
        || accessTransformation == "valueBasedClassification" 
        || accessTransformation == "valueShift";

    if (dataChange) {
        applyAccessTransformation(blockData, accessTransformation, blockSize, false);
    }
    
    for (auto& value : blockData) {
        unique_values_set_access.insert(value);
    }
}

int main(int argc, char* argv[]) {
    if (argc < 11) {
        std::cout << "Usage: " << argv[0] << " <file path> <block size> <num blocks> <num reps> <initial codec names | 'all'>[] <access codec names | 'all'>[] <block ordering {default: row-major | 'zigzag' | 'morton'}>[] <initial transformation {default: none | 'threshold' | 'smoothAndShift' | 'indexBasedClassification' | 'valueBasedClassification' | 'valueShift'}>[] <sample access pattern {default: 'random' | 'linear'}>[] <access transformation {default: 'linearXOR' | 'linearSum' | 'randomXOR' | 'randomSum' | 'threshold' | 'smoothAndShift' | 'indexBasedClassification' | 'valueBasedClassification' | 'valueShift'}>[]";
        return 1;
    }

    const char* filePath = argv[1];
    int blockSize = atoi(argv[2]);
    int numBlocks = atoi(argv[3]);
    int numReps = atoi(argv[4]);
    std::vector<std::string> initialCodecNames = parseBarDelimited(std::string(argv[5]));
    std::vector<std::string> accessCodecNames = parseBarDelimited(std::string(argv[6]));
    std::vector<std::string> orderings = parseCommaDelimited(std::string(argv[7]));
    std::vector<std::string> initialTransformations = parseCommaDelimited(std::string(argv[8]));
    std::vector<std::string> sampleAccessPatterns = parseCommaDelimited(std::string(argv[9]));
    std::vector<std::string> accessTransformations = parseCommaDelimited(std::string(argv[10]));

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
            for (auto& accessTransformation : accessTransformations) {

                int32_t min = std::numeric_limits<int32_t>::max();
                std::unordered_set<int32_t> unique_values_set_initial;
                std::unordered_set<int32_t> unique_values_set_access;

                // Calculate full blocks that fit in the raster dimensions
                int totalFullBlocks = blocksInWidth * blocksInHeight;
                // Calculate interval to sample blocks evenly
                int sampleInterval = std::max(1, totalFullBlocks / numBlocks);

                for (int blockNum = 0, sampledBlocks = 0; sampledBlocks < numBlocks; blockNum += sampleInterval, sampledBlocks++) {
                    int blockIndex = blockNum % totalFullBlocks;
                    int xOff = (blockIndex % blocksInWidth) * blockSize;
                    int yOff = (blockIndex / blocksInWidth) * blockSize;
                    computeMinAndUniqueValuesForBlock(band, ordering, initialTransformation, accessTransformation, xOff, yOff, blockSize, min, unique_values_set_initial, unique_values_set_access);
                }

                // Convert unordered_set to vector for unique values
                std::vector<int32_t> unique_values_initial(unique_values_set_initial.begin(), unique_values_set_initial.end());
                std::vector<int32_t> unique_values_access(unique_values_set_access.begin(), unique_values_set_access.end());

                std::any dict_initial;
                std::vector<int32_t> reverseDict_initial;
                std::any dict_access;
                std::vector<int32_t> reverseDict_access;

                auto createAndAddCodecs = [&](auto dummy, const std::vector<int32_t>& unique_values, std::any& dict, std::vector<int32_t>& reverseDict) {
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

                    // auto cascadeDictCodec = std::make_unique<DictCodecAVX2<dict_type>>(std::any_cast<std::unordered_map<int32_t, dict_type>&>(dict), reverseDict);
                    // auto cascadeDeltaCodec = std::make_unique<DeltaCodecAVX512>();
                    // auto cascadeRleCodec = std::make_unique<RLECodecAVX512>();
                    // auto cascadeForCodec = std::make_unique<FORCodecAVX512>();

                    // for (auto& cascade : initCodecs(std::any_cast<std::unordered_map<int32_t, dict_type>&>(dict), reverseDict, 
                    //                                 /* nonCascaded */ false, /* cascadeCodec */ std::move(cascadeDictCodec))) {
                    //     base_codecs.push_back(std::move(cascade));
                    // }
                    // for (auto& cascade : initCodecs(std::any_cast<std::unordered_map<int32_t, dict_type>&>(dict), reverseDict, 
                    //                                 /* nonCascaded */ false, /* cascadeCodec */ std::move(cascadeDeltaCodec))) {
                    //     base_codecs.push_back(std::move(cascade));
                    // }
                    // for (auto& cascade : initCodecs(std::any_cast<std::unordered_map<int32_t, dict_type>&>(dict), reverseDict, 
                    //                                 /* nonCascaded */ false, /* cascadeCodec */ std::move(cascadeRleCodec))) {
                    //     base_codecs.push_back(std::move(cascade));
                    // }
                    // for (auto& cascade : initCodecs(std::any_cast<std::unordered_map<int32_t, dict_type>&>(dict), reverseDict, 
                    //                                 /* nonCascaded */ false, /* cascadeCodec */ std::move(cascadeForCodec))) {
                    //     base_codecs.push_back(std::move(cascade));
                    // }

                    auto directAccessCodec = std::make_unique<DirectAccessCodec>();
                    base_codecs.push_back(std::move(directAccessCodec));

                    return base_codecs;
                };

                std::cout << "num_unique_values_initial = " << unique_values_initial.size() << std::endl;
                std::cout << "num_unique_values_access = " << unique_values_access.size() << std::endl;

                std::vector<std::unique_ptr<StatefulIntegerCodec<int32_t>>> allCodecs_initial;
                allCodecs_initial = createAndAddCodecs(uint32_t{}, unique_values_initial, dict_initial, reverseDict_initial);
                std::vector<std::unique_ptr<StatefulIntegerCodec<int32_t>>> allCodecs_access;
                allCodecs_access = createAndAddCodecs(uint32_t{}, unique_values_access, dict_access, reverseDict_access);

                // Select the codecs with the specified names
                std::vector<std::unique_ptr<StatefulIntegerCodec<int32_t>>> baseCodecs;
                std::vector<std::unique_ptr<StatefulIntegerCodec<int32_t>>> accessCodecs;
                for (auto& codec : allCodecs_initial) {
                    if (initialCodecNames.size() == 1 && (initialCodecNames[0] == "all" || initialCodecNames[0] == "*")) {
                        std::unique_ptr<StatefulIntegerCodec<int32_t>> newCodec(codec->cloneFresh());
                        baseCodecs.push_back(std::move(newCodec));
                        continue;
                    }
                    for (auto& codecName : initialCodecNames) {
                        if (codecName == codec->name()) {
                            std::unique_ptr<StatefulIntegerCodec<int32_t>> newCodec(codec->cloneFresh());
                            baseCodecs.push_back(std::move(newCodec));
                            break;
                        }
                    }
                }
                for (auto& codec : allCodecs_access) {
                    if (accessCodecNames.size() == 1 && (accessCodecNames[0] == "all" || accessCodecNames[0] == "*")) {
                        std::unique_ptr<StatefulIntegerCodec<int32_t>> newCodec(codec->cloneFresh());
                        accessCodecs.push_back(std::move(newCodec));
                        continue;
                    }
                    for (auto& codecName : accessCodecNames) {
                        if (codecName == codec->name()) {
                            std::unique_ptr<StatefulIntegerCodec<int32_t>> newCodec(codec->cloneFresh());
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
                                std::cout << "file=" << filePath << ",blocksize=" << blockSize << ",numblocks=" << numBlocks << ",numreps=" << numReps << ",basecodec=" << baseCodecName << ",accesscodec=" << accessCodecName << ",ordering=" << ordering << ",initialtransformation=" << initialTransformation << ",sampleaccesspattern=" << sampleAccessPattern << ",accesstransformation=" << accessTransformation <<  std::endl;

                                std::size_t nDec = 0;
                                double meanTDec = 0.0;

                                std::size_t nTrans = 0;
                                double meanTTrans = 0.0;

                                for (int rep = 0; rep < numReps; rep++) {
                                    std::unique_ptr<StatefulIntegerCodec<int32_t>> experimentBaseCodec(baseCodec->cloneFresh());
                                    std::unique_ptr<StatefulIntegerCodec<int32_t>> experimentAccessCodec(accessCodec->cloneFresh());

                                    std::vector<std::unique_ptr<StatefulIntegerCodec<int32_t>>> codecGrid = splitIntoFullBlocks(
                                        band, nXSize, nYSize, blockSize, numBlocks, std::move(experimentBaseCodec), min, initialTransformation, ordering);

                                    if (codecGrid.size() == 0) {
                                        std::cerr << "NO CODECS FORMING GRID." << std::endl;
                                        GDALClose(dataset);
                                        return 1;
                                    }
                                        
                                    // Perform accesses - part to be measured
                                    benchmarkAccess(codecGrid, std::move(experimentAccessCodec), blockSize, sampleAccessPattern, accessTransformation, nDec, meanTDec, nTrans, meanTTrans);
                                }

                                std::cout << "meanTDec:" << meanTDec << ",meanTTrans:" << meanTTrans << std::endl;
                        }
                    }
                }
            }
        }
    }
    GDALClose(dataset);
    return 0;
}