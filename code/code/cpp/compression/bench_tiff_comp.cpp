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

// Function to benchmark a window of data
void benchmarkWindow(std::vector<int32_t>& windowData, std::vector<std::unique_ptr<StatefulIntegerCodec<int32_t>>>& codecs) {
    for (int ci = 0; ci < codecs.size(); ci++) {
        auto& codec = codecs[ci];

        // std::cout << "ws: " << windowStart << ", wl: " << windowData.size() << ", c: " << codec->name();
        std::cout << "c:" << ci;

        codec->allocEncoded(windowData.size());

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
        for (int i = 0; i < windowData.size(); i++) {
            if (windowData[i] != windowDataBack[i]) {
                std::cout << " ERROR see cerr " << std::endl;
                std::cerr << "in!=out " << codec->name() << std::endl;
                continue;
            }
        }

        // Calculate compression factor
        auto compressionFactor = (float)(numCodedValues * sizeCodedValue) / (float)(windowData.size() * sizeof(int32_t));
        auto bitsPerInt = (float)(numCodedValues * sizeCodedValue) / (float)windowData.size();
        std::cout << ",cf:" << compressionFactor << ",bpi:" << bitsPerInt; 

        // Calculate time taken
        auto compressTime = std::chrono::duration_cast<std::chrono::nanoseconds>(endEncode - startEncode).count();
        auto decompressTime = std::chrono::duration_cast<std::chrono::nanoseconds>(endDecode - startDecode).count();
        std::cout << ",tenc:" << compressTime << ",tdec:" << decompressTime << std::endl;

        // Reset codec.
        codecs[ci] = std::unique_ptr<StatefulIntegerCodec<int32_t>>(codec->cloneFresh());
    }
}

// std::vector<int32_t> readGeoTiffLinearWindow(GDALRasterBand* band, int windowStart, int windowSize, int totalPixels, int w, int h) {
//     int xStart = windowStart % w;
//     int yStart = windowStart / w;

//     int windowEnd = std::min(windowStart + windowSize, totalPixels);
//     int xEnd = windowEnd % w;
//     int yEnd = windowEnd / w;

//     // Adjust yEnd for the edge case where windowEnd is at the start of a new row
//     if (xEnd == 0 && windowEnd != totalPixels) {
//         yEnd -= 1; // Move yEnd back by one row
//     }

//     // Ensure yEnd does not exceed the raster height
//     yEnd = std::min(yEnd, h - 1);

//     int readWidth = w; // Always read full width of the raster
//     int readHeight = yEnd - yStart + 1; // Calculate the number of rows to read

//     std::vector<int32_t> data(readWidth * readHeight);
//     band->RasterIO(GF_Read, 0, yStart, readWidth, readHeight, data.data(), readWidth, readHeight, GDT_Int32, 0, 0);

//     std::vector<int32_t> windowData;
//     windowData.reserve(windowSize);

//     for (int row = 0; row < readHeight; ++row) {
//         int rowStart = row * w;
//         int rowEnd = rowStart + w;

//         // Adjust for the start and end of the window
//         if (row == 0) {
//             rowStart += xStart;
//         }
//         if (row == readHeight - 1 && xEnd != 0) {
//             rowEnd = row * w + xEnd;
//         }

//         windowData.insert(windowData.end(), data.begin() + rowStart, data.begin() + rowEnd);
//     }

//     return windowData;
// }

std::vector<int32_t> readGeoTiffBlock(GDALRasterBand* band, int xOff, int yOff, int blockSize, int rasterWidth, int rasterHeight) {
    int blockWidth = std::min(blockSize, rasterWidth - xOff);
    int blockHeight = std::min(blockSize, rasterHeight - yOff);

    std::vector<int32_t> blockData(blockWidth * blockHeight);
    band->RasterIO(GF_Read, xOff, yOff, blockWidth, blockHeight, blockData.data(), blockWidth, blockHeight, GDT_Int32, 0, 0);

    return blockData;
}

