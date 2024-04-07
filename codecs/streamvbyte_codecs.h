#include "generic_codecs.h"
#include <cstdint>
#include <string>
#include <iostream>
#include "streamvbyte.h"
#include <cassert>

#ifdef __clang__
#pragma clang diagnostic ignored "-Wdeclaration-after-statement"
#pragma clang diagnostic ignored "-Wunused-variable"
#endif

class StreamVByteCodec : public StatefulIntegerCodec<int32_t> {
public:

  std::vector<uint8_t> compressed;

  void encodeArray(const int32_t *in, const size_t length) override {
    size_t compsize = streamvbyte_encode(reinterpret_cast<const uint32_t *>(in), length, compressed.data()); // encoding
    compressed.resize(compsize);
  }

  void decodeArray(int32_t *out, const std::size_t length) override {
    size_t compsize = compressed.size();
    assert(
        streamvbyte_decode(compressed.data(), reinterpret_cast<uint32_t *>(out), length)
        == compsize);
  }

  std::size_t encodedNumValues() override {
    return compressed.size();
  }

  std::size_t encodedSizeValue() override {
    return sizeof(uint8_t);
  }

  virtual ~StreamVByteCodec() {}

  std::string name() const override {
    return "STREAMVBYTE";
  }

  std::size_t getOverflowSize(size_t) const override {
    return 0;
  }

  StatefulIntegerCodec<int32_t>* cloneFresh() const override {
    return new StreamVByteCodec();
  }

  void allocEncoded(const int32_t* in, size_t length) override {
    compressed.resize(streamvbyte_max_compressedbytes(length));
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