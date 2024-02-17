#include "types.h"

#define GRID_SIZE = 10

typedef struct QuadtreeNode {
    int x, y; // Position of the node (top-left corner of the region)
    int width, height; // Size of the region
    struct QuadtreeNode *children[4]; // Children nodes

    // Data for leaf nodes
    COMPRESS_DTYPE* compressedData; // Pointer to compressed data for this region
    RASTER_DTYPE* uncompressedData;

    size_t compressedSize;   // Size of the compressed data
    size_t uncompressedSize; // Size of the uncompressed data

    bool compress;
} QuadtreeNode;

QuadtreeNode* createQuadtree(RASTER_DTYPE** rasterData, size_t width, size_t height, size_t gridSize, bool compress);

void buildQuadtree(QuadtreeNode* node, RASTER_DTYPE** rasterData, size_t gridSize, bool compress);

void subdivideQuadtreeNode(QuadtreeNode* node);

QuadtreeNode* findQuadtreeLeafNode(QuadtreeNode* node, int x, int y);

bool isLeafNode(QuadtreeNode* node);

void freeQuadtree(QuadtreeNode* node);