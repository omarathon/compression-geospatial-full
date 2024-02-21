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

#define GDAL_DTYPE GDT_Int32

std::vector<std::unique_ptr<StatefulIntegerCodec<int32_t>>> splitIntoFullBlocks(
    GDALRasterBand* band, int rasterWidth, int rasterHeight, int blockSize, int numBlocks,
    std::unique_ptr<StatefulIntegerCodec<int32_t>> baseCodec, const int32_t min, const int ordering) {
    
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

        // blockData is currently in row-major order. remap according to desired ordering
        if (ordering == 1) {
            blockData = remapToZigzagOrder(blockData, blockSize);
        }
        else if (ordering == 2) {
            blockData = remapToMortonOrder(blockData, blockSize);
        }

        // Compress block data and store
        std::unique_ptr<StatefulIntegerCodec<int32_t>> clonedCodec(baseCodec->cloneFresh());
        clonedCodec->allocEncoded(blockData.data(), blockData.size());
        clonedCodec->encodeArray(blockData.data(), blockData.size());
        codecs[sampledBlocks] = std::move(clonedCodec);
    }

    return codecs;
}

void benchmarkRandomAccess(const std::vector<std::unique_ptr<StatefulIntegerCodec<int32_t>>>& codecs, int blockSize, int numRandomSamples) {
    srand(1);  // Seed the random number generator with 1
    
    int32_t* decbuf = (int32_t*)malloc((blockSize*blockSize + codecs[0]->getOverflowSize(blockSize*blockSize)) * sizeof(int32_t));

    std::size_t totalNanos = 0;
    std::vector<std::size_t> times;

    for (int i = 0; i < numRandomSamples; i++) {
        int blockIndex = rand() % (codecs.size()); // Ignore the last block as it's a different size
        const auto& codec = codecs[blockIndex];

        auto startDecode = std::chrono::high_resolution_clock::now();
        codec->decodeArray(decbuf, blockSize*blockSize);
        auto endDecode = std::chrono::high_resolution_clock::now();

        auto decodeTime = std::chrono::duration_cast<std::chrono::nanoseconds>(endDecode - startDecode).count();

        auto startEncode= std::chrono::high_resolution_clock::now();
        codec->encodeArray(decbuf, blockSize*blockSize);
        auto endEncode = std::chrono::high_resolution_clock::now();

        auto encodeTime = std::chrono::duration_cast<std::chrono::nanoseconds>(endEncode - startEncode).count();

        totalNanos += decodeTime + encodeTime;
        times.push_back(decodeTime + encodeTime);
    }

    double meanTime = (double)totalNanos / numRandomSamples;
    double variance = 0.0;
    for (auto time : times) {
        variance += (time - meanTime) * (time - meanTime);
    }
    variance /= numRandomSamples;

    std::cout << "tottdec:" << totalNanos;
    std::cout << ",meantdec:" << meanTime;
    std::cout << ",vartdec:" << variance << std::endl;

    free(decbuf);
}

// Function to process a single block for min and unique values
void computeMinAndUniqueValuesForBlock(GDALRasterBand* band, int xOff, int yOff, int blockSize, int32_t& min, std::unordered_set<int32_t>& unique_values_set) {
    std::vector<int32_t> blockData(blockSize * blockSize, 0);
    auto error = band->RasterIO(GF_Read, xOff, yOff, blockSize, blockSize, blockData.data(), blockSize, blockSize, GDT_Int32, 0, 0);
    if (error != CE_None) {
        std::cerr << "Error reading block data at offset (" << xOff << ", " << yOff << ")." << std::endl;
        return;
    }
    for (auto& value : blockData) {
        min = std::min(min, value);
        unique_values_set.insert(value);
    }
}

