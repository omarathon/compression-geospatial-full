#include "gdal.h"
#include "cpl_conv.h" // For CPLMalloc and CPLFree
#include <stdio.h>

int closest_multiple_less_than(int value) {
    return (value / 64) * 64;
}

int main() {
    GDALDatasetH hDataset;
    GDALAllRegister();

    // Open the dataset
    hDataset = GDALOpen("/home/omar/diss/geotiffs/2656.tif", GA_ReadOnly);
    if(hDataset == NULL) {
        printf("Failed to open file.\n");
        return 1;
    }

    GDALRasterBandH hBand = GDALGetRasterBand(hDataset, 1);
    int nXSize = GDALGetRasterBandXSize(hBand);
    int nYSize = GDALGetRasterBandYSize(hBand);

    // Define chunk size
    // int nChunkXSize = closest_multiple_less_than(nXSize / sizeof(unsigned char)); // Width of the chunk
    int nChunkXSize = closest_multiple_less_than(nXSize / sizeof(unsigned char)); // Width of the chunk
    printf("nChunkXSize: %d\n", nChunkXSize);
    int nChunkYSize = 1 / sizeof(unsigned char);    // Height of the chunk, adjust as needed

    unsigned char *pafScanline = (unsigned char *) CPLMalloc(sizeof(unsigned char) * nChunkXSize * nChunkYSize);
    
    double total = 0.0;

    // Process the file in chunks
    for (int y = 0; y < nYSize; y += nChunkYSize) {
        // int nReadYSize = (y + nChunkYSize > nYSize) ? nYSize - y : nChunkYSize;
        for (int x = 0; x < nXSize; x += nChunkXSize) {
            int nReadXSize = (x + nChunkXSize > nXSize) ? nXSize - x : nChunkXSize;

            GDALRasterIO(hBand, GF_Read, x, y, nReadXSize, 1, 
                             pafScanline, nReadXSize, 1, GDT_Byte, 0, 0);

            // Perform operations on the chunk here
            // For example, calculate mean, apply filters, etc.
            for (int i = 0; i < nReadXSize * 1; ++i) {
                total += pafScanline[i];
            }
        }
    }

    double mean = total / (nXSize * nYSize);
    printf("Mean Value: %lf\n", mean);

    // Cleanup
    CPLFree(pafScanline);
    GDALClose(hDataset);

    return 0;
}
