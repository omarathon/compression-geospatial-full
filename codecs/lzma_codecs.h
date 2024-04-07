#include "generic_codecs.h"
#include <cstdint>
#include <string>
#include <iostream>
#include <lzma.h>
#include <cassert>

#ifdef __clang__
#pragma clang diagnostic ignored "-Wreturn-local-addr"
#endif

class LZMACodec : public StatefulIntegerCodec<int32_t> {
public:

  std::vector<uint8_t> compressed;

  void encodeArray(const int32_t *in, const size_t length) override {
    lzma_stream strm = LZMA_STREAM_INIT;
    if (lzma_easy_encoder(&strm, LZMA_PRESET_DEFAULT, LZMA_CHECK_CRC64) != LZMA_OK) {
        throw std::runtime_error("LZMA compression failed.");
        return;
    }

    strm.next_in = reinterpret_cast<const uint8_t*>(in);
    strm.avail_in = length * sizeof(int32_t);
    size_t outputSize = strm.avail_in + strm.avail_in / 3 + 128;
    compressed.resize(outputSize);
    strm.next_out = compressed.data();
    strm.avail_out = outputSize;

    if (lzma_code(&strm, LZMA_FINISH) != LZMA_STREAM_END) {
        throw std::runtime_error("LZMA compression didn't finish successfully.");
        return;
    }

    compressed.resize(strm.total_out);
    lzma_end(&strm);
  }

  void decodeArray(int32_t *out, const std::size_t length) override {
        lzma_stream strm = LZMA_STREAM_INIT;
        if (lzma_stream_decoder(&strm, UINT64_MAX, 0) != LZMA_OK) {
            throw std::runtime_error("LZMA decompressor initialization failed.");
            return;
        }

        strm.next_in = compressed.data();
        strm.avail_in = compressed.size();

        strm.next_out = reinterpret_cast<uint8_t*>(out);
        strm.avail_out = length * sizeof(int32_t);

        if (lzma_code(&strm, LZMA_FINISH) != LZMA_STREAM_END) {
            throw std::runtime_error("LZMA decompression didn't finish successfully.");
        }

        lzma_end(&strm);
  }

  std::size_t encodedNumValues() override {
    return compressed.size();
  }

  std::size_t encodedSizeValue() override {
    return sizeof(uint8_t);
  }

  virtual ~LZMACodec() {}

  std::string name() const override {
    return "LZMA";
  }

  std::size_t getOverflowSize(size_t) const override {
    return 0;
  }

  StatefulIntegerCodec<int32_t>* cloneFresh() const override {
    return new LZMACodec();
  }

  void allocEncoded(const int32_t* in, size_t length) override {
    // Only known after encoding stream is formed.
  };

  void clear() override {
      compressed.clear();
      compressed.shrink_to_fit();
  }

  std::vector<int32_t>& getEncoded() override {
      throw std::runtime_error("Encoded format does not match input. Cannot forward.");
      std::vector<int32_t> dummy{};
      return dummy;
  };
};