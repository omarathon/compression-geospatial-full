#include "quadtree.h"
#include "gdal.h"
#include "cpl_conv.h" // For CPLMalloc and CPLFree
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <math.h>

#define GDAL_DTYPE GDT_Int16
#define MAX_NODES 100
#define BLOCK_SIZE 2 // 3
#define USE_QUAD_TREE true
#define COMPRESS_QUAD_TREE false
#define WINDOW_RADIUS 0

void findOverlappingNodes(QuadtreeNode* node, size_t windowTop, size_t windowBottom, size_t windowLeft, size_t windowRight, QuadtreeNode** overlappingNodes, size_t* count) {
    if (node == NULL) {
        return;
    }

    int nodeTop = node->y;
    int nodeLeft = node->x;
    int nodeBottom = node->y + node->height;
    int nodeRight = node->x + node->width;

    // Check if the node overlaps with the window
    bool overlaps = !(nodeLeft > windowRight || nodeRight < windowLeft ||
                      nodeTop > windowBottom || nodeBottom < windowTop);
    
    printf("overlaps %d node: t %d l %d b %d r %d, window: t %d l %d b %d r %d\n", overlaps, nodeTop, nodeLeft, nodeBottom, nodeRight, windowTop, windowLeft, windowBottom, windowRight);

    if (overlaps) {
        if (isLeafNode(node)) {
            overlappingNodes[(*count)++] = node;
        } else {
            // Recursively check children
            for (int i = 0; i < 4; i++) {
                findOverlappingNodes(node->children[i], windowTop, windowBottom, windowLeft, windowRight, overlappingNodes, count);
            }
        }
    }
}

void decompressNode(QuadtreeNode* node, RASTER_DTYPE* decompressBuffer) {
    TURBOP4_DEC(node->compressedData, node->uncompressedSize, decompressBuffer);
}

void readWindowQuadtree(QuadtreeNode* root, size_t x, size_t y, int radius, RASTER_DTYPE** window, RASTER_DTYPE* decompressBuffer) {
    // 1. Identify nodes overlapping with the window.
    // 2. For each node:
    //      Decompress -> read the points from the node within the window -> recompress

    // Find overlapping windows.
    int windowTop = y - radius < 0 ? 0 : y - radius;
    int windowBottom = y + radius >= root->height ? root->height - 1 : y + radius;
    int windowLeft = x - radius < 0 ? 0 : x - radius;
    int windowRight = x + radius >= root->width ? root->width - 1 : x + radius;
    size_t count = 0;
    QuadtreeNode* overlappingNodes[MAX_NODES];
    findOverlappingNodes(root, windowTop, windowBottom, windowLeft, windowRight, overlappingNodes, &count);
    
    printf("Reading window QuadTree x %lu y %lu\n", x, y);
    
    for (size_t ni = 0; ni < count; ni++) {
        QuadtreeNode* overlappingNode = overlappingNodes[ni];
        printf("o x %d y %d w %d h %d \n", overlappingNode->x, overlappingNode->y, overlappingNode->width, overlappingNode->height);

        if (overlappingNode->compress) {
            // Decompress the node.
            decompressNode(overlappingNode, decompressBuffer);
        }
        
        // Calculate the overlap between the node and the window.
        int nodeTop = overlappingNode->y;
        int nodeLeft = overlappingNode->x;
        int overlapTop = fmax(nodeTop, windowTop);
        int overlapBottom = fmin(nodeTop + overlappingNode->height, windowBottom);
        int overlapLeft = fmax(nodeLeft, windowLeft);
        int overlapRight = fmin(nodeLeft + overlappingNode->width, windowRight);

        printf("overlapTop %d overlapBottom %d overlapLeft %d overlapRight %d\n", overlapTop, overlapBottom, overlapLeft, overlapRight);

        // Iterate over each point in the overlap and copy to `window`
        for (int wy = overlapTop; wy < overlapBottom; wy++) {
            for (int wx = overlapLeft; wx < overlapRight; wx++) {
                int windowIndexY = wy - windowTop;
                int windowIndexX = wx - windowLeft;
                int nodeIndexY = wy - nodeTop;
                int nodeIndexX = wx - nodeLeft;

                printf("d %d\n", overlappingNode->uncompressedData[nodeIndexY * overlappingNode->width + nodeIndexX]);

                window[windowIndexY][windowIndexX] = 
                    overlappingNode->compress ? decompressBuffer[nodeIndexY * overlappingNode->width + nodeIndexX] : overlappingNode->uncompressedData[nodeIndexY * overlappingNode->width + nodeIndexX];
            }
        }
    }
}

