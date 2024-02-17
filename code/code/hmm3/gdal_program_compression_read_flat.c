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
// #define P4NENC_BOUND(n) ((n+16)/32+(n+16)*sizeof(FILE_DTYPE))
#define IN_FILE_TIF "/home/omar/diss/geotiffs/2656.tif"
#define OUT_FILE_TXT "raw/2656.txt"
// #define P4NENC_BOUND(n) (n*sizeof(FILE_DTYPE))

#define XDIV 1
#define YDIV 1

#define FILE_DTYPE_GDAL GDT_Byte
#define FILE_DTYPE uint8_t
#define TURBOP4_ENC p4nenc8 // p4nenc16 p4nenc128v16
#define TURBOP4_DEC p4ndec8 // p4ndec16 p4ndec128v16
#define P4NENC_BOUND(n) ((n+8)/8+(n+32)*sizeof(FILE_DTYPE))

#define NUM_RAND_ACCESS 10000
// #define P4NENC_BOUND(n) (n*sizeof(FILE_DTYPE))

#define COMPRESS true

#define CBUF(_n_) (((size_t)(_n_))*5/3+1024*1024)

int closest_multiple_less_than(int value) {
    return (value / 64) * 64;
}

int roundUp(int numToRound, int multiple) 
{
    // assert(multiple);
    return ((numToRound + multiple - 1) / multiple) * multiple;
}

typedef struct CompressedNode {
    uint8_t* compressedData;                // Pointer to compressed data
    size_t compressedSize;                  // Size of the compressed data
    size_t uncompressedSize;
    struct CompressedNode* next;  // Pointer to the next node
} CompressedNode;

typedef struct {
    CompressedNode* head;  // Pointer to the first node
    size_t totalSize;      // Total size of all compressed data
} CompressedDataList;

typedef struct {
    uint8_t* flattenedData;   // Pointer to the flattened data array
    size_t* startIndexes;     // Array of start indexes for each block in flattenedData
    size_t numBlocks;         // Number of blocks (or nodes) in the original list
    size_t totalSize;
} FlattenedData;

FlattenedData flattenLinkedList(CompressedDataList* list) {
    FlattenedData result = {0};
    size_t totalSize = 0;
    size_t numBlocks = 0;

    // Calculate total size and count blocks
    for (CompressedNode* node = list->head; node != NULL; node = node->next) {
        totalSize += node->compressedSize;
        numBlocks++;
    }

    // Allocate memory for the flattened data and start indexes
    result.flattenedData = (uint8_t*)malloc(totalSize);
    result.startIndexes = (size_t*)malloc(numBlocks * sizeof(size_t));
    result.numBlocks = numBlocks;
    result.totalSize = totalSize;
    if (!result.flattenedData || !result.startIndexes) {
        // Handle memory allocation failure
        free(result.flattenedData);
        free(result.startIndexes);
        return result;
    }

    // Copy data from each node to the flattened array
    size_t currentOffset = 0;
    size_t index = 0;
    for (CompressedNode* node = list->head; node != NULL; node = node->next) {
        memcpy(&result.flattenedData[currentOffset], node->compressedData, node->compressedSize);
        result.startIndexes[index++] = currentOffset;
        currentOffset += node->compressedSize;
    }

    return result;
}


CompressedNode* createCompressedNode(FILE_DTYPE* data, size_t dataSize) {
    CompressedNode* node = (CompressedNode*)malloc(sizeof(CompressedNode));
    if (!node) {
        fprintf(stderr, "Failed to allocate memory for node\n");
        return NULL;
    }

    // Compress data here and store in node
    node->compressedData = (uint8_t*)malloc(P4NENC_BOUND(dataSize));
    if (!node->compressedData) {
        fprintf(stderr, "Failed to allocate memory for compressed data\n");
        free(node);
        return NULL;
    }

    node->compressedSize = TURBOP4_ENC(data, dataSize, node->compressedData); // Compress
    // free(node->compressedData);
    node->compressedData = realloc(node->compressedData, node->compressedSize);
    node->uncompressedSize = dataSize;
    // printf("C %f \n", node->compressedSize / (double)(node->uncompressedSize * sizeof(FILE_DTYPE)));
    node->next = NULL;
    return node;
}

// Free the entire list
void freeLinkedList(CompressedDataList* list) {
    CompressedNode* current = list->head;
    while (current) {
        CompressedNode* next = current->next;
        free(current->compressedData);
        free(current);
        current = next;
    }
}


