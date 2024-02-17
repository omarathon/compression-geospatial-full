#include <lzma.h>
#include <iostream>
#include <vector>

void compressLZMA(const std::vector<int32_t>& input, std::vector<uint8_t>& output) {
    lzma_stream strm = LZMA_STREAM_INIT;
    if (lzma_easy_encoder(&strm, LZMA_PRESET_DEFAULT, LZMA_CHECK_CRC64) != LZMA_OK) {
        std::cerr << "LZMA compression failed." << std::endl;
        return;
    }

    strm.next_in = reinterpret_cast<const uint8_t*>(input.data());
    strm.avail_in = input.size() * sizeof(int32_t);
    size_t outputSize = strm.avail_in + strm.avail_in / 3 + 128;
    output.resize(outputSize);
    strm.next_out = output.data();
    strm.avail_out = outputSize;

    if (lzma_code(&strm, LZMA_FINISH) != LZMA_STREAM_END) {
        std::cerr << "LZMA compression didn't finish successfully." << std::endl;
    }

    output.resize(strm.total_out);
    lzma_end(&strm);
}

void decompressLZMA(const std::vector<uint8_t>& input, std::vector<int32_t>& output, size_t originalSize) {
    lzma_stream strm = LZMA_STREAM_INIT;
    if (lzma_stream_decoder(&strm, UINT64_MAX, 0) != LZMA_OK) {
        std::cerr << "LZMA decompressor initialization failed." << std::endl;
        return;
    }

    strm.next_in = input.data();
    strm.avail_in = input.size();
    output.resize(originalSize / sizeof(int32_t));
    strm.next_out = reinterpret_cast<uint8_t*>(output.data());
    strm.avail_out = originalSize;

    if (lzma_code(&strm, LZMA_FINISH) != LZMA_STREAM_END) {
        std::cerr << "LZMA decompression didn't finish successfully." << std::endl;
    }

    lzma_end(&strm);
}

int main() {
    std::vector<int32_t> data = {10,1,9,3,4,5,6,7,2,8,10,1,9,3,4,5,6,7,2,8,10,1,9,3,4,5,6,7,2,8,10,1,9,3,4,5,6,7,2,8,10,1,9,3,4,5,6,7,2,8};
    std::vector<uint8_t> compressedData;
    compressLZMA(data, compressedData);

    std::vector<int32_t> decompressedData;
    decompressLZMA(compressedData, decompressedData, data.size() * sizeof(int32_t));

    if (data != decompressedData) {
        std::cout << "in!=out" << std::endl;
        exit(1);
    }
    auto compressionFactor = (float)(compressedData.size() * sizeof(uint8_t)) / (float)(data.size() * sizeof(int32_t));
    auto bitsPerInt = (float)(compressedData.size() * sizeof(uint8_t)) / (float)data.size();
    std::cout << "cf " << compressionFactor << " bpi " << bitsPerInt << std::endl;
}
