#include "generic_codecs.h"
#include <cstdint>
#include <string>
#include <iostream>
#include <zstd.h>
#include <cassert>

#ifdef __clang__
#pragma clang diagnostic ignored "-Wreturn-local-addr"
#endif

// TODO: Add option to provide a pre-trained zstd dictionary.
class ZstdCodec : public StatefulIntegerCodec<int32_t> {
public:
  int compressionLevel;
  std::vector<char> compressed;

  ZstdCodec(int compressionLevel) : compressionLevel{compressionLevel} {}

  ZstdCodec() : ZstdCodec(/* compressionLevel */ 3) {}

  void encodeArray(const int32_t *in, const size_t length) override {
    size_t maxOutputSize = ZSTD_compressBound(length * sizeof(int32_t));
    size_t compressedSize = ZSTD_compress(compressed.data(), maxOutputSize, in, length * sizeof(int32_t), compressionLevel);
    if (ZSTD_isError(compressedSize)) {
        throw std::runtime_error("Zstd compression error: " + std::string(ZSTD_getErrorName(compressedSize)));
        return;
    }
    compressed.resize(compressedSize);
  }

  void decodeArray(int32_t *out, const std::size_t length) override {
    size_t const decompressedSize = ZSTD_decompress(out, length * sizeof(int32_t), compressed.data(), compressed.size());
    if (ZSTD_isError(decompressedSize)) {
        throw std::runtime_error("Zstd decompression error: " + std::string(ZSTD_getErrorName(decompressedSize)));
        return;
    }
  }

  std::size_t encodedNumValues() override {
    return compressed.size();
  }

  std::size_t encodedSizeValue() override {
    return sizeof(char);
  }

  virtual ~ZstdCodec() {}

  std::string name() const override {
    return "Zstd_" + std::to_string(compressionLevel);
  }

  std::size_t getOverflowSize(size_t) const override {
    return 0;
  }

  StatefulIntegerCodec<int32_t>* cloneFresh() const override {
    return new ZstdCodec(compressionLevel);
  }

  void allocEncoded(const int32_t* in, size_t length) override {
    size_t maxOutputSize = ZSTD_compressBound(length * sizeof(int32_t));
    compressed.resize(maxOutputSize);
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