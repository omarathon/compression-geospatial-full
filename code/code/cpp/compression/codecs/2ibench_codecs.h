#include "generic_codecs.h"
#include <cstdint>
#include <string>
#include <iostream>
#include <cstring>
#include <memory>

#include "2i_bench/include/ds2i/block_codecs.hpp"

/////////////////////////////////////
/* ===== interpolative_block ===== */
/////////////////////////////////////

class TwoIBenchInterpolativeBlockCodec : public StatefulIntegerCodec<int32_t> {
private:
    std::vector<uint8_t> compressed;

public:
    TwoIBenchInterpolativeBlockCodec() {}

    void encodeArray(const int32_t *in, const size_t length) override {
        compressed.clear();
        compressed.shrink_to_fit();
        for (size_t i = 0; i < length; i += ds2i::interpolative_block::block_size) {
            size_t cur_block_size = std::min(ds2i::interpolative_block::block_size, length - i);
            ds2i::interpolative_block::encode(reinterpret_cast<const uint32_t*>(in + i), -1, cur_block_size, compressed);
        }
    }

    void decodeArray(int32_t *out, const size_t length) override {
        const uint8_t* input_ptr = compressed.data();
        for (size_t block = 0, i = 0; i < length; i += ds2i::interpolative_block::block_size, ++block) {
            size_t cur_block_size = std::min(ds2i::interpolative_block::block_size, length - i);
            input_ptr = ds2i::interpolative_block::decode(input_ptr, reinterpret_cast<uint32_t *>(out + i), -1, cur_block_size);
        }
    }

    std::size_t encodedNumValues() override {
      return compressed.size();
    }

    std::size_t encodedSizeValue() override {
      return sizeof(uint8_t);
    }

    virtual ~TwoIBenchInterpolativeBlockCodec() {}

    std::string name() const override {
        return "2ibench_interpolativeblock";
    }

    std::size_t getOverflowSize(size_t) const override {
      return ds2i::interpolative_block::overflow;
    }

    StatefulIntegerCodec<int32_t>* cloneFresh() const override {
        return new TwoIBenchInterpolativeBlockCodec();
    }
};

class TwoIBenchInterpolativeBlockParallelCodec : public StatefulIntegerCodec<int32_t> {
private:
    std::vector<std::vector<uint8_t>> compressed_blocks;

public:
    TwoIBenchInterpolativeBlockParallelCodec() {}

    void encodeArray(const int32_t *in, const size_t length) override {
        size_t num_blocks = (length + ds2i::interpolative_block::block_size - 1) / ds2i::interpolative_block::block_size;
        compressed_blocks.resize(num_blocks);
        compressed_blocks.shrink_to_fit();

        #pragma omp parallel for
        for (size_t block = 0; block < num_blocks; ++block) {
            compressed_blocks[block].clear();
            compressed_blocks[block].shrink_to_fit();
            size_t start_idx = block * ds2i::interpolative_block::block_size;
            size_t end_idx = std::min(start_idx + ds2i::interpolative_block::block_size, length);
            size_t cur_block_size = end_idx - start_idx;
            ds2i::interpolative_block::encode(reinterpret_cast<const uint32_t*>(in + start_idx), -1, cur_block_size, compressed_blocks[block]);
        }
    }

    void decodeArray(int32_t *out, const size_t length) override {
        size_t num_blocks = compressed_blocks.size();

        #pragma omp parallel for
        for (size_t block = 0; block < num_blocks; ++block) {
            size_t start_idx = block * ds2i::interpolative_block::block_size;
            size_t cur_block_size = std::min(ds2i::interpolative_block::block_size, length - start_idx);
            ds2i::interpolative_block::decode(compressed_blocks[block].data(), reinterpret_cast<uint32_t *>(out + start_idx), -1, cur_block_size);
        }
    }

    std::size_t encodedNumValues() override {
        size_t total_size = 0;
        for (const auto& block : compressed_blocks) {
            total_size += block.size();
        }
        return total_size;
    }

    std::size_t encodedSizeValue() override {
        return sizeof(uint8_t);
    }

    virtual ~TwoIBenchInterpolativeBlockParallelCodec() {}

    std::string name() const override {
        return "2ibench_interpolativeblock_parallel";
    }

    std::size_t getOverflowSize(size_t) const override {
      return ds2i::interpolative_block::overflow;
    }

