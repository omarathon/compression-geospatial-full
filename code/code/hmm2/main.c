#include "types.h"
#include "gdal.h"
#include "cpl_conv.h" // For CPLMalloc and CPLFree
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <math.h>

#define GDAL_DTYPE GDT_Byte
#define BLOCK_SIZE 512 // 5 for random accesses, 16 for window (16 window is faster, MAKE SURE TO CLOSE EVERYTHING!!. 32 and 64 seem to be faster too)
#define USE_GRID true
#define COMPRESS_GRID false
#define WINDOW_RADIUS 10000
#define NUM_RANDOM_SAMPLE 1000
#define NUM_RANDOM_SAMPLE_IN_BLOCK 1
#define CACHE_THRASH_DIST 1024

#define CBUF(_n_) (((size_t)(_n_))*5/3+1024*1024)
#define RLE8  0xdau

typedef struct {
    RASTER_DTYPE* data;
    COMPRESS_DTYPE* compressedData;
    int width, height;
    int compressedSize;
} RasterBlock;

int randomBlockPoints[NUM_RANDOM_SAMPLE];
int randomUncompressedPoints[NUM_RANDOM_SAMPLE];


int randomPointsInBlock[NUM_RANDOM_SAMPLE_IN_BLOCK];

// l=bitnfpack8( in, m, out),         n,l, bitnfunpack8( out, m, cpy)

void compressBlock(RasterBlock* block, RASTER_DTYPE* blockData) {
    // COMPRESS_DTYPE compressed[P4NENC_BOUND(block->width * block->height)];
    // block->compressedSize = TURBOP4_ENC(blockData, block->width * block->height, compressed);
    // // printf("Compression ratio: %f\n", (float)blocks[i][j].compressedSize / (float)(blocks[i][j].height * blocks[i][j].width * sizeof(RASTER_DTYPE)));
    // block->compressedData = (COMPRESS_DTYPE*)malloc(block->compressedSize);
    // memcpy(block->compressedData, compressed, block->compressedSize);

    // COMPRESS_DTYPE compressed[CBUF(block->width * block->height)];
    // block->compressedSize = trlec(blockData, block->width * block->height, compressed);
    // block->compressedData = (COMPRESS_DTYPE*)malloc(block->compressedSize);
    // memcpy(block->compressedData, compressed, block->compressedSize);
    // free(compressed);

    COMPRESS_DTYPE compressed[CBUF(block->width * block->height)];
    block->compressedSize = srlec8(blockData, block->width * block->height, compressed, RLE8);
    // block->compressedSize = bitnfpack8(blockData, block->width * block->height, compressed);    
    block->compressedData = (COMPRESS_DTYPE*)malloc(block->compressedSize);
    memcpy(block->compressedData, compressed, block->compressedSize);
    // free(compressed);

}

void decompressBlock(RasterBlock* block, RASTER_DTYPE* decbuf) {
    //  printf("dd\n");
    // trled(block->compressedData, block->compressedSize, decbuf, block->width * block->height);
    // TURBOP4_DEC(block->compressedData, block->width * block->height, decbuf);
    //  printf("d\n");

    srled8(block->compressedData, block->compressedSize, decbuf, block->width * block->height, RLE8);
    // bitnfunpack8(block->compressedData, block->width * block->height, decbuf);
}

RasterBlock* splitIntoBlocks(RASTER_DTYPE** raster, int rasterWidth, int rasterHeight, int blockSize, int* blockRows, int* blockCols) {
    *blockRows = (rasterHeight + blockSize - 1) / blockSize; // Ceiling division
    *blockCols = (rasterWidth + blockSize - 1) / blockSize;

    size_t dataSize = 0;

    RasterBlock* blocks = (RasterBlock*)malloc(*blockRows * *blockCols * sizeof(RasterBlock));
    printf("Blocks storage without data: %lu %lu \n", *blockRows * *blockCols * sizeof(RasterBlock), sizeof(RasterBlock));

    // for (int i = 0; i < *blockRows; i++) {
    // }
    for (int i = 0; i < *blockRows; i++) {
        // blocks[i] = (RasterBlock*)malloc(*blockCols * sizeof(RasterBlock));
        // RasterBlock* blockRow = blocks[i];
        for (int j = 0; j < *blockCols; j++) {
            int p = i * *blockCols + j;
            RasterBlock* block = &blocks[p];
            block->width = (j < *blockCols - 1) ? blockSize : (rasterWidth - j * blockSize);
            block->height = (i < *blockRows - 1) ? blockSize : (rasterHeight - i * blockSize);

            RASTER_DTYPE blockData[block->height * block->width];
            // blocks[i][j].data = (RASTER_DTYPE*)malloc(blocks[i][j].height * blocks[i][j].width * sizeof(RASTER_DTYPE));
            for (int y = 0; y < block->height; y++) {
                for (int x = 0; x < block->width; x++) {
                    blockData[y * block->width + x] = raster[i * blockSize + y][j * blockSize + x];
                }
            }
            // Compress blocks[i][j]
            if (COMPRESS_GRID) {
                compressBlock(block, blockData);
                dataSize += block->compressedSize;
            }
            else {
                block->data = (RASTER_DTYPE*)malloc(block->height * block->width * sizeof(RASTER_DTYPE));
                memcpy(block->data, blockData, block->height * block->width * sizeof(RASTER_DTYPE));
                dataSize += block->height * block->width * sizeof(RASTER_DTYPE);
            }
        }
    }
    printf("Done building. Data size: %lu\n", dataSize);
    return blocks;
}

