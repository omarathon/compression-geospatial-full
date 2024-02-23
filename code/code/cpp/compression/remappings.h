#include <vector>
#include <cstdint>
#include <algorithm>
#include "morton.h"

// Remap a 1D array from row-major to Morton order using the libmorton library
std::vector<int32_t> remapToMortonOrder(const std::vector<int32_t>& input, int N) {
    std::vector<int32_t> output(N * N, 0);
    for (int y = 0; y < N; ++y) {
        for (int x = 0; x < N; ++x) {
            int index = y * N + x;
            uint_fast32_t mortonCode = libmorton::morton2D_32_encode(static_cast<uint_fast16_t>(x), static_cast<uint_fast16_t>(y));
            output[mortonCode] = input[index];
        }
    }
    return output;
}

// Remap a 1D array from row-major to zigzag order
std::vector<int32_t> remapToZigzagOrder(const std::vector<int32_t>& input, int N) {
    std::vector<int32_t> output(N * N); // Initialize output vector with the same size as input

    for (int y = 0; y < N; ++y) {
        if (y % 2 == 0) {
            // Even rows: left to right
            for (int x = 0; x < N; ++x) {
                int index = y * N + x; // Calculate index in the original (row-major) array
                output[index] = input[index];
            }
        } else {
            // Odd rows: right to left
            for (int x = 0; x < N; ++x) {
                int index = y * N + x; // Calculate index in the original (row-major) array
                int zigzagIndex = y * N + (N - 1 - x); // Calculate the zigzag index
                output[zigzagIndex] = input[index];
            }
        }
    }

    return output;
}