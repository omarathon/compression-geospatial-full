#pragma once

#include <cassert>
#include <cstdint>
#include <string>

#include "generic_codecs.h"
#include "simdcomp.h"

class SimdCompFusedCodecU16 : public StatefulIntegerCodec<uint16_t> {
 public:
  std::vector<uint8_t> compressed;
  uint32_t b;

  void EncodeArray(const uint16_t* in, const size_t length) override {
    __m128i* endofbuf =
        simdpack_length_u16(in, length, (__m128i*)compressed.data(), b);
    int howmanybytes =
        (endofbuf - (__m128i*)compressed.data()) * sizeof(__m128i);
    compressed.resize(howmanybytes);
  }

  void DecodeArray(uint16_t* out, const std::size_t length) override {
    uint32_t checksum = 0;
    simdunpack_length_u16((const __m128i*)compressed.data(), length, out, b,
                          &checksum);
    // Store int32 sum in 2 uint16 overflow slots
    out[length] = static_cast<uint16_t>(checksum & 0xFFFF);
    out[length + 1] = static_cast<uint16_t>(checksum >> 16);
  }

  std::size_t EncodedNumValues() override { return compressed.size(); }

  std::size_t EncodedSizeValue() override { return sizeof(uint8_t); }

  virtual ~SimdCompFusedCodecU16() {}

  std::string name() const override { return "simdcomp_fused"; }

  std::size_t GetOverflowSize(size_t) const override { return 2; }

  StatefulIntegerCodec<uint16_t>* CloneFresh() const override {
    return new SimdCompFusedCodecU16();
  }

  void AllocEncoded(const uint16_t* in, size_t length) override {
    b = maxbits_length_u16(in, length);
    compressed.resize(simdpack_compressedbytes_u16(length, b));
  };

  void clear() override {
    compressed.clear();
    compressed.shrink_to_fit();
  }

  std::vector<uint16_t>& GetEncoded() override {
    throw std::runtime_error(
        "Encoded format does not match input. Cannot forward.");
  };
};
