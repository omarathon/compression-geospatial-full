#include "generic_codecs.h"
#include <cstdint>
#include <string>
#include <iostream>
#include <cassert>
#include "simdcomp.h"

#ifdef __clang__
#pragma clang diagnostic ignored "-Wreturn-local-addr"
#endif

class SimdCompCodec : public StatefulIntegerCodec<int32_t> {
public:

    std::vector<uint8_t> compressed;
    uint32_t b;

  void encodeArray(const int32_t *in, const size_t length) override {
    __m128i * endofbuf = simdpack_length(reinterpret_cast<const uint32_t *>(in), length, (__m128i *)compressed.data(), b);
    int howmanybytes = (endofbuf-(__m128i *)compressed.data())*sizeof(__m128i);
    compressed.resize(howmanybytes);
  }

  void decodeArray(int32_t *out, const std::size_t length) override {
    simdunpack_length((const __m128i *)compressed.data(), length, reinterpret_cast<uint32_t *>(out), b);
  }

  std::size_t encodedNumValues() override {
    return compressed.size();
  }

  std::size_t encodedSizeValue() override {
    return sizeof(uint8_t);
  }

  virtual ~SimdCompCodec() {}

  std::string name() const override {
    return "simdcomp";
  }

  std::size_t getOverflowSize(size_t) const override {
    return 0;
  }

  StatefulIntegerCodec<int32_t>* cloneFresh() const override {
    return new SimdCompCodec();
  }

  void allocEncoded(const int32_t* in, size_t length) override {
    b = maxbits_length(reinterpret_cast<const uint32_t*>(in), length);
    compressed.resize(simdpack_compressedbytes(length, b));
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