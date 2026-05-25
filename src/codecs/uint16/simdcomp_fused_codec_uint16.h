#pragma once

#include <cassert>
#include <cstdint>
#include <string>
#include <vector>

#include "generic_codecs.h"
#include "predictive_codecs_u16.h"  // ZigzagEnc16
#include "simdcomp.h"
#include "delta_scratch_u16.h"

class SimdCompFusedCodecU16 : public StatefulIntegerCodec<uint16_t> {
 public:
  std::vector<uint8_t> compressed;
  uint32_t b;

  void EncodeArray(const uint16_t* in, const size_t length) override {
    __m256i* endofbuf =
        simdpack_length_u16(in, length, (__m256i*)compressed.data(), b);
    int howmanybytes =
        (endofbuf - (__m256i*)compressed.data()) * sizeof(__m256i);
    compressed.resize(howmanybytes);
  }

  void DecodeArray(uint16_t* out, const std::size_t length) override {
    uint32_t checksum = 0;
    simdunpack_length_u16((const __m256i*)compressed.data(), length, out, b,
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

// ── Fused delta variants ─────────────────────────────────────────────────────
//
// Same encoded format as SimdCompFusedCodecU16 except the encoder pre-applies
// a scalar delta+zigzag transform before bit-packing. The decoder uses a
// per-OutReg SIMD pipeline:
//   zigzag_dec -> prefix_sum [-> +carry -> update carry] -> aggregate.
//
// LOCAL: prev resets to 0 every 16 elements; each OutReg is an independent
//        prefix-sum window. No carry across OutRegs or blocks.
// CARRY: prev persists across the whole stream; SIMD threads a broadcast
//        carry __m256i across OutRegs and blocks, scalar tail seeds from it.

class SimdCompFusedDeltaLocalCodecU16 : public StatefulIntegerCodec<uint16_t> {
 public:
  std::vector<uint8_t> compressed;
  uint32_t b;

  void EncodeArray(const uint16_t* in, const size_t length) override {
    uint16_t prev = 0;
    for (size_t i = 0; i < length; ++i) {
      if ((i & 15) == 0) prev = 0;
      const uint16_t delta = static_cast<uint16_t>(in[i] - prev);
      s_delta_scratch[i] = ZigzagEnc16(delta);
      prev = in[i];
    }
    __m256i* endofbuf = simdpack_length_u16(s_delta_scratch, length,
                                             (__m256i*)compressed.data(), b);
    int howmanybytes =
        (endofbuf - (__m256i*)compressed.data()) * sizeof(__m256i);
    compressed.resize(howmanybytes);
  }

  void DecodeArray(uint16_t* out, const std::size_t length) override {
    uint32_t checksum = 0;
    simdunpack_length_u16_delta_local((const __m256i*)compressed.data(), length,
                                       out, b, &checksum);
    out[length] = static_cast<uint16_t>(checksum & 0xFFFF);
    out[length + 1] = static_cast<uint16_t>(checksum >> 16);
  }

  std::size_t EncodedNumValues() override { return compressed.size(); }
  std::size_t EncodedSizeValue() override { return sizeof(uint8_t); }
  virtual ~SimdCompFusedDeltaLocalCodecU16() {}

  std::string name() const override { return "simdcomp_fused_delta_local"; }

  std::size_t GetOverflowSize(size_t) const override { return 2; }

  StatefulIntegerCodec<uint16_t>* CloneFresh() const override {
    return new SimdCompFusedDeltaLocalCodecU16();
  }

  void AllocEncoded(const uint16_t* in, size_t length) override {
    // Bit-width is determined by maxbits of the TRANSFORMED stream.
    uint16_t prev = 0;
    for (size_t i = 0; i < length; ++i) {
      if ((i & 15) == 0) prev = 0;
      const uint16_t delta = static_cast<uint16_t>(in[i] - prev);
      s_delta_scratch[i] = ZigzagEnc16(delta);
      prev = in[i];
    }
    b = maxbits_length_u16(s_delta_scratch, length);
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

class SimdCompFusedDeltaCarryCodecU16 : public StatefulIntegerCodec<uint16_t> {
 public:
  std::vector<uint8_t> compressed;
  uint32_t b;

  void EncodeArray(const uint16_t* in, const size_t length) override {
    uint16_t prev = 0;
    for (size_t i = 0; i < length; ++i) {
      const uint16_t delta = static_cast<uint16_t>(in[i] - prev);
      s_delta_scratch[i] = ZigzagEnc16(delta);
      prev = in[i];
    }
    __m256i* endofbuf = simdpack_length_u16(s_delta_scratch, length,
                                             (__m256i*)compressed.data(), b);
    int howmanybytes =
        (endofbuf - (__m256i*)compressed.data()) * sizeof(__m256i);
    compressed.resize(howmanybytes);
  }

  void DecodeArray(uint16_t* out, const std::size_t length) override {
    uint32_t checksum = 0;
    simdunpack_length_u16_delta_carry((const __m256i*)compressed.data(), length,
                                       out, b, &checksum);
    out[length] = static_cast<uint16_t>(checksum & 0xFFFF);
    out[length + 1] = static_cast<uint16_t>(checksum >> 16);
  }

  std::size_t EncodedNumValues() override { return compressed.size(); }
  std::size_t EncodedSizeValue() override { return sizeof(uint8_t); }
  virtual ~SimdCompFusedDeltaCarryCodecU16() {}

  std::string name() const override { return "simdcomp_fused_delta_carry"; }

  std::size_t GetOverflowSize(size_t) const override { return 2; }

  StatefulIntegerCodec<uint16_t>* CloneFresh() const override {
    return new SimdCompFusedDeltaCarryCodecU16();
  }

  void AllocEncoded(const uint16_t* in, size_t length) override {
    uint16_t prev = 0;
    for (size_t i = 0; i < length; ++i) {
      const uint16_t delta = static_cast<uint16_t>(in[i] - prev);
      s_delta_scratch[i] = ZigzagEnc16(delta);
      prev = in[i];
    }
    b = maxbits_length_u16(s_delta_scratch, length);
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
