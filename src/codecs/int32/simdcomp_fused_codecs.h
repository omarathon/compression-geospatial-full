#pragma once

#include <cassert>
#include <cstdint>
#include <string>

#include "generic_codecs.h"
#include "simdcomp.h"

// SimdComp variant that writes the decode-time 32-bit checksum (lower 32 bits
// of the SIMD sum) into the two overflow slots following the decoded data.
// Used with AccessTransformation::LinearSumFused.
class SimdCompFusedCodec : public StatefulIntegerCodec<int32_t> {
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
    out[length]     = static_cast<int32_t>(checksum & 0xFFFFFFFF);
    out[length + 1] = static_cast<int32_t>(checksum >> 32);
  }

  std::size_t EncodedNumValues() override { return compressed.size(); }

  std::size_t EncodedSizeValue() override { return sizeof(uint8_t); }

  virtual ~SimdCompFusedCodec() {}

  std::string name() const override { return "simdcomp_fused"; }

  std::size_t GetOverflowSize(size_t) const override { return 2; }

  StatefulIntegerCodec<int32_t> *CloneFresh() const override {
    return new SimdCompFusedCodec();
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
