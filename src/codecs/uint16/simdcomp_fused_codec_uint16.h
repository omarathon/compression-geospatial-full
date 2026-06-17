#pragma once

#include <cassert>
#include <cstdint>
#include <cstdlib>
#include <string>
#include <vector>

#include "generic_codecs.h"
#include "predictive_codecs_u16.h"  // ZigzagEnc16
#include "simdcomp.h"
#include "delta_scratch_u16.h"

// Sub-block size for chunked-b bit-packing. 256 matches simdcomp's internal
// SIMDBlockSize_u16 so we can call the existing per-block pack/unpack helpers
// directly with a different bit width per call.
static constexpr size_t kFusedSubBlockSize = 256;

// ── Base SimdComp fused (no transform), with per-256-element b ──────────────
//
// Same chunking as TurboPack256: bit-pack each 256-element sub-block at its
// own b, so a single high-magnitude pixel in one corner of a 65,536-element
// access block doesn't inflate the bit width for the whole encode.
//
// Header layout: [madd_safe : uint8][bs : uint8 × num_sub_blocks][payloads]
// madd_safe = 1 iff all values < 2^15 (enables signed madd-widen aggregate).

// Plain and madd-widen fused-sum unpack (from SimdCompU16W256 static lib).
extern "C" void simdunpack_u16_w256(const __m256i*, uint16_t*, uint32_t, __m256i*);
extern "C" void simdunpack_u16_w256_madd(const __m256i*, uint16_t*, uint32_t, __m256i*);

class SimdCompFusedCodecU16 : public StatefulIntegerCodec<uint16_t> {
 public:
  std::vector<uint8_t> compressed;

  void EncodeArray(const uint16_t* in, const size_t length) override {
    assert(length % kFusedSubBlockSize == 0);
    const size_t num_sb = length / kFusedSubBlockSize;

    // OR-reduce to determine madd safety (all values < 2^15).
    uint16_t orall = 0;
    for (size_t i = 0; i < length; ++i) orall |= in[i];

    auto& scratch = GetPackScratch();
    scratch[0] = (orall < 0x8000u) ? 1u : 0u;  // madd_safe byte
    uint8_t* bs = scratch.data() + 1;
    uint8_t* out_ptr = bs + num_sb;
    for (size_t k = 0; k < num_sb; ++k) {
      const uint16_t* sb = in + k * kFusedSubBlockSize;
      const uint32_t b_k = maxbits_length_u16(sb, kFusedSubBlockSize);
      bs[k] = static_cast<uint8_t>(b_k);
      simdpack_u16(sb, reinterpret_cast<__m256i*>(out_ptr), b_k);
      out_ptr += static_cast<size_t>(b_k) * sizeof(__m256i);
    }
    const size_t actual = static_cast<size_t>(out_ptr - scratch.data());
    assert(actual <= scratch.size());
    compressed.assign(scratch.data(), scratch.data() + actual);
  }

  void DecodeArray(uint16_t* out, const std::size_t length) override {
    assert(length % kFusedSubBlockSize == 0);
    const size_t num_sb = length / kFusedSubBlockSize;

    // madd_safe is baked per block at encode time, so a grid is a madd/unpack
    // MIX. Set FORCE_UNPACK=1 to force the unpack-widen aggregate everywhere
    // (clean unpack-vs-unpack comparisons; read once, no hot-loop overhead).
    static const bool kForceUnpack = (std::getenv("FORCE_UNPACK") != nullptr);
    const bool madd = (compressed.data()[0] != 0) && !kForceUnpack;
    const uint8_t* bs = compressed.data() + 1;
    const uint8_t* in_ptr = bs + num_sb;

    __m256i sum = _mm256_setzero_si256();
    if (madd) {
      for (size_t k = 0; k < num_sb; ++k) {
        const uint32_t b_k = bs[k];
        if (b_k == 0) continue;  // all-zero sub-block: sum += 0, skip nullunpacker
        simdunpack_u16_w256_madd(reinterpret_cast<const __m256i*>(in_ptr),
                                 out + k * kFusedSubBlockSize, b_k, &sum);
        in_ptr += static_cast<size_t>(b_k) * sizeof(__m256i);
      }
    } else {
      for (size_t k = 0; k < num_sb; ++k) {
        const uint32_t b_k = bs[k];
        if (b_k == 0) continue;  // all-zero sub-block: sum += 0, skip nullunpacker
        simdunpack_u16_w256(reinterpret_cast<const __m256i*>(in_ptr),
                            out + k * kFusedSubBlockSize, b_k, &sum);
        in_ptr += static_cast<size_t>(b_k) * sizeof(__m256i);
      }
    }

    __m128i lo = _mm256_castsi256_si128(sum);
    __m128i hi = _mm256_extracti128_si256(sum, 1);
    __m128i s = _mm_add_epi32(lo, hi);
    s = _mm_add_epi32(s, _mm_shuffle_epi32(s, _MM_SHUFFLE(1, 0, 3, 2)));
    s = _mm_add_epi32(s, _mm_shuffle_epi32(s, _MM_SHUFFLE(2, 3, 0, 1)));
    const uint32_t total = static_cast<uint32_t>(_mm_cvtsi128_si32(s));

    out[length] = static_cast<uint16_t>(total & 0xFFFF);
    out[length + 1] = static_cast<uint16_t>(total >> 16);
  }