void randomSample(RasterBlock* blocks, int blockRows, int blockCols, int blockSize, int width, int height, int sample, RASTER_DTYPE* decbuf) {
    int blockPos = randomBlockPoints[sample];
    RasterBlock* block = blocks + blockPos;
    // if (COMPRESS_GRID) {
    //     int dataPos = rand() % block->compressedSize;
    //     block->compressedData[dataPos];
    // }
    // else {
    //     int dataX = rand() % block->width;
    //     int dataY = rand() % block->height;
    //     block->data[dataY * block->width + dataX];
    // }
    // sample a random point from the block
    RASTER_DTYPE decoded[(BLOCK_SIZE * BLOCK_SIZE + 32)];

    if (COMPRESS_GRID) {
        decompressBlock(block, decoded);
        // trled(block->compressedData, block->compressedSize, decbuf, block->width * block->height);
    }
    else {
        memcpy(decoded, block->data, block->width * block->height * sizeof(RASTER_DTYPE));
    }

    decoded[block->width * block->height / 2] += 1;
    // for (int i = 0; i < NUM_RANDOM_SAMPLE_IN_BLOCK; i+=1) {
    //     decoded[i * CACHE_THRASH_DIST] += 1;
    // }

    // for (int i = 0; i < NUM_RANDOM_SAMPLE_IN_BLOCK; i++) {
    //     RASTER_DTYPE killcache[0];
    //     killcache[0];
    //     // int pb =  randomPointsInBlock[i] < block->width * block->height ? randomBlockPoints[i] : block->width * block->height - 1;
    //     // decoded[pb]++;
    // }
    // for (int i = 0; i < block->width * block->height; i++) {
    // decoded[block->width * block->height]++;
    // }

    // Recompress
    if (COMPRESS_GRID) {
        compressBlock(block, decoded);
    }
    else {
        memcpy(block->data, decoded, block->width * block->height * sizeof(RASTER_DTYPE));
    }


    // decbuf[block->width * block->height - 1];

    // Operate on decoded data
    // for (int i = 0; i < block->width * block->height; i++) {
    //     decbuf[0];
    // }

    // if (COMPRESS_GRID) {
    //     // COMPRESS_DTYPE compressed[P4NENC_BOUND(block->width * block->height)];
    //     for (int rep = 0; rep < 1; rep++) {
    //         int dataX = rand() % block->width;
    //         int dataY = rand() % block->height;
    //         decbuf[dataY * block->width + dataX];
    //     }
    //     // int compSize = TURBOP4_ENC(decbuf, block->width * block->height, compressed);
    //     // // printf("Compression ratio: %f\n", (float)blocks[i][j].compressedSize / (float)(blocks[i][j].height * blocks[i][j].width * sizeof(RASTER_DTYPE)));
    //     // block->compressedData = (COMPRESS_DTYPE*)malloc(compSize);
    //     // memcpy(block->compressedData, compressed, compSize);
    // }
    // else {
    //     for (int rep = 0; rep < 1; rep++) {
    //         int dataX = rand() % block->width;
    //         int dataY = rand() % block->height;
    //         block->data[dataY * block->width + dataX];
    //     }
    // }
}

