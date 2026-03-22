#pragma once

#include <algorithm>
#include <cstdint>
#include <format>
#include <stdexcept>
#include <vector>

#include "morton.h"

// Remap a 1D array from row-major to Morton order using the libmorton library.
inline std::vector<int32_t> RemapToMortonOrder(
    const std::vector<int32_t>& input, int N) {
  if (static_cast<int>(input.size()) != N * N)
    throw std::invalid_argument(
        "Morton remap input size does not match the specified dimensions.");

  std::vector<int32_t> output(N * N, 0);
  uint_fast32_t maxIndex = static_cast<uint_fast32_t>(N * N - 1);

  for (int y = 0; y < N; ++y) {
    for (int x = 0; x < N; ++x) {
      uint_fast32_t mortonCode = libmorton::morton2D_32_encode(
          static_cast<uint_fast16_t>(x), static_cast<uint_fast16_t>(y));
      if (mortonCode > maxIndex)
        throw std::out_of_range(
            std::format("Morton remap code {} exceeds bounds (max {})",
                        mortonCode, maxIndex));
      output[mortonCode] = input[y * N + x];
    }
  }
  return output;
}

// Remap a 1D array from row-major to zigzag order.
inline std::vector<int32_t> RemapToZigzagOrder(
    const std::vector<int32_t>& input, int N) {
  std::vector<int32_t> output(N * N);

  for (int y = 0; y < N; ++y) {
    for (int x = 0; x < N; ++x) {
      int src = y * N + x;
      int dst = (y % 2 == 0) ? src : y * N + (N - 1 - x);
      output[dst] = input[src];
    }
  }
  return output;
}
