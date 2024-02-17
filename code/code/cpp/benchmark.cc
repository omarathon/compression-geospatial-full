#include <iostream>
#include <vector>
#include <chrono>
#include <numeric>
#include <cmath>
#include "gdal_priv.h"
// #include "compression.h"

std::vector<int32_t> readGeoTiffData(const char* filename, int& w, int& h) {
    GDALDataset* dataset = (GDALDataset*) GDALOpen(filename, GA_ReadOnly);
    if (!dataset) {
        std::cerr << "Failed to open file: " << filename << std::endl;
        exit(1);
    }

    // Print dataset metadata
    char **papszMetadata = dataset->GetMetadata("IMAGE_STRUCTURE");
    if (papszMetadata != NULL) {
        std::cout << "Metadata for " << filename << ":\n";
        for (int i = 0; papszMetadata[i] != NULL; ++i) {
            std::cout << papszMetadata[i] << "\n";
        }
    }

    // std::cout << "hmm" << " " << dataset->GetMetadataItem/("COMPRESSION", "IMAGE_STRUCTURE") << std::endl;

    // Print metadata for each band
    int nBands = dataset->GetRasterCount();
    std::cout << "nBands " << nBands << std::endl;

    for (int i = 1; i <= nBands; i++) {
        GDALRasterBand* band = dataset->GetRasterBand(i);
        papszMetadata = band->GetMetadata("IMAGE_STRUCTURE");
        if (papszMetadata != NULL) {
            std::cout << "Metadata for Band " << i << ":\n";
            for (int j = 0; papszMetadata[j] != NULL; ++j) {
                std::cout << papszMetadata[j] << "\n";
            }
        }

        // Additional band information can be printed here if needed
    }

    GDALRasterBand* band = dataset->GetRasterBand(1);
    int xSize = band->GetXSize();
    int ySize = band->GetYSize();
    std::vector<int32_t> data(xSize * ySize);
    band->RasterIO(GF_Read, 0, 0, xSize, ySize, data.data(), xSize, ySize, GDT_Int32, 0, 0);
    GDALClose(dataset);
    w = xSize;
    h = ySize;
    return data;
}

void benchmark(std::vector<int32_t>& data, int iterations, const char* compmode, const int w, const int h) {
    std::vector<double> compressTimes, decompressTimes;
    size_t dataSizeInBits = data.size() * sizeof(int32_t) * 8; // Bits before compression

    std::cout << " w " << w << " h " << h << " d " << data.size() << std::endl;

    for (int i = 0; i < iterations; ++i) {
        // Mock Compression

        // Create a GDAL dataset with DEFLATE compression options
        GDALDriver* driver = GetGDALDriverManager()->GetDriverByName("MEM");
        GDALDataset* memDataset = driver->Create("wtf", w, h, 1, GDT_Int32, NULL);
        memDataset->SetMetadataItem("COMPRESSION", "NONE", "IMAGE_STRUCTURE");
        if (memDataset == NULL || driver == NULL) {
            printf("error");
            exit(1);
        }
        GDALRasterBand* memBand = memDataset->GetRasterBand(1);
        memBand->SetMetadataItem("COMPRESSION", "NONE", "IMAGE_STRUCTURE");
        

        auto start = std::chrono::high_resolution_clock::now();
        // Write the data to the in-memory dataset with DEFLATE compression
        if (memBand->RasterIO(GF_Write, 0, 0, w, h, data.data(), w, h, GDT_Int32, 0, 0) != CE_None) {
            printf("error");
            exit(1);
        }

        auto end = std::chrono::high_resolution_clock::now();
        compressTimes.push_back(std::chrono::duration<double>(end - start).count());

        // Read the compressed data from the in-memory dataset
        std::vector<int32_t> compressedData(w * h);
        // std::vector<int32_t> compressedData(data.size());

        // Mock Decompression
        start = std::chrono::high_resolution_clock::now();
        if (memBand->RasterIO(GF_Read, 0, 0, w, h, compressedData.data(), w, h, GDT_Int32, 0, 0) != CE_None) {
            printf("error");
            exit(1);
        }

    
        // size_t compressedSizeInBits = compressedData.size() * sizeof(int32_t) * 8; // Bits after compression

        
        

        end = std::chrono::high_resolution_clock::now();
        decompressTimes.push_back(std::chrono::duration<double>(end - start).count());

        // Report compression ratio for first iteration as it stays the same
        if (i == 0) {
            // double compressionRatio = static_cast<double>(dataSizeInBits) / compressedSizeInBits;
            // double bitsPerInteger = static_cast<double>(compressedSizeInBits) / compressedData.size();

            // std::cout << "Compression Ratio: " << compressionRatio << ", Bits per Integer: " << bitsPerInteger << "\n";
        }

        // Verify decompressed data is equal to original data
        // for (int i = 0; i < data.size(); i++) {
        //     if (compressedData[i] != 0 ) {
        //         std::cout << "c " << compressedData[i] << " d " << data[i] << "\n";
        //     }
        // }
        for (int j = 0; j < data.size(); j++) {
            if (compressedData[j] != data[j]) {
                std::cerr << "Error: Decompressed data does not match original data! " << compressedData[j] << " " << data[j] << " " << j << std::endl;
                exit(1);
            }
        }
    }

    // Calculate Mean and Standard Deviation for times
    double meanCompressTime = std::accumulate(compressTimes.begin(), compressTimes.end(), 0.0) / compressTimes.size();
    double meanDecompressTime = std::accumulate(decompressTimes.begin(), decompressTimes.end(), 0.0) / decompressTimes.size();

    double sdCompress = std::sqrt(std::inner_product(compressTimes.begin(), compressTimes.end(), compressTimes.begin(), 0.0) / compressTimes.size() - meanCompressTime * meanCompressTime);
    double sdDecompress = std::sqrt(std::inner_product(decompressTimes.begin(), decompressTimes.end(), decompressTimes.begin(), 0.0) / decompressTimes.size() - meanDecompressTime * meanDecompressTime);

    // Reporting Aggregate Metrics
    std::cout << "Average Compression Time: " << meanCompressTime << "s (SD: " << sdCompress << ")\n";
    std::cout << "Average Decompression Time: " << meanDecompressTime << "s (SD: " << sdDecompress << ")\n";
}

int main(int argc, char** argv) {
    GDALAllRegister();

    if (argc != 3) {
        std::cerr << "Usage: " << argv[0] << " <GeoTIFF file path> <compmode>" << std::endl;
        return 1;
    }

    const char* filename = argv[1];
    const char* compmode = argv[2];


    int iterations = 10; // Number of iterations for benchmarking

    int w, h;
    std::vector<int32_t> data = readGeoTiffData(filename, w, h);
    benchmark(data, iterations, compmode, w, h);

    return 0;
}
