#include <iostream>
#include <vector>
#include <chrono>
#include <memory>
#include "gdal_priv.h"
#include "codec_collection.h"
#include <unordered_map>
#include <cmath>
#include <algorithm>
#include <any>
#include <variant>
#include "remappings.h"
#include "transformations.h"
#include "util.h"

struct CodecStats {
    float cf  = 0;
    float bpi = 0;
    float tenc = 0;
    float tdec = 0;
};

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

// Function to benchmark a window of data
std::vector<CodecStats> benchmarkWindow(std::vector<int32_t>& windowData, std::vector<std::unique_ptr<StatefulIntegerCodec<int32_t>>>& codecs) {
    std::vector<CodecStats> allCodecStats(codecs.size());

    for (int ci = 0; ci < codecs.size(); ci++) {
        auto& codec = codecs[ci];

        CodecStats stats;

        codec->allocEncoded(windowData.data(), windowData.size());

        auto startEncode = std::chrono::high_resolution_clock::now();
        try {
            codec->encodeArray(windowData.data(), windowData.size());
        }
        catch (const std::exception& error) {
            std::cout << " ERROR see cerr " << std::endl;
            std::cerr << "error encoding " << codec->name() << ": " << error.what() << std::endl;
            continue;
        }
        auto endEncode = std::chrono::high_resolution_clock::now();
        
        size_t numCodedValues = codec->encodedNumValues();
        size_t sizeCodedValue = codec->encodedSizeValue();

        std::vector<int32_t> windowDataBack(windowData.size() + codec->getOverflowSize(windowData.size()));
        auto startDecode = std::chrono::high_resolution_clock::now();
        try {
            codec->decodeArray(windowDataBack.data(), windowData.size());
        }
        catch (const std::exception& error) {
            std::cout << " ERROR see cerr " << std::endl;
            std::cerr << "error decoding " << codec->name() << ": " << error.what() << std::endl;
            continue;
        }
        auto endDecode = std::chrono::high_resolution_clock::now();

        // Verify
        bool good = true;
        for (int i = 0; i < windowData.size(); i++) {
            if (windowData[i] != windowDataBack[i]) {
                std::cout << " ERROR see cerr " << std::endl;
                std::cerr << "in!=out " << codec->name() << "(" << "i=" << i << ":o" << windowData[i] << "b" << windowDataBack[i] << "," << "len=" << windowData.size() << ")" <<std::endl;
                good = false;
                break;
            }
        }

        if (!good) {
            continue;
        }

        // Calculate compression factor
        auto compressionFactor = (float)(numCodedValues * sizeCodedValue) / (float)(windowData.size() * sizeof(int32_t));
        auto bitsPerInt = (float)(numCodedValues * sizeCodedValue) / (float)windowData.size();
        // std::cout << ",cf:" << compressionFactor << ",bpi:" << bitsPerInt; 
        stats.cf = compressionFactor;
        stats.bpi = bitsPerInt;

        // Calculate time taken
        auto compressTime = std::chrono::duration_cast<std::chrono::nanoseconds>(endEncode - startEncode).count();
        auto decompressTime = std::chrono::duration_cast<std::chrono::nanoseconds>(endDecode - startDecode).count();
        // std::cout << ",tenc:" << compressTime << ",tdec:" << decompressTime << std::endl;
        stats.tenc = compressTime;
        stats.tdec = decompressTime;

        // Reset codec.
        codecs[ci] = std::unique_ptr<StatefulIntegerCodec<int32_t>>(codec->cloneFresh());

        allCodecStats[ci] = stats;
    }

    return allCodecStats;
}

