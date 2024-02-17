#include <vector>
#include <cstdint>
#include <iostream>
#include <zstd.h>

template <typename T>
void compressZstd(const std::vector<T>& input, std::vector<char>& output, int compressionLevel) {
    size_t maxOutputSize = ZSTD_compressBound(input.size() * sizeof(T));
    output.resize(maxOutputSize);

    size_t compressedSize = ZSTD_compress(output.data(), maxOutputSize, input.data(), input.size() * sizeof(T), compressionLevel);
    if (ZSTD_isError(compressedSize)) {
        std::cerr << "Zstd compression error: " << ZSTD_getErrorName(compressedSize) << std::endl;
        return;
    }

    output.resize(compressedSize);
}

template <typename T>
void decompressZstd(const std::vector<char>& input, std::vector<T>& output, size_t originalSize) {
    output.resize(originalSize / sizeof(T));

    size_t const decompressedSize = ZSTD_decompress(output.data(), originalSize, input.data(), input.size());
    if (ZSTD_isError(decompressedSize)) {
        std::cerr << "Zstd decompression error: " << ZSTD_getErrorName(decompressedSize) << std::endl;
        return;
    }
}

template <typename T>
void check(const std::vector<T>& input, std::vector<char>& compressed, std::vector<T>& decompressed) {
    if (input != decompressed) {
        std::cout << "in!=out" << std::endl;
        exit(1);
    }
    auto compressionFactor = (float)(compressed.size() * sizeof(char)) / (float)(input.size() * sizeof(T));
    auto bitsPerInt = (float)(compressed.size() * sizeof(char)) / (float)input.size();
    std::cout << "cf " << compressionFactor << " bpi " << bitsPerInt << std::endl;
}

int main() {
    // Example usage
    std::vector<uint8_t> data8 = {10,1,9,3,4,5,6,7,2,8,10,1,9,3,4,5,6,7,2,8,10,1,9,3,4,5,6,7,2,8,10,1,9,3,4,5,6,7,2,8};
    std::vector<uint16_t> data16 = {10,1,9,3,4,5,6,7,2,8,10,1,9,3,4,5,6,7,2,8,10,1,9,3,4,5,6,7,2,8,10,1,9,3,4,5,6,7,2,8};
    std::vector<uint32_t> data32 = {10,1,9,3,4,5,6,7,2,8,10,1,9,3,4,5,6,7,2,8,10,1,9,3,4,5,6,7,2,8,10,1,9,3,4,5,6,7,2,8};

    std::vector<char> compressedData8, compressedData16, compressedData32;
    std::vector<uint8_t> decompressedData8;
    std::vector<uint16_t> decompressedData16;
    std::vector<uint32_t> decompressedData32;

    int compressionLevel = 3; // Adjust as needed

    compressZstd(data8, compressedData8, compressionLevel);
    compressZstd(data16, compressedData16, compressionLevel);
    compressZstd(data32, compressedData32, compressionLevel);

    decompressZstd(compressedData8, decompressedData8, data8.size() * sizeof(uint8_t));
    decompressZstd(compressedData16, decompressedData16, data16.size() * sizeof(uint16_t));
    decompressZstd(compressedData32, decompressedData32, data32.size() * sizeof(uint32_t));

    check(data8, compressedData8, decompressedData8);
    check(data16, compressedData16, decompressedData16);
    check(data32, compressedData32, decompressedData32);
}
