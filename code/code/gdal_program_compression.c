#include "gdal.h"
#include "cpl_conv.h" // For CPLMalloc and CPLFree
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "ic.h"
#include <math.h>

// #define FILE_DTYPE_GDAL GDT_UInt32
// #define FILE_DTYPE uint32_t
// #define TURBOP4_ENC p4nenc128v32 // p4nenc16 p4nenc128v16
// #define TURBOP4_DEC p4ndec128v32 // p4ndec16 p4ndec128v16
// #define P4NENC_BOUND(n) ((n+32)/32+(n+32)*sizeof(FILE_DTYPE))

// #define FILE_DTYPE_GDAL GDT_Int16
// #define FILE_DTYPE int16_t
// #define TURBOP4_ENC p4nenc128v16 // p4nenc16 p4nenc128v16
// #define TURBOP4_DEC p4ndec128v16 // p4ndec16 p4ndec128v16
// #define P4NENC_BOUND(n) ((n+16)/16+(n+32)*sizeof(FILE_DTYPE))

#define FILE_DTYPE_GDAL GDT_Byte
#define FILE_DTYPE uint8_t
#define TURBOP4_ENC p4nenc8 // p4nenc16 p4nenc128v16
#define TURBOP4_DEC p4ndec8 // p4ndec16 p4ndec128v16
#define P4NENC_BOUND(n) ((n+8)/8+(n+32)*sizeof(FILE_DTYPE))

#define COMPRESS false

float applyWindowedSweepRaster(FILE_DTYPE** rasterData, size_t width, size_t height, int radius) {
    size_t countTotal = 0;
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            float sum = 0.0;
            size_t count = 0;

            // Define the circular window
            for (int wy = -radius; wy <= radius; wy++) {
                for (int wx = -radius; wx <= radius; wx++) {
                    if (sqrt(wx * wx + wy * wy) <= radius) {
                        int px = x + wx;
                        int py = y + wy;

                        // Boundary check
                        if (px >= 0 && px < width && py >= 0 && py < height) {
                            // sum += rasterData[py][px];
                            countTotal++;
                        }
                    }
                }
            }

            // // Compute the average for the window
            // float avg = sum / (count+1);
            // sumAvg += avg;
            // countAvg++;
        }
    }
    printf("Count total: %lu\n", countTotal);
    return 0.0;
    // return sumAvg / countAvg;
}

FILE_DTYPE** create2DGridFrom1DArray(FILE_DTYPE* flattenedArray, size_t width, size_t height) {
    FILE_DTYPE** grid = (FILE_DTYPE**)malloc(height * sizeof(FILE_DTYPE*));
    for (int i = 0; i < height; i++) {
        grid[i] = (FILE_DTYPE*)malloc(width * sizeof(FILE_DTYPE));
        for (int j = 0; j < width; j++) {
            grid[i][j] = flattenedArray[i * width + j];
        }
    }
    return grid;
}

// linearly traverse through the n values
void linearSweep(FILE_DTYPE *data, int n) {
    for (int i = 0; i < n; i++) {
        FILE_DTYPE value = data[i];
    }
}

// linearly traverse through the n values and increment them by 1, bounding the result to [0,128).
void linearSweepWrite(FILE_DTYPE *data, int n) {
    for (int i = 0; i < n; i++) {
        data[i] = (data[i] + 1) % 128;
    }
}

double sum = 0;
size_t sampleSize = 0;
void linearSweepAvg(FILE_DTYPE *data, int n) {
    for (int i = 0; i < n; i++) {
        FILE_DTYPE value = data[i];
        sum += (double)value;
        sampleSize++;
    }
}

// randomly sample n values from the collection of n values
void randomSweep(FILE_DTYPE *data, size_t n) {
    // Seed the random number generator
    for (int i = 0; i < n; i++) {
        // Generate a random index between 0 and n-1
        int randomIndex = rand() % n;
        // Access the value at the random index
        FILE_DTYPE value = data[randomIndex];
    }
}

// randomly sample n values from the collection of n values and increment them by 1, bounding the result to [0,128).
void randomSweepWrite(FILE_DTYPE *data, size_t n) {
    // Seed the random number generator
    for (int i = 0; i < n; i++) {
        // Generate a random index between 0 and n-1
        int randomIndex = rand() % n;
        // Access the value at the random index
        data[randomIndex] = (data[randomIndex] + 1) % 128;
    }
}