    StatefulIntegerCodec<int32_t>* cloneFresh() const override {
        return new TwoIBenchInterpolativeBlockParallelCodec();
    }
};


/////////////////////////////////////
/* ===== qmx_block ===== */
/////////////////////////////////////

class TwoIBenchQmxBlockCodec : public StatefulIntegerCodec<int32_t> {
private:
    std::vector<uint8_t> compressed;

public:
    TwoIBenchQmxBlockCodec() {}

    void encodeArray(const int32_t *in, const size_t length) override {
        compressed.clear();
        compressed.shrink_to_fit();
        for (size_t i = 0; i < length; i += ds2i::qmx_block::block_size) {
            size_t cur_block_size = std::min(ds2i::qmx_block::block_size, length - i);
            ds2i::qmx_block::encode(reinterpret_cast<const uint32_t*>(in + i), -1, cur_block_size, compressed);
        }
    }

    void decodeArray(int32_t *out, const size_t length) override {
        const uint8_t* input_ptr = compressed.data();
        for (size_t block = 0, i = 0; i < length; i += ds2i::qmx_block::block_size, ++block) {
            size_t cur_block_size = std::min(ds2i::qmx_block::block_size, length - i);
            input_ptr = ds2i::qmx_block::decode(input_ptr, reinterpret_cast<uint32_t *>(out + i), -1, cur_block_size);
        }
    }

    std::size_t encodedNumValues() override {
      return compressed.size();
    }

    std::size_t encodedSizeValue() override {
      return sizeof(uint8_t);
    }

    virtual ~TwoIBenchQmxBlockCodec() {}

    std::string name() const override {
        return "2ibench_qmxblock";
    }

    std::size_t getOverflowSize(size_t) const override {
      return ds2i::qmx_block::overflow;
    }

    StatefulIntegerCodec<int32_t>* cloneFresh() const override {
        return new TwoIBenchQmxBlockCodec();
    }
};

class TwoIBenchQmxBlockParallelCodec : public StatefulIntegerCodec<int32_t> {
private:
    std::vector<std::vector<uint8_t>> compressed_blocks;

public:
    TwoIBenchQmxBlockParallelCodec() {}

    void encodeArray(const int32_t *in, const size_t length) override {
        size_t num_blocks = (length + ds2i::qmx_block::block_size - 1) / ds2i::qmx_block::block_size;
        compressed_blocks.resize(num_blocks);
        compressed_blocks.shrink_to_fit();

        #pragma omp parallel for
        for (size_t block = 0; block < num_blocks; ++block) {
            compressed_blocks[block].clear();
            compressed_blocks[block].shrink_to_fit();
            size_t start_idx = block * ds2i::qmx_block::block_size;
            size_t end_idx = std::min(start_idx + ds2i::qmx_block::block_size, length);
            size_t cur_block_size = end_idx - start_idx;
            ds2i::qmx_block::encode(reinterpret_cast<const uint32_t*>(in + start_idx), -1, cur_block_size, compressed_blocks[block]);
        }
    }

    void decodeArray(int32_t *out, const size_t length) override {
        size_t num_blocks = compressed_blocks.size();

        #pragma omp parallel for
        for (size_t block = 0; block < num_blocks; ++block) {
            size_t start_idx = block * ds2i::qmx_block::block_size;
            size_t cur_block_size = std::min(ds2i::qmx_block::block_size, length - start_idx);

            // Decode into buffer as each block has overflow
            std::vector<int32_t> decoded(ds2i::qmx_block::block_size + getOverflowSize());
            ds2i::qmx_block::decode(compressed_blocks[block].data(), reinterpret_cast<uint32_t *>(decoded.data()), -1, cur_block_size);

            // Copy decoded data into the output array
            std::copy(decoded.begin(), decoded.begin() + cur_block_size, out + start_idx);
        }
    }

    std::size_t encodedNumValues() override {
        size_t total_size = 0;
        for (const auto& block : compressed_blocks) {
            total_size += block.size();
        }
        return total_size;
    }

    std::size_t encodedSizeValue() override {
        return sizeof(uint8_t);
    }

    virtual ~TwoIBenchQmxBlockParallelCodec() {}

    std::string name() const override {
        return "2ibench_qmxblock_parallel";
    }

    std::size_t getOverflowSize(size_t) const override {
      return ds2i::qmx_block::overflow;
    }

    StatefulIntegerCodec<int32_t>* cloneFresh() const override {
        return new TwoIBenchQmxBlockParallelCodec();
    }
};