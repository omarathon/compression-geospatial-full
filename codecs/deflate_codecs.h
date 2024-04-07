#include "generic_codecs.h"
#include <cstdint>
#include <string>
#include <zlib.h>
#include <iostream>

#ifdef __clang__
#pragma clang diagnostic ignored "-Wreturn-local-addr"
#endif

class DeflateCodec : public StatefulIntegerCodec<int32_t> {
public:

  std::vector<uint8_t> compressed;

  void encodeArray(const int32_t *in, const size_t length) override {
    uLongf outputSize = compressBound(length * sizeof(int32_t));
    if (compress2(compressed.data(), &outputSize, reinterpret_cast<const Bytef*>(in), length * sizeof(int32_t), Z_BEST_COMPRESSION) != Z_OK) {
        std::cerr << "DEFLATE compression failed." << std::endl;
        exit(1);
    }
    compressed.resize(outputSize);
  }

  void decodeArray(int32_t *out, const std::size_t length) override {
    uLongf outputSize = length;
    uLongf compressedSize = length * sizeof(int32_t);

    if (uncompress(reinterpret_cast<Bytef*>(out), &compressedSize, compressed.data(), compressed.size()) != Z_OK) {
        std::cerr << "DEFLATE decompression failed." << std::endl;
        return;
    }
  }

  std::size_t encodedNumValues() override {
    return compressed.size();
  }

  std::size_t encodedSizeValue() override {
    return sizeof(uint8_t);
  }

  virtual ~DeflateCodec() {}

  std::string name() const override {
    return "DEFLATE";
  }

  std::size_t getOverflowSize(size_t) const override {
    return 0;
  }

  StatefulIntegerCodec<int32_t>* cloneFresh() const override {
    return new DeflateCodec();
  }

  void allocEncoded(const int32_t* in, size_t length) override {
    uLongf outputSize = compressBound(length * sizeof(int32_t));
    compressed.resize(outputSize); // NOTE: reserve doesn't work.
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