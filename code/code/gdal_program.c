#include "gdal.h"
#include "cpl_conv.h" // For CPLMalloc and CPLFree
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

// linearly traverse through the n values
void linearSweep(unsigned char *data, int n) {
    for (int i = 0; i < n; i++) {
        unsigned char value = data[i];
    }
}

// randomly sample n values from the collection of n values
void randomSweep(unsigned char *data, int n) {
    // Seed the random number generator
    for (int i = 0; i < n; i++) {
        // Generate a random index between 0 and n-1
        int randomIndex = rand() % n;
        // Access the value at the random index
        unsigned char value = data[randomIndex];
    }
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

    // Print dataset size and number of bands
    printf("Size is %dx%d, %d bands\n",
           GDALGetRasterXSize(hDataset), GDALGetRasterYSize(hDataset),
           GDALGetRasterCount(hDataset));

    GDALDataType eType;
    // Loop through bands
    for (int i = 1; i <= GDALGetRasterCount(hDataset); i++) {
        GDALRasterBandH hBand = GDALGetRasterBand(hDataset, i);
        eType = GDALGetRasterDataType(hBand);
        const char *pszDataType = GDALGetDataTypeName(eType);

        printf("Band %d: Data type = %s\n", i, pszDataType);
    }

    // For simplicity, let's just work with the first band
    GDALRasterBandH hBand = GDALGetRasterBand(hDataset, 1);

    // int nXSize = GDALGetRasterBandXSize(hBand);
    // int nYSize = GDALGetRasterBandYSize(hBand);
    int nXSize = 1000;
    int nYSize = 1000;

    // Read the raster data
    unsigned char *pafScanline = (unsigned char *) CPLMalloc(sizeof(unsigned char) * nXSize * nYSize);
    if (GDALRasterIO(hBand, GF_Read, 0, 0, nXSize, nYSize, 
                     pafScanline, nXSize, nYSize, GDT_Byte, 0, 0) != CE_None) {
        printf("RasterIO read error\n");
        CPLFree(pafScanline);
        GDALClose(hDataset);
        return 1;
    }

    for (int rep = 0; rep < 500; rep++) {
        randomSweep(pafScanline, nXSize * nYSize);
    }

    // double total;
    // // Calculate mean value
    // for (int rep = 0; rep < 500; rep++) {
    //     total = 0.0;
    //     for (int i = 0; i < nXSize * nYSize; ++i) {
    //         total += pafScanline[i];
    //     }
    // }

    // double mean = total / (nXSize * nYSize);
    // printf("Mean Value: %lf\n", mean);

    // Saving as PNG requires additional code and possibly using a different library
    // such as libpng, which is beyond the scope of this example.

    // Cleanup
    CPLFree(pafScanline);
    GDALClose(hDataset);

    return 0;
}