void readWindow(RasterBlock** blocks, int blockRows, int blockCols, int blockSize, int x, int y, int radius, RASTER_DTYPE* decbuf) {
    int windowSize = 2 * radius + 1;

    int startBlockRow = fmax(y - radius, 0) / blockSize;
    int endBlockRow = fmin(y + radius, blockRows * blockSize - 1) / blockSize;
    int startBlockCol = fmax(x - radius, 0) / blockSize;
    int endBlockCol = fmin(x + radius, blockCols * blockSize - 1) / blockSize;

    for (int blockRow = startBlockRow; blockRow <= endBlockRow; blockRow++) {
        for (int blockCol = startBlockCol; blockCol <= endBlockCol; blockCol++) {
            RasterBlock block = blocks[blockRow][blockCol];
            // Decompress block
            if (COMPRESS_GRID) {
                decompressBlock(&block, decbuf);
            }
            RASTER_DTYPE* data = COMPRESS_GRID ? decbuf : block.data;

            int overlapStartX = fmax(x - radius, blockCol * blockSize);
            int overlapEndX = fmin(x + radius, (blockCol + 1) * blockSize - 1);
            int overlapStartY = fmax(y - radius, blockRow * blockSize);
            int overlapEndY = fmin(y + radius, (blockRow + 1) * blockSize - 1);

            // for (int rep = 0; rep < 100; rep++) {
                for (int wx = overlapStartX; wx <= overlapEndX; wx++) {
                    for (int wy = overlapStartY; wy <= overlapEndY; wy++) {
                        int inBlockX = wx % blockSize;
                        int inBlockY = wy % blockSize;

                        if (inBlockY < block.height && inBlockX < block.width) {
                            int windowIndexY = wy - (y - radius);
                            int windowIndexX = wx - (x - radius);

                            if (windowIndexY >= 0 && windowIndexY < windowSize && windowIndexX >= 0 && windowIndexX < windowSize) {
                                // window[windowIndexY][windowIndexX] = data[inBlockY * block.width + inBlockX];
                                data[inBlockY * block.width + inBlockX];
                                // data[inBlockY * block.width + inBlockX];
                            }
                        }
                    }
                }
            // }
            // // re-compress block
            // if (COMPRESS_GRID) {
            //     COMPRESS_DTYPE compressed[P4NENC_BOUND(block.width * block.height)];
            //     int compSize = TURBOP4_ENC(data, block.width * block.height, compressed);
            //     block.compressedData = (COMPRESS_DTYPE*)malloc(compSize);
            //     memcpy(block.compressedData, compressed, compSize);
            // }
        }
    }
}

float applyWindowedReadGrid(RasterBlock* blocks, int blockRows, int blockCols, int blockSize, size_t width, size_t height, int radius, RASTER_DTYPE* decbuf) {
    printf("Applying windowed sweep on the quadtree.\n");
    // Allocate memory.
    // RASTER_DTYPE** window = (RASTER_DTYPE**)malloc((2 * radius + 1) * sizeof(RASTER_DTYPE*));
    // for (int i = 0; i < (2 * radius + 1); i++) {
    //     window[i] = (RASTER_DTYPE*)malloc((2 * radius + 1) * sizeof(RASTER_DTYPE));
    // }

    int x = width / 2;
    int y = height / 2;
    // Compute.
    for (int rep = 0; rep < NUM_RANDOM_SAMPLE; rep++) {
        // for (int x = 0; x < width; x += width / 3) {
            // for (int y = 0; y < height; y += height / 3) {
                //readWindow(blocks, blockRows, blockCols, blockSize, x, y, radius, decbuf);
                randomSample(blocks, blockRows, blockCols, blockSize, width, height, rep, decbuf);
            // }
        // }
    }
    

    float sum = 0.0;
    size_t count = 0;

    // Read the window from the uncompressed data.
    // readWindow(blocks, blockRows, blockCols, blockSize, x, y, radius, window, decbuf);

    // Process the values in the window.
    // for (int wy = -radius; wy <= radius; wy++) {
    //     for (int wx = -radius; wx <= radius; wx++) {
    //         int px = x + wx;
    //         int py = y + wy;
    //         if (px >= 0 && px < width && py >= 0 && py < height) {
    //             sum += window[wy + radius][wx + radius];
    //             count++;
    //         }
    //     }
    // }
    // Compute the average for the window
    float avg = sum / count;

    // Free memory.
    // for (int i = 0; i < (2 * radius + 1); i++) {
    //     free(window[i]);
    // }
    // free(window);
    // free(decbuf);

    // Return result.
    return avg;
}