void buildLinkedList(CompressedNode** head, FILE_DTYPE* uncompressedData, size_t totalSize, size_t blockSize) {
    clock_t begin = clock();
    size_t offset = 0;
    CompressedNode *current = NULL, *previous = NULL;

    while (offset < totalSize) {
        size_t currentBlockSize = ((totalSize - offset) > blockSize) ? blockSize : (totalSize - offset);
        current = createCompressedNode(&uncompressedData[offset], currentBlockSize);

        if (!*head) {
            *head = current; // Set head if it's the first node
        } else {
            previous->next = current; // Link the new node
        }
        previous = current;
        offset += currentBlockSize;
    }
    clock_t end = clock();
    double time_spent = (double)(end - begin) / CLOCKS_PER_SEC;
    printf("Time to compress: %f\n", time_spent);
}



// linearly traverse through the n values
void linearSweep(FILE_DTYPE *data, int n) {
    for (int i = 0; i < n; i++) {
        FILE_DTYPE value = data[i];
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

// linearly traverse through the n values and increment them by 1, bounding the result to [0,128).
void linearSweepWrite(FILE_DTYPE *data, int n) {
    for (int i = 0; i < n; i++) {
        data[i] = ((int)(data[i] + 1)) % 128;
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
        data[randomIndex] = (int)(data[randomIndex] + 1) % 128;
    }
}


// void compressArray(FILE_DTYPE* input, size_t length, uint8_t** compressed, size_t* compressedSize) {
//     *compressed = (uint8_t*)malloc(P4NENC_BOUND(length)); // Allocate memory
//     *compressedSize = TURBOP4_ENC(input, length, *compressed); // Compress
// }

void decompressAndProcessWindow(FlattenedData* flattened, FILE_DTYPE* decompressed, size_t decompressedSize) {
    for (size_t i = 0; i < flattened->numBlocks; i++) {
        size_t startIndex = flattened->startIndexes[i];
        size_t blockSize = (i < flattened->numBlocks - 1) ? 
                           (flattened->startIndexes[i + 1] - startIndex) :
                           (flattened->totalSize - startIndex);

        // Access the block at flattened->flattenedData + startIndex
        uint8_t* block = flattened->flattenedData + startIndex;

        // Decompress the block
        TURBOP4_DEC(block, decompressedSize, decompressed);

        // Process the block
        // For example, just printing the first byte of each block
        linearSweep(decompressed, decompressedSize);
    }
}

FILE_DTYPE daUncomp(FILE_DTYPE* uncomp, size_t i) {
    return uncomp[i];
}

FILE_DTYPE daComp(uint8_t* comp, uint16_t* off, size_t i, size_t blockSize, size_t n, FILE_DTYPE* dbuf) {
    size_t block = i / blockSize;
    size_t blockIndex = i % blockSize;
    size_t blockBegin = off[block];
    size_t blockLen = fmin(n - i, blockSize);
    printf("dacomp %lu %lu %lu %lu \n", block, blockIndex, blockBegin, blockLen);
    TURBOP4_DEC(&comp[blockBegin], blockSize, dbuf);
    printf("s\n");
    return dbuf[blockIndex];
}

uint8_t* compress(FILE_DTYPE* data, int n, int* compSize) {
    uint8_t* tmp = (uint8_t*)malloc(P4NENC_BOUND(n));
    *compSize = TURBOP4_ENC(data, n, tmp);
    uint8_t* compressed = (uint8_t*)malloc(*compSize);
    memcpy(compressed, tmp, *compSize);
    printf("Comp size: %d, ratio = %f \n", *compSize, (float)*compSize / (float)(n * sizeof(FILE_DTYPE)));
    free(tmp);
    return compressed;
    // realloc(compressed, compSize);
}

void writeArrayToFile(FILE_DTYPE *pafScanline, size_t length, const char *filename) {
    FILE *file = fopen(filename, "w");
    if (file == NULL) {
        perror("Error opening file");
        exit(1);
    }

    for (size_t i = 0; i < length; i++) {
        fprintf(file, "%d,", pafScanline[i]);
    }

    fclose(file);
}

int main() {
    // Seed random generator.
    srand(0);

    GDALDatasetH hDataset;
    GDALAllRegister();

    // Open the dataset
    hDataset = GDALOpen(IN_FILE_TIF, GA_ReadOnly);
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

    int nXSize = GDALGetRasterBandXSize(hBand) / XDIV;
    int nYSize = GDALGetRasterBandYSize(hBand) / YDIV;
    // int nXSize = 1000;
    // int nYSize = 1000;

    // Read the raster data
    clock_t begin = clock();
    FILE_DTYPE *pafScanline = (FILE_DTYPE *) CPLMalloc(sizeof(FILE_DTYPE) * nXSize * nYSize);
    if (GDALRasterIO(hBand, GF_Read, 0, 0, nXSize, nYSize, 
                     pafScanline, nXSize, nYSize, FILE_DTYPE_GDAL, 0, 0) != CE_None) {
        printf("RasterIO read error\n");
        CPLFree(pafScanline);
        GDALClose(hDataset);
        return 1;
    }
    clock_t end = clock();
    double time_spent = (double)(end - begin) / CLOCKS_PER_SEC;
    printf("Time to load file: %f\n", time_spent);

    size_t blockSize = 128; // Size of decompression window
    size_t length = nXSize * nYSize;

    size_t numWindows = (length + blockSize - 1) / blockSize;

    size_t blockSizeLastWindow = (int)length - ((int)numWindows - 1) * (int)blockSize;

    size_t numRandomOperationsInWindow = 5000;

    // generate random indexes
    int randomIndexes[numRandomOperationsInWindow];
    int randomIndexesLastWindow[numRandomOperationsInWindow];
    for (int i = 0; i < numRandomOperationsInWindow; i++) {
        randomIndexes[i] = rand() % blockSize;
    }
    for (int i = 0; i < numRandomOperationsInWindow; i++) {
        randomIndexesLastWindow[i] = rand() % blockSizeLastWindow;
    }

    int* randomWindows = (int*)malloc(NUM_RAND_ACCESS * sizeof(int));
    for (int i = 0; i < NUM_RAND_ACCESS; i++) {
        randomWindows[i] = rand() % numWindows;
    }

    // WRITE TO FILE
    // writeArrayToFile(pafScanline, length, OUT_FILE_TXT);
    // CPLFree(pafScanline);
    // GDALClose(hDataset);
    // return;

    // COMPRESS AND CHECK EQUAL
    // uint8_t* out = (uint8_t*)malloc(CBUF(length));
    // uint8_t* cpy = (uint8_t*)malloc(length);
    // unsigned l=trlec(pafScanline, length, out);
    // trled(out, l, cpy, length);
    // printf("Compressed size: %d\n", l);
    // for (int i = 0; i < length; i++) {
    //     if (pafScanline[i] != cpy[i]) {
    //         printf("ERROR @ %d\n", i);
    //     }
    // }
    // CPLFree(pafScanline);
    // GDALClose(hDataset);
    // return;

    //compute avg
    sum = 0;
    sampleSize = 0;
    linearSweepAvg(pafScanline, nXSize * nYSize);
    printf("Actual mean: %f \n", sum / (double)sampleSize);

    // FILE_DTYPE* decbuf = (FILE_DTYPE*)malloc(numWindows * (blockSize + 32) * sizeof(FILE_DTYPE));
    FILE_DTYPE* decbuf = (FILE_DTYPE*)malloc((blockSize + 32) * sizeof(FILE_DTYPE));

    // Compress the data
    #if COMPRESS
                int compSize;
                uint8_t* compressed = compress(pafScanline, nXSize * nYSize, &compSize);

                // FILE_DTYPE* decompressed = (FILE_DTYPE*)malloc((blockSize + 32) * sizeof(FILE_DTYPE));

                // uint8_t** compWindows = (uint8_t**)malloc(numWindows * sizeof(uint8_t*));

                uint32_t* windowCompressedSizes = (uint32_t*)malloc(numWindows * sizeof(uint32_t));

                printf("numWindows: %d \n", numWindows);

                // Decompress the data in windows and compute the mean.
                // sum = 0;
                // sampleSize = 0;
                int windowIndex = 0;
                int i = 0;
                int decI = 0;
                // begin = clock();
                // // printf("Decode i = %d, decI = %d, windowIndex = %d\n", i, decI, windowIndex);
                while (i < length) {
                    // Read from i to i + blockSize - 1
                    int windowSize = (i + blockSize - 1 >= length) ? length - i : blockSize;
                    // printf("Decode i = %d, decI = %d, windowIndex = %d\n, windowSize = %d\n", i, decI, windowIndex, windowSize);

                    int numDec = TURBOP4_DEC(&compressed[decI], blockSize, decbuf);
                    // linearSweepAvg(decompressed, windowSize);
                    windowCompressedSizes[windowIndex++] = decI;
                    // int sizeCompWindow = roundUp(numDec, 1);
                    // compWindows[windowIndex] = (uint8_t*)malloc(numDec);
                    // memcpy(compWindows[windowIndex], &compressed[decI], numDec);
                    // windowCompressedSizes[windowIndex] = decI;
                    // windowIndex++;
                    decI += numDec;
                    i += windowSize;
                }
                // end = clock();
                // free(compressed);
                // time_spent = (double)(end - begin) / CLOCKS_PER_SEC;
                // printf("Time to decompress and process window: %f\n", time_spent);

                // printf("Actual compressed: %f \n", sum / (double)sampleSize);
    #endif
    
    #if !COMPRESS
        
        for (int rep = 0; rep < 10; rep++) {
            for (int i = 0; i < length; i++) {
                pafScanline[i] = pafScanline[i];
            }
            for (int i = 0; i < blockSize + 32; i++) {
                decbuf[i] = decbuf[i];
            }
            for (int i = 0; i < NUM_RAND_ACCESS; i++) {
                randomWindows[i] = randomWindows[i];
            }
        }
        printf("Cached.\n");
        begin = clock();
        // Perform random accesses
        // float sum = 0;
        for (int wi = 0; wi < NUM_RAND_ACCESS; wi++) {
            // this window has indexes window * windowSize to (window + 1) * windowSize - 1
            int window = randomWindows[wi];
            size_t blockLen = window == numWindows - 1 ? length - (window * blockSize) : blockSize;
            // load the window into decbuf
            // for (int i = 0; i < blockLen; i++) {
            //     // decbuf[window * blockSize + i] = pafScanline[window * blockSize + i];
            //     decbuf[i] = pafScanline[window * blockSize + i];
            // }
            // FILE_DTYPE decbuff[(blockSize + 32) * sizeof(FILE_DTYPE)];
            memcpy(decbuf, &pafScanline[window * blockSize], blockLen);

            decbuf[blockSize / 2];

            // we have decoded the window. read it.
            // for (int i = 0; i < blockLen; i++) {
            //     decbuf[i] += 1;
            // }

            // // re-encode
            // memcpy(&pafScanline[window * blockSize], decbuf, blockLen);


            // TURBOP4_DEC(compWindows[window], blockLen, decompressed);


            // if (window == numWindows - 1) {
            //     for (int i = 0; i < numRandomOperationsInWindow; i++) {
            //         pafScanline[window * blockSize + randomIndexesLastWindow[i]];
            //     }
            // }
            // else {
            //     for (int i = 0; i < numRandomOperationsInWindow; i++) {
            //         pafScanline[window * blockSize + randomIndexes[i]];
            //     }
            // }
            // for (int i = 0; i < blockLen; i++) {
            //     size_t indexInWindow = rand() % blockLen;
            //     pafScanline[window * blockSize + indexInWindow];
            // }
            // int i = rand() % length;
            // daUncomp(pafScanline, i);
        }
        // linearSweep(pafScanline, length);
        end = clock();
        time_spent = (double)(end - begin) / CLOCKS_PER_SEC;
        printf("Random access time uncompressed: %f\n", time_spent);
    #endif
    

    CPLFree(pafScanline);

    // free(windowCompressedSizes);

    #if COMPRESS
                int windowLocs[NUM_RAND_ACCESS];
                for (int wi = 0; wi < NUM_RAND_ACCESS; wi++) {
                    int window = randomWindows[wi];
                    windowLocs[wi] = windowCompressedSizes[window];
                }
                free(windowCompressedSizes);

                // load the compressed data into cache
                for (int rep = 0; rep < 1000; rep++) {
                    // for (int w = 0; w < numWindows; w++) {
                    //     uint16_t windowSize = windowCompressedSizes[w];
                    //     for (int i = 0; i < windowSize; i++) {
                    //         compWindows[w][i] = compWindows[w][i];
                    //     }
                    // }
                    for (int i = 0; i < compSize; i++) {
                        compressed[i] = compressed[i];
                    }
                    for (int i = 0; i < blockSize + 32; i++) {
                        decbuf[i] = decbuf[i];
                    }
                    // for (int i = 0; i < numWindows; i++) {
                    //     windowCompressedSizes[i] = windowCompressedSizes[i];
                    // }
                    for (int i = 0; i < NUM_RAND_ACCESS; i++) {
                        randomWindows[i] = randomWindows[i];
                        windowLocs[i] = windowLocs[i];
                    }
                }



                // free(windowCompressedSizes);
                printf("Cached.\n");

                begin = clock();
                // float sum = 0;
                // for (int rep = 0; rep < length; rep++) {
                //     size_t window = rand() % numWindows;
                //     // this window has indexes window * windowSize to (window + 1) * windowSize - 1
                //     size_t blockLen = window == numWindows - 1 ? length - (window * blockSize) : blockSize;
                //     // size_t blockLen = window == numWindows - 1 ? length - (window * blockSize) : blockSize;
                //     // printf("w %lu\n", window);
                //     TURBOP4_DEC(compWindows[window], blockSize, decompressed);
                //     // printf("s\n");
                //     for (int repin = 0; repin < 1; repin++) {
                //         size_t indexInWindow = rand() % blockLen;
                //         // decompressed[indexInWindow];
                //     }




                //     // size_t i = rand() % length;
                //     // // printf("ra i = %lu\n", i);

                //     // size_t block = i / blockSize;
                //     // size_t blockIndex = i % blockSize;
                //     // // size_t blockBegin = windowCompressedSizes[block];
                //     // size_t blockLen = length - i < blockSize ? length - i : blockSize;
                //     // // printf("dacomp %lu %lu %lu %lu \n", block, blockIndex, blockBegin, blockLen);
                //     // TURBOP4_DEC(compWindows[block], blockLen, decompressed);
                //     // // TURBOP4_DEC(&compressed[blockBegin], blockSize, decompressed);
                //     // decompressed[blockIndex];

                //     // daComp(compressed, windowCompressedSizes, i, blockSize, length, decompressed);
                // }
                // int ci = 0;
                for (int wi = 0; wi < NUM_RAND_ACCESS; wi++) {
                    int window = randomWindows[wi];
                    // size_t window = rand() % numWindows;
                    // read window
                    
                    size_t blockLen = window == numWindows - 1 ? length - (window * blockSize) : blockSize;
                    // int nDec = TURBOP4_DEC(&compressed[ci], blockSize, &decbuf[window * blockSize]);
                    // FILE_DTYPE decbuff[(blockSize + 32) * sizeof(FILE_DTYPE)];
                    // p4dec8(&compressed[windowCompressedSizes[window]], blockSize, decbuf);
                    p4dec8(&compressed[windowLocs[wi]], blockSize, decbuf);
                    // ci += nDec;

                    decbuf[blockSize / 2];

                    // we have decoded the window. read it.
                    // for (int i = 0; i < blockLen; i++) {
                    //     decbuf[i] += 1;
                    // }

                    // read values from window
                    // if (window == numWindows - 1) {
                    //     for (int i = 0; i < numRandomOperationsInWindow; i++) {
                    //         decompressed[randomIndexesLastWindow[i]];
                    //     }
                    // }
                    // else {
                    //     for (int i = 0; i < numRandomOperationsInWindow; i++) {
                    //         decompressed[randomIndexes[i]];
                    //     }
                    // }
                    // // for (int i = 0; i < blockLen; i++) {
                    // //     // size_t indexInWindow = rand() % blockLen;
                    // //     decompressed[i];
                    // // }
                }
                end = clock();
                time_spent = (double)(end - begin) / CLOCKS_PER_SEC;
                printf("Random access time compressed: %f\n", time_spent);
    #endif

    // for (int window = 0; window < numWindows; window++) {
    //     // Mean of subsets
    //     // Let's do the mean of the 5th window. blockSize * 5 to 6 * blockSize - 1
    //     sum = 0;
    //     sampleSize = 0;
    //     linearSweepAvg(&pafScanline[blockSize * window], blockSize);
    //     float meanActual = sum / (float)sampleSize;
    //     // printf("Actual mean block %d: %f\n", window, sum / (double)sampleSize);
    //     sum = 0;
    //     sampleSize = 0;
    //     // printf("block %d\n", windowCompressedSizes[window]);
    //     TURBOP4_DEC(&compressed[windowCompressedSizes[window]], blockSize, decompressed);
    //     linearSweepAvg(decompressed, blockSize);
    //     float meanCompressed = sum / (float)sampleSize;
    //     if (meanActual != meanCompressed) {
    //         printf("INCONSISTENCY\n");
    //     }
    //     // printf("Compressed mean block %d: %f\n", window, sum / (double)sampleSize);
    // }


    #if COMPRESS
        // free(windowCompressedSizes);
        // free(decompressed);
        free(compressed);
        // for (int w = 0; w < numWindows; w++) {
        //     free(compWindows[w]);
        // }
        // free(compWindows);
    #endif

    free(decbuf);

    free(randomWindows);
    

    // Cleanup
    // CPLFree(pafScanline);
    GDALClose(hDataset);

    // CPLFree(pafScanline);

    return 0;
}
