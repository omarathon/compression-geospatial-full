#include <cassert>
#include <cstdint>
#include <iostream>
#include <string>

#include "generic_codecs.h"
#include "simdcomp.h"

#ifdef __clang__
#pragma clang diagnostic ignored "-Wreturn-local-addr"
#endif

class SimdCompCodec : public StatefulIntegerCodec<int32_t> {
 public:
  std::vector<uint8_t> compressed;
  uint32_t b;

  void EncodeArray(const int32_t *in, const size_t length) override {
    __m128i *endofbuf =
        simdpack_length(reinterpret_cast<const uint32_t *>(in), length,
                        (__m128i *)compressed.data(), b);
    int howmanybytes =
        (endofbuf - (__m128i *)compressed.data()) * sizeof(__m128i);
    compressed.resize(howmanybytes);
  }

  void DecodeArray(int32_t *out, const std::size_t length) override {
    uint64_t checksum = 0;
    simdunpack_length((const __m128i *)compressed.data(), length,
                      reinterpret_cast<uint32_t *>(out), b, &checksum);
  }

  std::size_t EncodedNumValues() override { return compressed.size(); }

  std::size_t EncodedSizeValue() override { return sizeof(uint8_t); }

  virtual ~SimdCompCodec() {}

  std::string name() const override { return "simdcomp"; }

  std::size_t GetOverflowSize(size_t) const override { return 0; }

  StatefulIntegerCodec<int32_t> *CloneFresh() const override {
    return new SimdCompCodec();
  }

  void AllocEncoded(const int32_t *in, size_t length) override {
    b = maxbits_length(reinterpret_cast<const uint32_t *>(in), length);
    compressed.resize(simdpack_compressedbytes(length, b));
  };

  void clear() override {
    compressed.clear();
    compressed.shrink_to_fit();
  }

  std::vector<int32_t> &GetEncoded() override {
    throw std::runtime_error(
        "Encoded format does not match input. Cannot forward.");
  };
};