  std::size_t EncodedNumValues() override { return compressed.size(); }

  std::size_t EncodedSizeValue() override { return sizeof(uint8_t); }

  virtual ~SimdCompFusedCodecU16() {}

  std::string name() const override { return "simdcomp_fused"; }

  std::size_t GetOverflowSize(size_t) const override { return 2; }

  StatefulIntegerCodec<uint16_t>* CloneFresh() const override {
    return new SimdCompFusedCodecU16();
  }

  void AllocEncoded(const uint16_t*, size_t) override {
    // No-op: EncodeArray writes through the shared scratch and `compressed`
    // is sized exactly via assign() at encode time.
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

// ── Chunked-b fused delta codecs (per-SIMD-block bit width) ──────────────────
// Same per-256-element b chunking as the base codec, applied on top of the
// delta+zigzag transform. The LOCAL variant decodes each sub-block with its
// own per-OutReg prefix-sum window; the CARRY variant additionally threads
// the broadcast-carry __m256i across sub-blocks so prev continues correctly.

class SimdCompFusedDeltaLocalCodecU16 : public StatefulIntegerCodec<uint16_t> {
 public:
  std::vector<uint8_t> compressed;

  void EncodeArray(const uint16_t* in, const size_t length) override {
    assert(length % kFusedSubBlockSize == 0);

    // 1. Delta + zigzag (prev resets every 16 elements — LOCAL).
    uint16_t prev = 0;
    for (size_t i = 0; i < length; ++i) {
      if ((i & 15) == 0) prev = 0;
      const uint16_t delta = static_cast<uint16_t>(in[i] - prev);
      s_delta_scratch[i] = ZigzagEnc16(delta);
      prev = in[i];
    }

    // 2. Per-sub-block bit-pack with its own b.
    const size_t num_sb = length / kFusedSubBlockSize;
    uint8_t* bs = compressed.data();
    uint8_t* out_ptr = compressed.data() + num_sb;
    for (size_t k = 0; k < num_sb; ++k) {
      uint16_t* sb_deltas = s_delta_scratch + k * kFusedSubBlockSize;
      const uint32_t b_k =
          maxbits_length_u16(sb_deltas, kFusedSubBlockSize);
      bs[k] = static_cast<uint8_t>(b_k);
      simdpack_u16(sb_deltas, reinterpret_cast<__m256i*>(out_ptr), b_k);
      out_ptr += static_cast<size_t>(b_k) * sizeof(__m256i);
    }
    compressed.resize(out_ptr - compressed.data());
    compressed.shrink_to_fit();
  }

  void DecodeArray(uint16_t* out, const std::size_t length) override {
    assert(length % kFusedSubBlockSize == 0);

    const size_t num_sb = length / kFusedSubBlockSize;
    const uint8_t* bs = compressed.data();
    const uint8_t* in_ptr = compressed.data() + num_sb;

    __m256i sum = _mm256_setzero_si256();
    for (size_t k = 0; k < num_sb; ++k) {
      const uint32_t b_k = bs[k];
      simdunpack_u16_delta_local(
          reinterpret_cast<const __m256i*>(in_ptr),
          out + k * kFusedSubBlockSize, b_k, &sum);
      in_ptr += static_cast<size_t>(b_k) * sizeof(__m256i);
    }

    // Horizontal reduce sum → uint32 stored in 2 overflow slots.
    __m128i lo = _mm256_castsi256_si128(sum);
    __m128i hi = _mm256_extracti128_si256(sum, 1);
    __m128i s = _mm_add_epi32(lo, hi);
    s = _mm_add_epi32(s, _mm_shuffle_epi32(s, _MM_SHUFFLE(1, 0, 3, 2)));
    s = _mm_add_epi32(s, _mm_shuffle_epi32(s, _MM_SHUFFLE(2, 3, 0, 1)));
    const uint32_t total = static_cast<uint32_t>(_mm_cvtsi128_si32(s));

    out[length] = static_cast<uint16_t>(total & 0xFFFF);
    out[length + 1] = static_cast<uint16_t>(total >> 16);
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
    // Worst case: every sub-block needs b=16 → 16 × 32 = 512 bytes payload
    // plus 1 byte header per sub-block.
    const size_t num_sb = length / kFusedSubBlockSize;
    compressed.resize(num_sb + num_sb * 16 * sizeof(__m256i));
    (void)in;
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

  void EncodeArray(const uint16_t* in, const size_t length) override {
    assert(length % kFusedSubBlockSize == 0);

    // 1. Delta + zigzag with continuous prev (CARRY).
    uint16_t prev = 0;
    for (size_t i = 0; i < length; ++i) {
      const uint16_t delta = static_cast<uint16_t>(in[i] - prev);
      s_delta_scratch[i] = ZigzagEnc16(delta);
      prev = in[i];
    }

    // 2. Per-sub-block bit-pack into shared thread-local scratch; assign
    //    exact bytes into `compressed` at the end. No worst-case alloc on
    //    `compressed`, so glibc's freelist stays small per codec instance.
    const size_t num_sb = length / kFusedSubBlockSize;
    auto& scratch = GetPackScratch();
    uint8_t* bs = scratch.data();
    uint8_t* out_ptr = scratch.data() + num_sb;
    for (size_t k = 0; k < num_sb; ++k) {
      uint16_t* sb_deltas = s_delta_scratch + k * kFusedSubBlockSize;
      const uint32_t b_k =
          maxbits_length_u16(sb_deltas, kFusedSubBlockSize);
      bs[k] = static_cast<uint8_t>(b_k);
      simdpack_u16(sb_deltas, reinterpret_cast<__m256i*>(out_ptr), b_k);
      out_ptr += static_cast<size_t>(b_k) * sizeof(__m256i);
    }
    const size_t actual = static_cast<size_t>(out_ptr - scratch.data());
    assert(actual <= scratch.size());
    compressed.assign(scratch.data(), scratch.data() + actual);
  }

  void DecodeArray(uint16_t* out, const std::size_t length) override {
    assert(length % kFusedSubBlockSize == 0);

    const size_t num_sb = length / kFusedSubBlockSize;
    const uint8_t* bs = compressed.data();
    const uint8_t* in_ptr = compressed.data() + num_sb;

    __m256i sum = _mm256_setzero_si256();
    uint16_t carry = 0;  // threaded across sub-blocks
    for (size_t k = 0; k < num_sb; ++k) {
      const uint32_t b_k = bs[k];
      simdunpack_u16_delta_carry(
          reinterpret_cast<const __m256i*>(in_ptr),
          out + k * kFusedSubBlockSize, b_k, &carry, &sum);
      in_ptr += static_cast<size_t>(b_k) * sizeof(__m256i);
    }

    __m128i lo = _mm256_castsi256_si128(sum);
    __m128i hi = _mm256_extracti128_si256(sum, 1);
    __m128i s = _mm_add_epi32(lo, hi);
    s = _mm_add_epi32(s, _mm_shuffle_epi32(s, _MM_SHUFFLE(1, 0, 3, 2)));
    s = _mm_add_epi32(s, _mm_shuffle_epi32(s, _MM_SHUFFLE(2, 3, 0, 1)));
    const uint32_t total = static_cast<uint32_t>(_mm_cvtsi128_si32(s));

    out[length] = static_cast<uint16_t>(total & 0xFFFF);
    out[length + 1] = static_cast<uint16_t>(total >> 16);
  }

  std::size_t EncodedNumValues() override { return compressed.size(); }
  std::size_t EncodedSizeValue() override { return sizeof(uint8_t); }
  virtual ~SimdCompFusedDeltaCarryCodecU16() {}

  std::string name() const override { return "simdcomp_fused_delta_carry"; }

  std::size_t GetOverflowSize(size_t) const override { return 2; }

  StatefulIntegerCodec<uint16_t>* CloneFresh() const override {
    return new SimdCompFusedDeltaCarryCodecU16();
  }

  void AllocEncoded(const uint16_t*, size_t) override {
    // No-op: EncodeArray writes through shared scratch and sizes `compressed`
    // exactly via assign() — no worst-case allocation on `compressed`.
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
