#include <vector>
#include <cstdint>
#include <algorithm>
#include "morton.h"
#include <stdexcept>

// Remap a 1D array from row-major to Morton order using the libmorton library
std::vector<int32_t> remapToMortonOrder(const std::vector<int32_t>& input, int N) {
    if (input.size() != N * N) {
        throw std::invalid_argument("Morton remap input size does not match the specified dimensions.");
    }

    std::vector<int32_t> output(N * N, 0);
    uint_fast32_t maxIndex = N * N - 1;

    for (int y = 0; y < N; ++y) {
        for (int x = 0; x < N; ++x) {
            int index = y * N + x;
            uint_fast32_t mortonCode = libmorton::morton2D_32_encode(static_cast<uint_fast16_t>(x), static_cast<uint_fast16_t>(y));
            if (mortonCode > maxIndex) {
                std::cerr << "Morton remap error: Morton code out of bounds - " << mortonCode << std::endl;
                throw std::out_of_range("Morton remap code exceeds the bounds of the output array.");
            }
            output[mortonCode] = input[index];
        }
    }
    return output;
}

// Remap a 1D array from row-major to zigzag order
std::vector<int32_t> remapToZigzagOrder(const std::vector<int32_t>& input, int N) {
    std::vector<int32_t> output(N * N);

    for (int y = 0; y < N; ++y) {
        if (y % 2 == 0) {
            for (int x = 0; x < N; ++x) {
                int index = y * N + x; 
                output[index] = input[index];
            }
        } else {
            for (int x = 0; x < N; ++x) {
                int index = y * N + x;
                int zigzagIndex = y * N + (N - 1 - x);
                output[zigzagIndex] = input[index];
            }
        }
    }

    return output;
}