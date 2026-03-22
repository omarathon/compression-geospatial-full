#pragma once

#include <algorithm>
#include <cstdint>
#include <vector>

#include "gdal_priv.h"

// Updates `min` with the minimum value found in the given raster block.
inline void ComputeMinForBlock(GDALRasterBand* band, int xOff, int yOff,
                                int blockSize, int32_t& min) {
  std::vector<int32_t> blockData(blockSize * blockSize);
  band->RasterIO(GF_Read, xOff, yOff, blockSize, blockSize, blockData.data(),
                 blockSize, blockSize, GDT_Int32, 0, 0);
  for (auto& v : blockData) min = std::min(min, v);
}

// Reads a raster block, handling partial blocks at the raster boundary.
inline std::vector<int32_t> ReadGeoTiffBlock(GDALRasterBand* band, int xOff,
                                              int yOff, int blockSize,
                                              int rasterWidth,
                                              int rasterHeight) {
  int w = std::min(blockSize, rasterWidth - xOff);
  int h = std::min(blockSize, rasterHeight - yOff);
  std::vector<int32_t> data(w * h);
  band->RasterIO(GF_Read, xOff, yOff, w, h, data.data(), w, h, GDT_Int32, 0,
                 0);
  return data;
}
