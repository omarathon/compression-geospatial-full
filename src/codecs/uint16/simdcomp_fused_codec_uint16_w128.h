#pragma once

#include <cassert>
#include <cstdint>
#include <cstdlib>
#include <string>
#include <vector>

#include <immintrin.h>

#include "generic_codecs.h"
#include "delta_scratch_u16.h"

// 128-bit (SSE, 8 uint16 lanes) counterpart of SimdCompFusedCodecU16, for an
// apples-to-apples 128-vs-256 width comparison WITHIN simdcomp's own codegen.
// Backed by the generated minimal variant simdbitpacking_u16_w128.c
// (simdpack_u16_w128 / simdunpack_u16_w128), compiled as its own static lib so
// it is dispatched/non-inlined exactly like the 256-bit kernel in libsimdcomp.a.
extern "C" void simdpack_u16_w128(const uint16_t *in, __m128i *out,
                                  const uint32_t bit);
extern "C" void simdunpack_u16_w128(const __m128i *in, uint16_t *out,
                                    const uint32_t bit, __m128i *sum);
// madd-widen aggregate variant: ~1.5× faster decode, valid only when all values
// < 2^15 (signed madd). The codec stores a 1-byte flag and dispatches on it.
extern "C" void simdunpack_u16_w128_madd(const __m128i *in, uint16_t *out,
                                         const uint32_t bit, __m128i *sum);
#ifdef FOR_DECODE_NOAGG  // benchmark-only: produce OutReg, skip the widening sum
extern "C" void simdunpack_u16_w128_noagg(const __m128i *in, uint16_t *out,
                                          const uint32_t bit, __m128i *sum);
#endif

static constexpr size_t kFusedSubBlockSize128 = 128;

class SimdCompFusedCodecU16_128 : public StatefulIntegerCodec<uint16_t> {
 public:
  std::vector<uint8_t> compressed;

  static inline uint32_t MaxBits128(const uint16_t *sb) {
    uint16_t orall = 0;
    for (size_t i = 0; i < kFusedSubBlockSize128; ++i) orall |= sb[i];
    return orall ? (uint32_t)(32 - __builtin_clz(orall)) : 0u;
  }

  void EncodeArray(const uint16_t *in, const size_t length) override {
    assert(length % kFusedSubBlockSize128 == 0);
    const size_t num_sb = length / kFusedSubBlockSize128;

    // Layout: [madd_safe : uint8][bs : uint8 × num_sb][payloads].
    auto &scratch = GetPackScratch();
    uint8_t *flag = scratch.data();
    uint8_t *bs = scratch.data() + 1;
    uint8_t *out_ptr = bs + num_sb;
    uint16_t orall = 0;  // OR of all values → madd-safe iff top bit never set
    for (size_t k = 0; k < num_sb; ++k) {
      const uint16_t *sb = in + k * kFusedSubBlockSize128;
      uint16_t sb_or = 0;
      for (size_t i = 0; i < kFusedSubBlockSize128; ++i) sb_or |= sb[i];
      orall |= sb_or;
      const uint32_t b_k = sb_or ? (uint32_t)(32 - __builtin_clz(sb_or)) : 0u;
      bs[k] = (uint8_t)b_k;
      simdpack_u16_w128(sb, reinterpret_cast<__m128i *>(out_ptr), b_k);
      out_ptr += (size_t)b_k * sizeof(__m128i);
    }
    *flag = (orall < 0x8000u) ? 1 : 0;
    const size_t actual = (size_t)(out_ptr - scratch.data());
    assert(actual <= scratch.size());
    compressed.assign(scratch.data(), scratch.data() + actual);
  }

  void DecodeArray(uint16_t *out, const std::size_t length) override {
    assert(length % kFusedSubBlockSize128 == 0);
    const size_t num_sb = length / kFusedSubBlockSize128;

    static const bool kForceUnpack = (std::getenv("FORCE_UNPACK") != nullptr);
    const uint8_t madd = kForceUnpack ? 0 : compressed.data()[0];
    const uint8_t *bs = compressed.data() + 1;
    const uint8_t *in_ptr = bs + num_sb;

    __m128i sum = _mm_setzero_si128();
#ifdef FOR_DECODE_NOAGG
    (void)madd;  // benchmark-only: produce OutReg, XOR sink (sums are wrong)
    for (size_t k = 0; k < num_sb; ++k) {
      const uint32_t b_k = bs[k];
      simdunpack_u16_w128_noagg(reinterpret_cast<const __m128i *>(in_ptr),
                                out + k * kFusedSubBlockSize128, b_k, &sum);
      in_ptr += (size_t)b_k * sizeof(__m128i);
    }
    if (false)
#endif
    if (madd) {
      for (size_t k = 0; k < num_sb; ++k) {
        const uint32_t b_k = bs[k];
        simdunpack_u16_w128_madd(reinterpret_cast<const __m128i *>(in_ptr),
                                 out + k * kFusedSubBlockSize128, b_k, &sum);
        in_ptr += (size_t)b_k * sizeof(__m128i);
      }
    } else {
      for (size_t k = 0; k < num_sb; ++k) {
        const uint32_t b_k = bs[k];
        simdunpack_u16_w128(reinterpret_cast<const __m128i *>(in_ptr),
                            out + k * kFusedSubBlockSize128, b_k, &sum);
        in_ptr += (size_t)b_k * sizeof(__m128i);
      }
    }

    // Horizontal sum of the 4 u32 lanes.
    __m128i s = _mm_add_epi32(sum, _mm_shuffle_epi32(sum, _MM_SHUFFLE(1, 0, 3, 2)));
    s = _mm_add_epi32(s, _mm_shuffle_epi32(s, _MM_SHUFFLE(2, 3, 0, 1)));
    const uint32_t total = (uint32_t)_mm_cvtsi128_si32(s);

    out[length] = (uint16_t)(total & 0xFFFF);
    out[length + 1] = (uint16_t)(total >> 16);
  }

  std::size_t EncodedNumValues() override { return compressed.size(); }
  std::size_t EncodedSizeValue() override { return sizeof(uint8_t); }
  virtual ~SimdCompFusedCodecU16_128() {}
  std::string name() const override { return "simdcomp_fused_128"; }
  std::size_t GetOverflowSize(size_t) const override { return 2; }
  StatefulIntegerCodec<uint16_t> *CloneFresh() const override {
    return new SimdCompFusedCodecU16_128();
  }
  void AllocEncoded(const uint16_t *, size_t) override {}
  void clear() override {
    compressed.clear();
    compressed.shrink_to_fit();
  }
  std::vector<uint16_t> &GetEncoded() override {
    throw std::runtime_error("Encoded format does not match input.");
  }
};