void readWindowRaster(RASTER_DTYPE** raster, size_t width, size_t height, size_t x, size_t y, int radius, RASTER_DTYPE** window) {
    for (int rep = 0; rep < 100; rep++) {
        for (int wy = -radius; wy <= radius; wy++) {
            for (int wx = -radius; wx <= radius; wx++) {
                int px = x + wx;
                int py = y + wy;
                // Boundary check
                if (px >= 0 && px < width && py >= 0 && py < height) {
                    raster[py][px];
                    // window[wy + radius][wx + radius] = raster[py][px];
                }
            }   
        }
    }
}

float applyWindowedReadRaster(RASTER_DTYPE** rasterData, size_t width, size_t height, int radius) {
    printf("Applying windowed sweep on the raster.\n");
    // Allocate memory.
    // RASTER_DTYPE** window = (RASTER_DTYPE**)malloc((2 * radius + 1) * sizeof(RASTER_DTYPE*));
    // for (int i = 0; i < 2 * radius + 1; i++) {
    //     window[i] = (RASTER_DTYPE*)malloc((2 * radius + 1) * sizeof(RASTER_DTYPE));
    // }
    // // Compute.
    // int x = width / 2;
    // int y = height / 2;
    // // for (int x = 0; x < width; x += width / 10) {
    //     // for (int y = 0; y < height; y += height / 10) {
    //         readWindowRaster(rasterData, width, height, x, y, radius, window);
    //     // }
    // // }

    // float sum = 0.0;
    // size_t count = 0;
    // // Read the window from the uncompressed data.
    // // readWindowRaster(rasterData, width, height, x, y, radius, window);
    // // Process the values in the window.
    // // for (int wy = -radius; wy <= radius; wy++) {
    // //     for (int wx = -radius; wx <= radius; wx++) {
    // //         int px = x + wx;
    // //         int py = y + wy;
    // //         if (px >= 0 && px < width && py >= 0 && py < height) {
    // //             sum += window[wy + radius][wx + radius];
    // //             count++;
    // //         }
    // //     }
    // // }
    // // Compute the average for the window
    // float avg = sum / count;

    // // Free memory.
    // for (int i = 0; i < 2 * radius + 1; i++) {
    //     free(window[i]);
    // }
    // free(window);
    // // Return result.
    // return avg;
    
    for (int rep = 0; rep < NUM_RANDOM_SAMPLE; rep++) {
        int p = randomUncompressedPoints[rep];
        int x = p % width;
        int y = p / width;
        rasterData[y][x];
    }
    return 0;
}

float applyWindowedSweepRaster(RASTER_DTYPE** rasterData, size_t width, size_t height, int radius) {
    printf("Applying windowed sweep on the raster.\n");
    // Allocate memory.
    RASTER_DTYPE** window = (RASTER_DTYPE**)malloc((2 * radius + 1) * sizeof(RASTER_DTYPE*));
    for (int i = 0; i < 2 * radius + 1; i++) {
        window[i] = (RASTER_DTYPE*)malloc((2 * radius + 1) * sizeof(RASTER_DTYPE));
    }
    // Compute.
    float sumAvg = 0.0;
    size_t countAvg = 0;
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            float sum = 0.0;
            size_t count = 0;
            // Read the window from the uncompressed data.
            readWindowRaster(rasterData, width, height, x, y, radius, window);
            // Process the values in the window.
            for (int wy = -radius; wy <= radius; wy++) {
                for (int wx = -radius; wx <= radius; wx++) {
                    int px = x + wx;
                    int py = y + wy;
                    if (px >= 0 && px < width && py >= 0 && py < height) {
                        sum += window[wy + radius][wx + radius];
                        count++;
                    }
                }
            }
            // Compute the average for the window
            float avg = sum / count;
            sumAvg += avg;
            countAvg++;
        }
    }
    // Free memory.
    for (int i = 0; i < 2 * radius + 1; i++) {
        free(window[i]);
    }
    free(window);
    // Return result.
    return sumAvg / countAvg;
}

RASTER_DTYPE** create2DGridFrom1DArray(RASTER_DTYPE* flattenedArray, size_t width, size_t height) {
    RASTER_DTYPE** grid = (RASTER_DTYPE**)malloc(height * sizeof(RASTER_DTYPE*));
    for (int i = 0; i < height; i++) {
        grid[i] = (RASTER_DTYPE*)malloc(width * sizeof(RASTER_DTYPE));
        for (int j = 0; j < width; j++) {
            grid[i][j] = flattenedArray[i * width + j];
        }
    }
    return grid;
}