std::vector<int32_t> readGeoTiffBlock(GDALRasterBand* band, int xOff, int yOff, int blockSize, int rasterWidth, int rasterHeight) {
    int blockWidth = std::min(blockSize, rasterWidth - xOff);
    int blockHeight = std::min(blockSize, rasterHeight - yOff);

    std::vector<int32_t> blockData(blockWidth * blockHeight);
    band->RasterIO(GF_Read, xOff, yOff, blockWidth, blockHeight, blockData.data(), blockWidth, blockHeight, GDT_Int32, 0, 0);

    return blockData;
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

// BEHAVIOUR: if composite codec matches the name of a codec, then we will only benchmark composites of that codec + physical codecs. if composite codec does not match, then we benchmark all non-cascades (both logical & physical algorithms)
int main(int argc, char** argv) {
    if (argc != 7) {
        std::cerr << "usage: " << argv[0] << " <GeoTIFF file path> <block size> <num blocks> <block ordering {default: row-major | 'zigzag' | 'morton'}>[] <composite codec>[] <transformation {default: none | 'threshold' | 'smoothAndShift' | 'indexBasedClassification' | 'valueBasedClassification' | 'valueShift'}>[]" << std::endl;
        return 1;
    }

    GDALAllRegister();
    const char* filename = argv[1];
    int blockSize = std::stoi(argv[2]);
    int nBlocks = std::stoi(argv[3]);
    std::vector<std::string> orderings = parseCommaDelimited(std::string(argv[4]));
    std::vector<std::string> composite_codec_names = parseCommaDelimited(std::string(argv[5]));
    std::vector<std::string> transformations = parseCommaDelimited(std::string(argv[6]));

    GDALDataset* dataset = (GDALDataset*) GDALOpen(filename, GA_ReadOnly);
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

                    std::cout << "**BENCHMARK**\nfile=" << argv[1] << ",blockSize=" << blockSize << ",nBlocks=" << nBlocks << ",composite=" << composite_codec_name << ",ordering=" << ordering << ",transformation=" << transformation << std::endl;

                    // Make dict etc.

                        int blocksInWidth = rasterWidth / blockSize;
                        int blocksInHeight = rasterHeight / blockSize;

                        int32_t min = std::numeric_limits<int32_t>::max();
                        std::unordered_set<int32_t> unique_values_set;

                        
                        for (int y = 0; y < blocksInHeight; ++y) {
                            for (int x = 0; x < blocksInWidth; ++x) {
                                int xOff = x * blockSize;
                                int yOff = y * blockSize;
                                computeMinAndUniqueValuesForBlock(band, ordering, transformation, xOff, yOff, blockSize, min, unique_values_set);
                            }
                        }

                        size_t num_unique_values = unique_values_set.size();

                        std::cout << "num_unique_values = " << num_unique_values << std::endl;

                        std::vector<int32_t> unique_values(unique_values_set.begin(), unique_values_set.end());

                        std::any dict;
                        std::vector<int32_t> reverseDict;
                    
                    
                    auto createAndAddCodecs = [&](auto dummy) {
                        using dict_type = decltype(dummy);
                        std::unordered_map<int32_t, dict_type> localDict;

                        reverseDict.resize(num_unique_values);
                        dict_type index = 0;
                        for (int32_t value : unique_values) {
                            localDict[value] = index;
                            reverseDict[index] = value;
                            ++index;
                        }

                        dict = std::move(localDict); // Store the dictionary in std::any

                        auto all_codecs = initCodecs(std::any_cast<std::unordered_map<int32_t, dict_type>&>(dict), reverseDict, 
                                        /* nonCascaded */ true, /* cascadeCodec */ nullptr);
                        
                        // Build cascades if desired.
                        // Select the codec with the specified name
                        std::unique_ptr<StatefulIntegerCodec<int32_t>> baseCodec;
                        for (auto& codec : all_codecs) {
                            if (codec->name() == composite_codec_name) {
                                baseCodec = std::move(codec);
                                break;
                            }
                        }

                        if (baseCodec) {
                            return initCodecs(std::any_cast<std::unordered_map<int32_t, dict_type>&>(dict), reverseDict, 
                                        /* nonCascaded */ false, /* cascadeCodec */ std::move(baseCodec));
                        }
                        
                        return all_codecs;
                    };

                    int num_bits_required = std::ceil(std::log2(num_unique_values));

                    // Initialize codecs based on dictType
                    std::vector<std::unique_ptr<StatefulIntegerCodec<int32_t>>> codecs;
                    codecs = createAndAddCodecs(uint32_t{});

                    std::cout << "*CODECS:*" << std::endl;
                    for (int ci = 0; ci < codecs.size(); ci++) {
                        std::cout << ci << "=" << codecs[ci]->name() << std::endl;
                    }
                    std::cout << "*ENDCODECS*" << std::endl;

                    // Calculate the total number of blocks that fit in the raster
                    int totalBlocks = blocksInWidth * blocksInHeight;

                    // Calculate spacing between blocks to process
                    int blockSpacing = nBlocks == 0 ? 0 : std::max(1, totalBlocks / nBlocks);

                    // Counter for processed blocks
                    int processedBlocks = 0;
                    int windowIndex = 0;

                    // Benchmark each window
                    std::vector<std::vector<CodecStats>> codecWindowStats(codecs.size());
                    for (int by = 0; by < blocksInHeight && processedBlocks < nBlocks; by++) {
                        for (int bx = 0; bx < blocksInWidth && processedBlocks < nBlocks; bx++) {
                            // Check if the current block index matches the spacing criteria
                            if ((by * blocksInWidth + bx) % blockSpacing != 0) continue;

                            int xOff = bx * blockSize;
                            int yOff = by * blockSize;

                            std::vector<int32_t> blockData = readGeoTiffBlock(band, xOff, yOff, blockSize, rasterWidth, rasterHeight);
                            if (blockData.size() != blockSize * blockSize) {
                                continue;
                            }

                            // Normalise window to all positive
                            if (min < 0) {
                                for (int i = 0; i < blockData.size(); i++) {
                                    blockData[i] += (-min);
                                }
                            }

                            remapAndTransformData(blockData, ordering, transformation, blockSize);

                            std::vector<CodecStats> allCodecStats = benchmarkWindow(blockData, codecs);
                            for (int ci = 0; ci < codecs.size(); ci++) {
                                codecWindowStats[ci].push_back(allCodecStats[ci]);
                            }

                            processedBlocks++; // Increment the count of processed blocks
                        }
                    }

                    for (int ci = 0; ci < codecs.size(); ci++) {
                        std::cout << "c:" << ci;

                        std::vector<CodecStats>& codecStatsAcrossWindows = codecWindowStats[ci];
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
                        float cfmean = mean(cfs);
                        float cfvar = variance(cfs, cfmean);
                        float bpimean = mean(bpis);
                        float bpivar = variance(bpis, bpimean);
                        float tencmean = mean(tencs);
                        float tencvar = variance(tencs, tencmean);
                        float tdecmean = mean(tdecs);
                        float tdecvar = variance(tdecs, tdecmean);
                        std::cout << ",cfmean:" << cfmean << ",cfvar:" << cfvar
                                  << ",bpimean:" << bpimean << ",bpivar:" << bpivar 
                                  << ",tencmean:" << tencmean << ",tencvar:" << tencvar 
                                  << ",tdecmean:" << tdecmean << ",tdecvar:" << tdecvar << std::endl; 
                    }
                }
                catch (const std::exception& e) {
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