#include <lz4.h>

#include <cassert>
#include <cstdint>
#include <iostream>
#include <string>

#include "generic_codecs.h"

#ifdef __clang__
#pragma clang diagnostic ignored "-Wreturn-local-addr"
#endif

class LZ4Codec : public StatefulIntegerCodec<int32_t> {
 public:
  std::vector<char> compressed;

  void EncodeArray(const int32_t* in, const size_t length) override {
    int maxOutputSize = LZ4_compressBound(length * sizeof(int32_t));
    int compressedDataSize = LZ4_compress_default(
        reinterpret_cast<const char*>(in), compressed.data(),
        length * sizeof(int32_t), maxOutputSize);
    if (compressedDataSize <= 0) {
      throw std::runtime_error("LZ4 compression failed.");
      return;
    }
    compressed.resize(compressedDataSize);
  }

  void DecodeArray(int32_t* out, const std::size_t length) override {
    int decompressedSize =
        LZ4_decompress_safe(compressed.data(), reinterpret_cast<char*>(out),
                            compressed.size(), length * sizeof(int32_t));
    if (decompressedSize < 0) {
      throw std::runtime_error("LZ4 decompression failed." + decompressedSize);
      return;
    }
    assert(decompressedSize == (length * sizeof(int32_t)));
  }

  std::size_t EncodedNumValues() override { return compressed.size(); }

  std::size_t EncodedSizeValue() override { return sizeof(char); }

  virtual ~LZ4Codec() {}

  std::string name() const override { return "LZ4"; }

  std::size_t GetOverflowSize(size_t) const override { return 0; }

  StatefulIntegerCodec<int32_t>* CloneFresh() const override {
    return new LZ4Codec();
  }

  void AllocEncoded(const int32_t* in, size_t length) override {
    compressed.resize(LZ4_compressBound(length * sizeof(int32_t)));
  };

  void clear() override {
    compressed.clear();
    compressed.shrink_to_fit();
  }

  std::vector<int32_t>& GetEncoded() override {
    throw std::runtime_error(
        "Encoded format does not match input. Cannot forward.");
  };
};