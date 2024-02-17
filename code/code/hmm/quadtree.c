#include "types.h"
#include "quadtree.h"

QuadtreeNode* createQuadtreeNode(int x, int y, int width, int height) {
    QuadtreeNode* node = (QuadtreeNode*)malloc(sizeof(QuadtreeNode));
    node->x = x;
    node->y = y;
    node->width = width;
    node->height = height;
    node->uncompressedSize = width * height;
    for (int i = 0; i < 4; i++) {
        node->children[i] = NULL;
    }
    return node;
}

COMPRESS_DTYPE* compressRasterData(RASTER_DTYPE* data, size_t n, size_t* compressedSize) {
    COMPRESS_DTYPE* compressed = (COMPRESS_DTYPE*)malloc(P4NENC_BOUND(n));
    *compressedSize = TURBOP4_ENC(data, n, compressed);
    return compressed;
}

RASTER_DTYPE* extractRasterData(RASTER_DTYPE** rasterData, int x, int y, int width, int height) {
    RASTER_DTYPE* data = (RASTER_DTYPE*)malloc(height * width * sizeof(RASTER_DTYPE));
    if (!data) {
        return NULL;
    }
    for (int i = 0; i < height; i++) {
        for (int j = 0; j < width; j++) {
            data[i * width + j] = rasterData[y + i][x + j];
        }
    }
    return data;
}

void buildQuadtree(QuadtreeNode* node, RASTER_DTYPE** rasterData, size_t gridSize, bool compress) {
    node->compress = compress;
    if (node->width <= gridSize || node->height <= gridSize) {
        // This node is a leaf. Store the raster data & compressed raster data here.
        node->uncompressedData = extractRasterData(rasterData, node->x, node->y, node->width, node->height);
        if (compress) {
            node->compressedData = compressRasterData(node->uncompressedData, node->width * node->height, &node->compressedSize);
            free(node->uncompressedData);
            node->uncompressedData = NULL;
        }
        printf("Leaf at x = %d, y = %d, w = %d, h = %d\n", node->x, node->y, node->width, node->height);
        return;
    }

    int halfWidth = node->width / 2;
    int halfHeight = node->height / 2;
    int extraWidth = node->width % 2;
    int extraHeight = node->height % 2;

    // Create four children nodes
    node->children[0] = createQuadtreeNode(node->x, node->y, halfWidth, halfHeight);
    node->children[1] = createQuadtreeNode(node->x, node->y + halfHeight, halfWidth, halfHeight + extraHeight);
    node->children[2] = createQuadtreeNode(node->x + halfWidth, node->y, halfWidth + extraWidth, halfHeight);
    node->children[3] = createQuadtreeNode(node->x + halfWidth, node->y + halfHeight, halfWidth + extraWidth, halfHeight + extraHeight);

    // Recurse on children
    for (int i = 0; i < 4; i++) {
        buildQuadtree(node->children[i], rasterData, gridSize, compress);
    }
}

QuadtreeNode* createQuadtree(RASTER_DTYPE** rasterData, size_t width, size_t height, size_t gridSize, bool compress) {
    printf("Creating quadtree\n");
    QuadtreeNode* root = createQuadtreeNode(0, 0, width, height);
    buildQuadtree(root, rasterData, gridSize, compress);
    printf("Done creating quadtree\n");
    return root;
}

bool isLeafNode(QuadtreeNode* node) {
    return node->children[0] == NULL;
}

QuadtreeNode* findQuadtreeLeafNode(QuadtreeNode* node, int x, int y) {
    if (node == NULL || !isLeafNode(node)) {
        return NULL;
    }

    if (x >= node->x && x < node->x + node->width && y >= node->y && y < node->y + node->height) {
        return node;
    }

    // Recursively search in the appropriate child
    int index = (y >= node->y + node->height / 2) * 2 + (x >= node->x + node->width / 2);
    return findQuadtreeLeafNode(node->children[index], x, y);
}

void freeQuadtree(QuadtreeNode* node) {
    if (node == NULL) {
        return;
    }

    // Recursively free child nodes
    for (int i = 0; i < 4; i++) {
        freeQuadtree(node->children[i]);
    }

    // Free the dynamically allocated data in the node
    if (node->compressedData != NULL) {
        free(node->compressedData);
    }
    if (node->uncompressedData != NULL) {
        free(node->uncompressedData);
    }

    // Finally, free the node itself
    free(node);
}