float applyWindowedReadQuadtree(QuadtreeNode* root, size_t width, size_t height, int radius) {
    printf("Applying windowed sweep on the quadtree.\n");

    // Allocate memory.
    RASTER_DTYPE** window = (RASTER_DTYPE**)malloc((2 * radius + 1) * sizeof(RASTER_DTYPE*));
    for (int i = 0; i < (2 * radius + 1); i++) {
        window[i] = (RASTER_DTYPE*)malloc((2 * radius + 1) * sizeof(RASTER_DTYPE));
    }
    RASTER_DTYPE* decompressBuffer = (RASTER_DTYPE*)malloc((BLOCK_SIZE * BLOCK_SIZE + 32) * sizeof(RASTER_DTYPE));

    // Compute.
    int x = width / 2;
    int y = height / 2;

    float sum = 0.0;
    size_t count = 0;

    // Read the window from the uncompressed data.
    readWindowQuadtree(root, x, y, radius, window, decompressBuffer);

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

    // Free memory.
    for (int i = 0; i < (2 * radius + 1); i++) {
        free(window[i]);
    }
    free(window);
    free(decompressBuffer);

    // Return result.
    return avg;
}

float applyWindowedSweepQuadtree(QuadtreeNode* root, size_t width, size_t height, int radius) {
    printf("Applying windowed sweep on the quadtree.\n");
    // Allocate memory.
    RASTER_DTYPE** window = (RASTER_DTYPE**)malloc((2 * radius + 1) * sizeof(RASTER_DTYPE*));
    for (int i = 0; i < (2 * radius + 1); i++) {
        window[i] = (RASTER_DTYPE*)malloc((2 * radius + 1) * sizeof(RASTER_DTYPE));
    }
    RASTER_DTYPE* decompressBuffer = (RASTER_DTYPE*)malloc((BLOCK_SIZE * BLOCK_SIZE + 32) * sizeof(RASTER_DTYPE));

    // Compute.
    float sumAvg = 0.0;
    size_t countAvg = 0;
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            float sum = 0.0;
            size_t count = 0;
            // Read the window from the uncompressed data.
            readWindowQuadtree(root, x, y, radius, window, decompressBuffer);
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
    for (int i = 0; i < (2 * radius + 1); i++) {
        free(window[i]);
    }
    free(window);
    free(decompressBuffer);

    // Return result.
    return sumAvg / countAvg;
}

void readWindowRaster(RASTER_DTYPE** raster, size_t width, size_t height, size_t x, size_t y, int radius, RASTER_DTYPE** window) {
    for (int wy = -radius; wy <= radius; wy++) {
        for (int wx = -radius; wx <= radius; wx++) {
            int px = x + wx;
            int py = y + wy;
            // Boundary check
            if (px >= 0 && px < width && py >= 0 && py < height) {
                raster[py][px];
                window[wy + radius][wx + radius] = raster[py][px];
            }
        }   
    }
}

float applyWindowedReadRaster(RASTER_DTYPE** rasterData, size_t width, size_t height, int radius) {
    printf("Applying windowed sweep on the raster.\n");
    // Allocate memory.
    RASTER_DTYPE** window = (RASTER_DTYPE**)malloc((2 * radius + 1) * sizeof(RASTER_DTYPE*));
    for (int i = 0; i < 2 * radius + 1; i++) {
        window[i] = (RASTER_DTYPE*)malloc((2 * radius + 1) * sizeof(RASTER_DTYPE));
    }
    // Compute.
    int x = width / 2;
    int y = height / 2;

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

    // Free memory.
    for (int i = 0; i < 2 * radius + 1; i++) {
        free(window[i]);
    }
    free(window);
    // Return result.
    return avg;
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

    int nXSize = 4;
    int nYSize = 4;

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

    #if USE_QUAD_TREE
        QuadtreeNode* root = createQuadtree(raster2D, nXSize, nYSize, BLOCK_SIZE, COMPRESS_QUAD_TREE);
        // free raster2D
        for (int i = 0; i < nYSize; i++) {
            free(raster2D[i]);
        }
        free(raster2D);
        begin = clock();
        float result = applyWindowedReadQuadtree(root, nXSize, nYSize, WINDOW_RADIUS);
        printf("Result: %f\n", result);
        end = clock();
        time_spent = (double)(end - begin) / CLOCKS_PER_SEC;
        printf("Time to decompress and process window: %f\n", time_spent);
        freeQuadtree(root);
    #else
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