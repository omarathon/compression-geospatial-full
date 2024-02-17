#include "ic.h"
#include "gdal.h"
#include "cpl_conv.h" // For CPLMalloc and CPLFree
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#define FILE_DTYPE_GDAL GDT_Int16
#define FILE_DTYPE uint16_t


// linearly traverse through the n values
void linearSweep(FILE_DTYPE *data, int n) {
    for (int i = 0; i < n; i++) {
        FILE_DTYPE value = data[i];
    }
}

// randomly sample n values from the collection of n values
void randomSweep(FILE_DTYPE *data, int n) {
    for (int i = 0; i < n; i++) {
        int randomIndex = rand() % n;
        // Access the value at the random index
        FILE_DTYPE value = data[randomIndex];
    }
}

void compressArray(const FILE_DTYPE* input, size_t length, uint8_t** compressed, size_t* compressedSize) {
    *compressedSize = p4nenc32(input, length, NULL); // Get required size
    *compressed = (uint8_t*)malloc(*compressedSize); // Allocate memory
    p4nenc32(input, length, *compressed); // Compress
}

void decompressAndProcessWindow(const uint8_t* compressed, size_t blockSize, size_t windowLength, size_t offset, size_t length) {
    FILE_DTYPE* decompressed = (FILE_DTYPE*)malloc(windowLength * sizeof(FILE_DTYPE));

    for (size_t i = offset; i < length && i + blockSize <= length; i += blockSize) {
        p4ndec32(&compressed[i], blockSize, decompressed); // Decompress window

        // Process elements in the decompressed window
        for (size_t j = 0; j < blockSize && i + j < length; ++j) {
            printf("%u ", decompressed[j]); // Example processing: simply print values
        }
    }

    free(decompressed);
}

int closest_multiple_less_than(int value) {
    return (value / 64) * 64;
}

int main() {
    // Seed random generator.
    srand(0);
    
    GDALDatasetH hDataset;
    GDALAllRegister();

    // Open the dataset
    hDataset = GDALOpen("/home/omar/diss/geotiffs/srtm_45_15.tif", GA_ReadOnly);
    if(hDataset == NULL) {
        printf("Failed to open file.\n");
        return 1;
    }

    GDALRasterBandH hBand = GDALGetRasterBand(hDataset, 1);

    // int width = GDALGetRasterBandXSize(hBand);
    // int height = GDALGetRasterBandYSize(hBand);
    int width = 1000;
    int height = 1000;
    int chunkSize = closest_multiple_less_than(width);
    int chunkSizeY = 1;
    printf("width: %d\n, chunkSize: %d\n", width, chunkSize);

    FILE_DTYPE *pafScanline = (FILE_DTYPE *) CPLMalloc(sizeof(FILE_DTYPE) * chunkSize * chunkSizeY);

    // double total;
    for (int rep = 0; rep < 500; rep++) {
        // total = 0.0;
        for (int y = 0; y < height; y += chunkSizeY) {
            int currentChunkHeight = chunkSizeY < height - y ? chunkSizeY : height - y;
            for (int x = 0; x < width; x += chunkSize) {
                int currentChunkWidth = chunkSize < width - x ? chunkSize : width - x;

                // Read the chunk
                // Note: Ensure pafScanline buffer size is appropriate for the readWidth and readHeight
                GDALRasterIO(hBand, GF_Read, x, y, currentChunkWidth, currentChunkHeight, 
                            pafScanline, currentChunkWidth, currentChunkHeight, FILE_DTYPE_GDAL, 0, 0);

                linearSweep(pafScanline, currentChunkWidth * currentChunkHeight);
            }
        }
    }

    // double mean = total / (width * height);
    // printf("Mean Value: %lf\n", mean);

    // Cleanup
    CPLFree(pafScanline);
    GDALClose(hDataset);

    return 0;
}