int main(int argc, char** argv) {
    if (argc != 4) {
        std::cerr << "Usage: " << argv[0] << " <GeoTIFF file path> <block size> <num blocks>" << std::endl;
        return 1;
    }

    GDALAllRegister();
    const char* filename = argv[1];
    int blockSize = std::stoi(argv[2]);
    int nBlocks = std::stoi(argv[3]);

    GDALDataset* dataset = (GDALDataset*) GDALOpen(filename, GA_ReadOnly);
    if (!dataset) {
        std::cerr << "Failed to open file: " << filename << std::endl;
        return 1;
    }

    GDALRasterBand* band = dataset->GetRasterBand(1);
    int rasterWidth = band->GetXSize();
    int rasterHeight = band->GetYSize();
    // int w = 1253;
    // int h = 4021;
    int totalPixels = rasterWidth * rasterHeight;

    // if (windowSize == -1) {
    //     windowSize = totalPixels;
    // }

    // windowSize = std::min(windowSize, totalPixels);
    // int windowStride = nWindows == -1 ? 1 : std::max(1, (totalPixels / windowSize) / nWindows);

    std::cout << "**BENCHMARK**\nfile=" << argv[1] << ",blockSize=" << blockSize << ",nBlocks=" << nBlocks << std::endl;

    // Read the full raster data
    std::vector<int32_t> data_large(totalPixels);
    band->RasterIO(GF_Read, 0, 0, rasterWidth, rasterHeight, data_large.data(), rasterWidth, rasterHeight, GDT_Int32, 0, 0);

    // Normalise to all positive
    int32_t min = *std::min_element(data_large.begin(), data_large.end());

    std::cout << "min = " << min << std::endl;
    if (min < 0) {
        for (int i = 0; i < data_large.size(); i++) {
            data_large[i] += (-min);
        }
    }

    // Extract unique values
    std::unordered_set<int32_t> unique_values_set(data_large.begin(), data_large.end());
    size_t num_unique_values = unique_values_set.size();

    std::cout << "num_unique_values = " << num_unique_values << std::endl;

    // Convert unordered_set to vector for unique values
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

        // auto cascadeCodec = std::make_unique<DictCodecPacking<dict_type>>(std::any_cast<std::unordered_map<int32_t, dict_type>&>(dict), reverseDict);
        // auto cascadeCodec = std::make_unique<DeltaCodecAVX2>();
        // auto cascadeCodec = std::make_unique<DeltaCodecAVX512>();
        // auto cascadeCodec = std::make_unique<FORCodecAVX512>();
        auto cascadeCodec = std::make_unique<RLECodecAVX2>();

        return initCodecs(std::any_cast<std::unordered_map<int32_t, dict_type>&>(dict), reverseDict, 
                          /* nonCascaded */ false, /* cascadeCodec */ std::move(cascadeCodec));
    };

    int num_bits_required = std::ceil(std::log2(num_unique_values));

    // Initialize codecs based on dictType
    std::vector<std::unique_ptr<StatefulIntegerCodec<int32_t>>> codecs;
    if (num_bits_required <= 8) {
        std::cout << "uint8 dict" << std::endl;
        codecs = createAndAddCodecs(uint8_t{});
    }
    else if (num_bits_required <= 16) {
        std::cout << "uint16 dict" << std::endl;
        codecs = createAndAddCodecs(uint16_t{});
    }
    else { // Assuming 32
        std::cout << "uint32 dict" << std::endl;
        codecs = createAndAddCodecs(uint32_t{});
    }

    std::cout << "*CODECS:*" << std::endl;
    for (int ci = 0; ci < codecs.size(); ci++) {
        std::cout << ci << "=" << codecs[ci]->name() << std::endl;
    }
    std::cout << "*ENDCODECS*" << std::endl;

    // Free memory used by data_large
    data_large.clear();
    data_large.shrink_to_fit();


    // Calculate the total number of blocks that fit in the raster
    int blocksInWidth = rasterWidth / blockSize;
    int blocksInHeight = rasterHeight / blockSize;
    int totalBlocks = blocksInWidth * blocksInHeight;

    // Calculate spacing between blocks to process
    int blockSpacing = std::max(1, totalBlocks / nBlocks);

    // Process each window
    // Ignore the last window as it's nonconsistently sized

    // Counter for processed blocks
    int processedBlocks = 0;

    int windowIndex = 0;

    // Iterate through the raster in NxN blocks
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
            std::cout << "*wi:" << windowIndex++ << "(" << xOff << "," << yOff << ")" << std::endl;

            benchmarkWindow(blockData, codecs);

            processedBlocks++; // Increment the count of processed blocks
        }
    }


    // int windowIndex = 0;
    // for (int windowStart = 0; windowStart < totalPixels; windowStart += windowStride * windowSize) {
    //     std::vector<int32_t> windowData = readGeoTiffLinearWindow(band, windowStart, windowSize, totalPixels, w, h);
    //     if (windowData.size() != windowSize) {
    //         continue;
    //     }
    //     // Normalise window to all positive
    //     if (min < 0) {
    //         for (int i = 0; i < windowData.size(); i++) {
    //             windowData[i] += (-min);
    //         }
    //     }
    //     std::cout << "*wi:" << windowIndex++ << std::endl;
    //     benchmarkWindow(windowData, codecs, windowStart);
    //     // if (!benchmarkWindow(windowData, codecs, windowStart)) {
    //         // return 1;
    //     // }
    // }

    GDALClose(dataset);
    return 0;
}