int main(int argc, char* argv[]) {
    if (argc < 7) {
        std::cout << "Usage: " << argv[0] << " <file path> <block size> <num blocks> <num random samples> <codec name> <block ordering {0=row_major,1=zig_zag,2=morton}>\n";
        return 1;
    }

    const char* filePath = argv[1];
    int blockSize = atoi(argv[2]);
    int numBlocks = atoi(argv[3]);
    int numRandomSamples = atoi(argv[4]);
    const char* codecName = argv[5];
    int ordering = atoi(argv[6]);

    if (ordering < 0 || ordering > 2) {
        std::cerr << "invalid ordering" << std::endl;
        return 1;
    }

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

    int32_t min = std::numeric_limits<int32_t>::max();
    std::unordered_set<int32_t> unique_values_set;

    int blocksInWidth = nXSize / blockSize;
    int blocksInHeight = nYSize / blockSize;

    // Process each block to compute min and unique values
    for (int y = 0; y < blocksInHeight; ++y) {
        for (int x = 0; x < blocksInWidth; ++x) {
            int xOff = x * blockSize;
            int yOff = y * blockSize;
            computeMinAndUniqueValuesForBlock(band, xOff, yOff, blockSize, min, unique_values_set);
        }
    }

    // Adjust unique_values_set to normalize all values based on the computed min
    std::unordered_set<int32_t> normalised_unique_values;
    for (const auto& value : unique_values_set) {
        normalised_unique_values.insert(min < 0 ? value - min : value);
    }

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

        // auto cascadeCodec = std::make_unique<DictCodecPacking<dict_type>>(std::any_cast<std::unordered_map<int32_t, dict_type>&>(dict), reverseDict);
        auto cascadeCodec = std::make_unique<DeltaCodecAVX2>();
        // auto cascadeCodec = std::make_unique<DeltaCodecAVX512>();
        // auto cascadeCodec = std::make_unique<FORCodecAVX512>();
        // auto cascadeCodec = std::make_unique<RLECodecAVX2>();

        return initCodecs(std::any_cast<std::unordered_map<int32_t, dict_type>&>(dict), reverseDict,
        /* nonCascaded */ true, std::move(cascadeCodec));
    };

    std::vector<int32_t> unique_values(normalised_unique_values.begin(), normalised_unique_values.end());

    std::cout << "num_unique_values = " << unique_values.size() << std::endl;

    int num_bits_required = std::ceil(std::log2(unique_values.size()));

    // Initialize codecs based on the number of bits required
    std::vector<std::unique_ptr<StatefulIntegerCodec<int32_t>>> allCodecs;
    if (num_bits_required <= 8) {
        std::cout << "uint8 dict" << std::endl;
        allCodecs = createAndAddCodecs(uint8_t{}, unique_values);
    } else if (num_bits_required <= 16) {
        std::cout << "uint16 dict" << std::endl;
        allCodecs = createAndAddCodecs(uint16_t{}, unique_values);
    } else { // Assuming 32
        std::cout << "uint32 dict" << std::endl;
        allCodecs = createAndAddCodecs(uint32_t{}, unique_values);
    }

    // Select the codec with the specified name
    std::unique_ptr<StatefulIntegerCodec<int32_t>> baseCodec;
    for (auto& codec : allCodecs) {
        if (codec->name() == codecName) {
            baseCodec = std::move(codec);
            break;
        }
    }

    if (!baseCodec) {
        std::cerr << "NO CODEC WITH NAME " << codecName << std::endl;
        GDALClose(dataset);
        return 1;
    }

    allCodecs.clear();
    allCodecs.shrink_to_fit();

    std::vector<std::unique_ptr<StatefulIntegerCodec<int32_t>>> codecs = splitIntoFullBlocks(
        band, nXSize, nYSize, blockSize, numBlocks, std::move(baseCodec), min, ordering);

    // <block size> <num blocks> <num random samples> <codec name>
    std::cout << "**BENCHMARK RANDOM ACCESS**" << std::endl;
    std::cout << "file=" << filePath << ",blocksize=" << blockSize << ",numblocks=" << numBlocks << ",randomsamples=" << numRandomSamples << ",codec=" << codecName << std::endl;

    if (codecs.size() == 0) {
        std::cerr << "NO CODECS FORMING GRID." << std::endl;
        GDALClose(dataset);
        return 1;
    }

    // Benchmark random accesses, with perf running.

    // // Fork process here
    // int pid = getpid();
    // int cpid = fork();

    // if (cpid == 0) {
    //     // Child process: run perf stat
    //     char buf[500];
    //     sprintf(buf, "perf stat -B -e cache-references,cache-misses,cycles,instructions,branches,faults,migrations -p %d > stat.log 2>&1", pid);
    //     execl("/bin/sh", "sh", "-c", buf, NULL);
    //     exit(0); // Ensure child exits if execl fails
    // } 
    // else {
    //     // Parent process: continue with the program

    //     // Set the child as the leader of its process group
    //     setpgid(cpid, 0);

    //     // Give a moment for the child process to start perf
    //     sleep(.5); // Might need adjustment
        
    // Perform random accesses - part to be measured
    benchmarkRandomAccess(codecs, blockSize, numRandomSamples);

    // // Kill child process and its descendants
    // kill(-cpid, SIGINT);

    // // Wait for the child process to complete
    // waitpid(cpid, NULL, 0);

    // sleep(.5);

    // // Read and output the contents of stat.log
    // std::ifstream statFile("stat.log");
    // std::string line;
    // while (std::getline(statFile, line)) {
    //     std::cout << line << std::endl;
    // }
    // statFile.close();

    GDALClose(dataset);
    return 0;
    // }
}