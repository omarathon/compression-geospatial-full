#include <lz4.h>
#include <vector>
#include <iostream>

void compressLZ4(const std::vector<int32_t>& input, std::vector<char>& output) {
    int maxOutputSize = LZ4_compressBound(input.size() * sizeof(int32_t));
    output.resize(maxOutputSize);

    int compressedDataSize = LZ4_compress_default(reinterpret_cast<const char*>(input.data()), output.data(), input.size() * sizeof(int32_t), maxOutputSize);
    if (compressedDataSize <= 0) {
        std::cerr << "LZ4 compression failed." << std::endl;
        return;
    }
    output.resize(compressedDataSize);
}

void decompressLZ4(const std::vector<char>& input, std::vector<int32_t>& output, int originalSize) {
    output.resize(originalSize / sizeof(int32_t));
    int decompressedSize = LZ4_decompress_safe(input.data(), reinterpret_cast<char*>(output.data()), input.size(), originalSize);
    if (decompressedSize < 0) {
        std::cerr << "LZ4 decompression failed." << std::endl;
        return;
    }
}

int main() {
    std::vector<int32_t> data = {10,1,9,3,4,5,6,7,2,8,10,1,9,3,4,5,6,7,2,8,10,1,9,3,4,5,6,7,2,8,10,1,9,3,4,5,6,7,2,8,10,1,9,3,4,5,6,7,2,8,10,1,9,3,4,5,6,7,2,8};
    std::vector<char> compressedData;
    compressLZ4(data, compressedData);

    std::vector<int32_t> decompressedData;
    decompressLZ4(compressedData, decompressedData, data.size() * sizeof(int32_t));

    if (data != decompressedData) {
        std::cout << "in!=out" << std::endl;
        exit(1);
    }
    auto compressionFactor = (float)(compressedData.size() * sizeof(uint8_t)) / (float)(data.size() * sizeof(int32_t));
    auto bitsPerInt = (float)(compressedData.size() * sizeof(uint8_t)) / (float)data.size();
    std::cout << "cf " << compressionFactor << " bpi " << bitsPerInt << std::endl;
}
