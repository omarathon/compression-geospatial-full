#include "gdal.h"
#include "cpl_conv.h" // For CPLMalloc and CPLFree
#include <stdio.h>

// linearly traverse through the n values
void linearSweep(unsigned char *data, int n) {
    for (int i = 0; i < n; i++) {
        unsigned char value = data[i];
    }
}

// randomly sample n values from the collection of n values
void randomSweep(unsigned char *data, int n) {
    for (int i = 0; i < n; i++) {
        int randomIndex = rand() % n;
        // Access the value at the random index
        unsigned char value = data[randomIndex];
    }
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

    unsigned char *pafScanline = (unsigned char *) CPLMalloc(sizeof(unsigned char) * chunkSize * chunkSizeY);

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
                            pafScanline, currentChunkWidth, currentChunkHeight, GDT_Byte, 0, 0);

                randomSweep(pafScanline, currentChunkWidth * currentChunkHeight);
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