void compressArray(FILE_DTYPE* input, size_t length, uint8_t** compressed, size_t* compressedSize) {
    // printf("0\n");
    // *compressedSize = TURBOP4_ENC(input, length, NULL); // Get required size
    // printf("1\n");
    *compressed = (uint8_t*)malloc(P4NENC_BOUND(length)); // Allocate memory
    // printf("2\n");
    *compressedSize = TURBOP4_ENC(input, length, *compressed); // Compress
    *compressed = realloc(*compressed, *compressedSize);
    // printf("Compsize1: %lu \n", *compressedSize);
    // printf("3\n");
}

void decompressAndProcessWindow(uint8_t* compressed, FILE_DTYPE* decompressed, size_t blockSize, size_t length) {
    sum = 0;
    sampleSize = 0;
    size_t index_compressed = 0;
    // printf("Begin \n");
    size_t index_decompressed = 0;
    while (index_decompressed < length) {
        size_t currentBlockSize = ((length - index_decompressed) > blockSize) ? blockSize : (length - index_decompressed);
        // FILE_DTYPE* dec_buf = (FILE_DTYPE*)malloc(currentBlockSize * sizeof(FILE_DTYPE));
        size_t nDec = TURBOP4_DEC(&compressed[index_compressed], currentBlockSize, decompressed);
        // printf("Done \n");
        linearSweepWrite(decompressed, currentBlockSize);
        index_decompressed += currentBlockSize;
        index_compressed += nDec;
        // free(dec_buf);
    }
    printf("Mean: %f \n", sum / (double)sampleSize);

    // size_t i = 0;

    // while (i < length) {
    //     size_t numDec = TURBOP4_DEC(&compressed[i], blockSize, decompressed); // Decompress window

    //     // Process elements in the decompressed window
    //     linearSweepAvg(decompressed, numDec);

    //     i += numDec;
    // }
}

int main() {
    // Seed random generator.
    srand(0);

    GDALDatasetH hDataset;
    GDALAllRegister();

    // Open the dataset
    hDataset = GDALOpen("/home/omar/diss/geotiffs/JRC_TMF_AnnualChange_v1_1990_AFR_ID16_S10_E10.tif", GA_ReadOnly);
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

    int nXSize = GDALGetRasterBandXSize(hBand);
    int nYSize = GDALGetRasterBandYSize(hBand);
    // int nXSize = 1000;
    // int nYSize = 1000;

    // Read the raster data
    FILE_DTYPE *pafScanline = (FILE_DTYPE *) CPLMalloc(sizeof(FILE_DTYPE) * nXSize * nYSize);
    if (GDALRasterIO(hBand, GF_Read, 0, 0, nXSize, nYSize, 
                     pafScanline, nXSize, nYSize, FILE_DTYPE_GDAL, 0, 0) != CE_None) {
        printf("RasterIO read error\n");
        CPLFree(pafScanline);
        GDALClose(hDataset);
        return 1;
    }

    // Process into a 2D array.
    FILE_DTYPE** raster2D = create2DGridFrom1DArray(pafScanline, nXSize, nYSize);

    CPLFree(pafScanline);

    #if COMPRESS
        size_t blockSize = 96000; // Size of decompression window
        size_t length = nXSize * nYSize;

        uint8_t* compressedData;
        size_t compressedSize;

        compressArray(pafScanline, length, &compressedData, &compressedSize);
        printf("Block size %lu total size %lu compressed size %lu compression factor %f \n", blockSize, length * sizeof(FILE_DTYPE), compressedSize, (float)compressedSize / (float)(length * sizeof(FILE_DTYPE)));

        FILE_DTYPE* decompressed = (FILE_DTYPE*)malloc(blockSize * sizeof(FILE_DTYPE) + 32 * 4);

    #endif

    clock_t begin = clock();
    for (int rep = 0; rep < 1; rep++) {
        #if COMPRESS
            decompressAndProcessWindow(compressedData, decompressed, blockSize, nXSize * nYSize);
        #else
            // sum = 0;
            // sampleSize = 0;
            applyWindowedSweepRaster(raster2D, nXSize, nYSize, 1);
            // linearSweep(pafScanline, nXSize * nYSize);
            // printf("Mean: %f \n", sum / (double)sampleSize);
        #endif
    }
    clock_t end = clock();
    double time_spent = (double)(end - begin) / CLOCKS_PER_SEC;
    printf("Time to decompress and process window (window sweep): %f\n", time_spent);

    #if COMPRESS
        free(compressedData);
        free(decompressed);
    #endif

    for(int i = 0; i < nYSize; i++)
        free(raster2D[i]);
    free(raster2D);

    // Cleanup
    // CPLFree(pafScanline);
    GDALClose(hDataset);

    return 0;
}