int main() {
    // Seed random generator.
    srand(0);

    GDALDatasetH hDataset;
    GDALAllRegister();

    // Open the dataset
    hDataset = GDALOpen("/home/omar/diss/geotiffs/2656.tif", GA_ReadOnly);
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

    // int nXSize = 10;
    // int nYSize = 10;

    // Read the raster data
    clock_t begin = clock();
    RASTER_DTYPE *pafScanline = (RASTER_DTYPE*) CPLMalloc(sizeof(RASTER_DTYPE) * nXSize * nYSize);
    if (GDALRasterIO(hBand, GF_Read, 0, 0, nXSize, nYSize, 
                     pafScanline, nXSize, nYSize, GDAL_DTYPE, 0, 0) != CE_None) {
        printf("RasterIO read error\n");
        CPLFree(pafScanline);
        GDALClose(hDataset);
        return 1;
    }
    clock_t end = clock();
    double time_spent = (double)(end - begin) / CLOCKS_PER_SEC;
    printf("Time to load file: %f\n", time_spent);

    size_t length = nXSize * nYSize;

    RASTER_DTYPE** raster2D = create2DGridFrom1DArray(pafScanline, nXSize, nYSize);
    CPLFree(pafScanline);

    #if USE_GRID
        int blockRows;
        int blockCols;
        printf("blocks\n");
        RasterBlock* blocks = splitIntoBlocks(raster2D, nXSize, nYSize, BLOCK_SIZE, &blockRows, &blockCols);
        printf("done\n");
        // free raster2D
        for (int i = 0; i < nYSize; i++) {
            free(raster2D[i]);
        }
        free(raster2D);

        for (int i = 0; i < NUM_RANDOM_SAMPLE; i++) {
            randomBlockPoints[i] = rand() % (blockCols * blockRows);
            // randomBlockY[i] = rand() % blockRows;
        }

        for (int i = 0; i < NUM_RANDOM_SAMPLE_IN_BLOCK; i++) {
            randomPointsInBlock[i] = rand() % (BLOCK_SIZE * BLOCK_SIZE);
        }

        printf("wtf\n");

        RASTER_DTYPE* decbuf = (RASTER_DTYPE*)malloc((BLOCK_SIZE * BLOCK_SIZE + 32) * sizeof(RASTER_DTYPE));

        // place all the data in the cache
        for (int rep = 0; rep < 10; rep++) {
            for (int row = 0; row < blockRows; row++) {
                for (int col = 0; col < blockCols; col++) {
                    RasterBlock* block = &blocks[row * blockCols + col];
                    for (int i = 0; i < (COMPRESS_GRID ? block->compressedSize : block->width * block->height); i++) {
                        if (COMPRESS_GRID) {
                            block->compressedData[i] = block->compressedData[i];
                        }
                        else {
                            block->data[i] = block->data[i];
                        }
                        block->height = block->height;
                        block->width = block->width;
                    }
                }
            }
            // for (int i = 0; i < BLOCK_SIZE * BLOCK_SIZE + 32; i++) {
            //     decbuf[i];
            // }
        }
        printf("Begin\n");
        begin = clock();
        float result = applyWindowedReadGrid(blocks, blockRows, blockCols, BLOCK_SIZE, nXSize, nYSize, WINDOW_RADIUS, decbuf);
        printf("Result: %f\n", result);
        end = clock();
        time_spent = (double)(end - begin) / CLOCKS_PER_SEC;
        printf("Time to decompress and process window: %f\n", time_spent);
        // free blocks
        for (int i = 0; i < blockRows; i++) {
            for (int j = 0; j < blockCols; j++) {
                if (COMPRESS_GRID) {
                    free(blocks[i * blockCols + j].compressedData);
                }
                else {
                    free(blocks[i * blockCols + j].data);
                }
            }
            // free(blocks[i]);
        }
        free(blocks);
        free(decbuf);
    #else
        for (int i = 0; i < NUM_RANDOM_SAMPLE; i++) {
            randomUncompressedPoints[i] = rand() % (nXSize * nYSize); 
        }
        begin = clock();
        float result = applyWindowedReadRaster(raster2D, nXSize, nYSize, WINDOW_RADIUS);
        printf("Result: %f\n", result);
        end = clock();
        time_spent = (double)(end - begin) / CLOCKS_PER_SEC;
        printf("Time to decompress and process window: %f\n", time_spent);

        // free raster2D
        for (int i = 0; i < nYSize; i++) {
            free(raster2D[i]);
        }
        free(raster2D);
    #endif

    printf("Done \n");

    GDALClose(hDataset);

    return 0;
}