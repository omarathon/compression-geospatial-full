#include <zlib.h>
#include <vector>
#include <iostream>

void compressDeflate(const std::vector<int32_t>& input, std::vector<uint8_t>& output) {
    uLongf outputSize = compressBound(input.size() * sizeof(int32_t));
    output.resize(outputSize);

    if (compress2(output.data(), &outputSize, reinterpret_cast<const Bytef*>(input.data()), input.size() * sizeof(int32_t), Z_BEST_COMPRESSION) != Z_OK) {
        std::cerr << "DEFLATE compression failed." << std::endl;
        return;
    }
    output.resize(outputSize);
}

void decompressDeflate(const std::vector<uint8_t>& input, std::vector<int32_t>& output, size_t originalSize) {
    uLongf outputSize = originalSize;
    output.resize(outputSize / sizeof(int32_t));

    if (uncompress(reinterpret_cast<Bytef*>(output.data()), &outputSize, input.data(), input.size()) != Z_OK) {
        std::cerr << "DEFLATE decompression failed." << std::endl;
        return;
    }
}

int main() {
    std::vector<int32_t> data = {10,1,9,3,4,5,6,7,2,8};
    std::vector<uint8_t> compressedData;
    compressDeflate(data, compressedData);

    std::vector<int32_t> decompressedData;
    decompressDeflate(compressedData, decompressedData, data.size() * sizeof(int32_t));

    if (data != decompressedData) {
        std::cout << "in!=out" << std::endl;
        exit(1);
    }
    auto compressionFactor = (float)(compressedData.size() * sizeof(uint8_t)) / (float)(data.size() * sizeof(int32_t));
    auto bitsPerInt = (float)(compressedData.size() * sizeof(uint8_t)) / (float)data.size();
    std::cout << "cf " << compressionFactor << " bpi " << bitsPerInt << std::endl;
}
