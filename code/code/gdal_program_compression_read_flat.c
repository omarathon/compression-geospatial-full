#include "gdal.h"
#include "cpl_conv.h" // For CPLMalloc and CPLFree
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "ic.h"

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
// #define P4NENC_BOUND(n) (n*sizeof(FILE_DTYPE))

#define FILE_DTYPE_GDAL GDT_Byte
#define FILE_DTYPE uint8_t
#define TURBOP4_ENC p4nenc8 // p4nenc16 p4nenc128v16
#define TURBOP4_DEC p4ndec8 // p4ndec16 p4ndec128v16
#define P4NENC_BOUND(n) ((n+8)/8+(n+32)*sizeof(FILE_DTYPE))
// #define P4NENC_BOUND(n) (n*sizeof(FILE_DTYPE))

#define COMPRESS true

int closest_multiple_less_than(int value) {
    return (value / 64) * 64;
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
        data[i] = (data[i] + 1) % 128;
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

    size_t blockSize = closest_multiple_less_than(100000); // Size of decompression window
    size_t length = nXSize * nYSize;

    // compute avg
    // sum = 0;
    // sampleSize = 0;
    // linearSweepAvg(pafScanline, nXSize * nYSize);
    // printf("Actual mean: %f \n", sum / (double)sampleSize);

    // Create a linked list from the compressed data
    CompressedDataList compressedList = { .head = NULL, .totalSize = 0 };
    buildLinkedList(&compressedList.head, pafScanline, length, blockSize);

    CPLFree(pafScanline);

    FlattenedData flattened = flattenLinkedList(&compressedList);

    freeLinkedList(&compressedList);

    FILE_DTYPE* decompressed = (FILE_DTYPE*)malloc((blockSize + 32) * sizeof(FILE_DTYPE));

    begin = clock();
    for (int rep = 0; rep < 1; rep++) {
        // printf("Decompressing window \n");
        // sum = 0;
        // sampleSize = 0;
        decompressAndProcessWindow(&flattened, decompressed, blockSize);
        // printf("Mean: %f \n", sum / (double)sampleSize);
        // printf("Decompressed window \n");
    }
    end = clock();
    time_spent = (double)(end - begin) / CLOCKS_PER_SEC;
    printf("Time to decompress and process window: %f\n", time_spent);

    printf("Done \n");

    free(flattened.flattenedData);
    free(flattened.startIndexes);
    free(decompressed);

    // Cleanup
    // CPLFree(pafScanline);
    GDALClose(hDataset);

    return 0;
}
