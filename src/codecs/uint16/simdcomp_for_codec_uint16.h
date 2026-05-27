#pragma once

// Frame-of-Reference (FoR) fused codecs over simdcomp's 256-element SIMD blocks.
//
// Three variants:
//   - SimdCompFusedForGlobalCodecU16:        one anchor per 256-element sub-block
//   - SimdCompFusedForLocalCodecU16:         one anchor per OutReg (per 16
//                                            elements). 16 anchors per sub-block.
//   - SimdCompFusedForHierarchicalCodecU16:  one global anchor per sub-block +
//                                            16 small local anchor deltas
//                                            bit-packed at auto-determined width.
//
// All three:
//   - Use the per-sub-block chunked-b layout already used by the base
//     SimdCompFusedCodecU16 (256-element sub-blocks, one byte of `b` each).
//   - Subtract anchors at encode time → unsigned residuals → no zigzag, no
//     prefix-sum, no carry-chain.
//   - At decode, call simdunpack_u16_corrected per sub-block with `corrections`
//     prefilled with broadcasts of each OutReg's anchor. The per-OutReg ADD
//     happens on the OutReg dep chain (honest cost — matches what an NDVI /
//     multiply pipeline would do, not just a sum-fused short-circuit).
//
// REQUIREMENT: `length % 256 == 0`. bench_pipeline -b 256 satisfies this.

#include <cassert>
#include <cstdint>
#include <cstring>
#include <immintrin.h>
#include <string>
#include <vector>

#include "generic_codecs.h"
#include "simdcomp.h"

namespace simdcomp_for_detail {

static constexpr size_t kSubBlockSize = 256;  // = SIMDBlockSize_u16
static constexpr size_t kOutRegsPerSub = 16;  // 256 elements / 16 lanes

// Sequential scalar bit-pack/unpack for small uint16 streams. Used by the
// FoR-hierarchical codec for the local-anchor-delta stream only — that stream
// has ~16 values per sub-block (4096 total per inner block) and is off the
// hot path, so we don't need simdcomp's columnar SIMD layout here. Sequential
// layout makes scalar extraction trivial and self-contained.
//   `b` ∈ [0, 16]. Output is little-endian bit-packed into a uint8 buffer.
//   For b == 0 we treat all values as 0 (skip the buffer).
static inline size_t seq_packed_bytes(size_t n, uint32_t b) {
  // ceil(n * b / 8)
  return (n * static_cast<size_t>(b) + 7) / 8;
}

static inline void seq_pack_u16(const uint16_t* in, size_t n, uint32_t b,
                                 uint8_t* out) {
  if (b == 0) return;
  std::memset(out, 0, seq_packed_bytes(n, b));
  for (size_t i = 0; i < n; ++i) {
    const uint32_t v = static_cast<uint32_t>(in[i]) &
                       ((b >= 32) ? 0xFFFFFFFFu : ((1u << b) - 1u));
    const size_t bit_pos = i * static_cast<size_t>(b);
    const size_t byte_pos = bit_pos / 8;
    const uint32_t shift = static_cast<uint32_t>(bit_pos % 8);
    // Up to b + 7 bits = up to 23 bits to write → fits in 3-4 bytes.
    uint64_t shifted = static_cast<uint64_t>(v) << shift;
    uint8_t* p = out + byte_pos;
    const size_t n_bytes = (shift + b + 7) / 8;
    for (size_t j = 0; j < n_bytes; ++j) {
      p[j] |= static_cast<uint8_t>(shifted & 0xFFu);
      shifted >>= 8;
    }
  }
}

static inline void seq_unpack_u16(const uint8_t* in, size_t n, uint32_t b,
                                   uint16_t* out) {
  if (b == 0) {
    std::memset(out, 0, n * sizeof(uint16_t));
    return;
  }
  const uint32_t mask = (b >= 32) ? 0xFFFFFFFFu : ((1u << b) - 1u);
  for (size_t i = 0; i < n; ++i) {
    const size_t bit_pos = i * static_cast<size_t>(b);
    const size_t byte_pos = bit_pos / 8;
    const uint32_t shift = static_cast<uint32_t>(bit_pos % 8);
    // Load up to 4 bytes into a uint32 (enough for b + 7 bits worst case
    // when b = 16, shift = 7 → 23 bits).
    uint32_t raw = 0;
    const size_t n_bytes = (shift + b + 7) / 8;
    for (size_t j = 0; j < n_bytes; ++j) {
      raw |= static_cast<uint32_t>(in[byte_pos + j]) << (j * 8);
    }
    out[i] = static_cast<uint16_t>((raw >> shift) & mask);
  }
}

// Horizontal-min of a __m256i of 16 uint16 lanes via _mm_minpos_epu16.
static inline uint16_t hmin_epu16_m256(__m256i v) {
  __m128i lo = _mm256_castsi256_si128(v);
  __m128i hi = _mm256_extracti128_si256(v, 1);
  __m128i m = _mm_min_epu16(lo, hi);
  m = _mm_minpos_epu16(m);
  return static_cast<uint16_t>(_mm_cvtsi128_si32(m));
}

// Horizontal-reduce a __m256i (8 × int32) to a uint32 scalar.
static inline uint32_t hsum_epi32_m256(__m256i v) {
  __m128i lo = _mm256_castsi256_si128(v);
  __m128i hi = _mm256_extracti128_si256(v, 1);
  __m128i s = _mm_add_epi32(lo, hi);
  s = _mm_add_epi32(s, _mm_shuffle_epi32(s, _MM_SHUFFLE(1, 0, 3, 2)));
  s = _mm_add_epi32(s, _mm_shuffle_epi32(s, _MM_SHUFFLE(2, 3, 0, 1)));
  return static_cast<uint32_t>(_mm_cvtsi128_si32(s));
}

// Scratch buffer for residuals during encode (thread-local; matches the size
// used by the existing fused-delta codecs). Defined `static` so each TU
// gets its own — same pattern as s_delta_scratch in delta_scratch_u16.h.
static thread_local uint16_t s_for_scratch[256 * 256];

}  // namespace simdcomp_for_detail

// ── FoR-global: one anchor per 256-element sub-block ─────────────────────────
//
// Layout:
//   [num_sb : uint32]
//   [anchors : uint16 × num_sb]
//   [bs : uint8 × num_sb]
//   [payload_0][payload_1] … [payload_{num_sb-1}]
//
// Per sub-block:
//   anchor_k = min over 256 elements of sub-block k
//   residual_i = value_i − anchor_k (always >= 0)
//   b_k = bit width needed for max residual within sub-block k
//
// Decode prefills `corrections[0..15] = broadcast(anchor_k)` (all same value
// for a global-FoR sub-block) before calling simdunpack_u16_corrected.
class SimdCompFusedForGlobalCodecU16 : public StatefulIntegerCodec<uint16_t> {
 public:
  std::vector<uint8_t> compressed;

  void EncodeArray(const uint16_t* in, const size_t length) override {
    using namespace simdcomp_for_detail;
    assert(length % kSubBlockSize == 0);
    const size_t num_sb = length / kSubBlockSize;

    uint8_t* hdr = compressed.data();
    auto* num_sb_ptr = reinterpret_cast<uint32_t*>(hdr);
    *num_sb_ptr = static_cast<uint32_t>(num_sb);
    auto* anchors_ptr = reinterpret_cast<uint16_t*>(hdr + sizeof(uint32_t));
    uint8_t* bs_ptr = reinterpret_cast<uint8_t*>(anchors_ptr + num_sb);
    uint8_t* out_ptr = bs_ptr + num_sb;

    for (size_t k = 0; k < num_sb; ++k) {
      const uint16_t* sb_in = in + k * kSubBlockSize;
      // Find min over 256 elements via SIMD reduction.
      __m256i acc = _mm256_set1_epi16(static_cast<short>(0xFFFFu));
      for (size_t i = 0; i < kSubBlockSize; i += 16) {
        __m256i v = _mm256_loadu_si256(
            reinterpret_cast<const __m256i*>(sb_in + i));
        acc = _mm256_min_epu16(acc, v);
      }
      const uint16_t anchor = hmin_epu16_m256(acc);
      anchors_ptr[k] = anchor;

      // residuals = values - anchor; track max for b.
      uint16_t* dst = s_for_scratch + k * kSubBlockSize;
      __m256i anchor_v = _mm256_set1_epi16(static_cast<short>(anchor));
      __m256i max_acc = _mm256_setzero_si256();
      for (size_t i = 0; i < kSubBlockSize; i += 16) {
        __m256i v = _mm256_loadu_si256(
            reinterpret_cast<const __m256i*>(sb_in + i));
        __m256i r = _mm256_sub_epi16(v, anchor_v);
        _mm256_storeu_si256(reinterpret_cast<__m256i*>(dst + i), r);
        max_acc = _mm256_max_epu16(max_acc, r);
      }
      // Reduce max_acc to scalar via hmax = bitwise OR (cheap, ≥ true max).
      // Actually take a proper horizontal max:
      __m128i lo = _mm256_castsi256_si128(max_acc);
      __m128i hi = _mm256_extracti128_si256(max_acc, 1);
      __m128i mx = _mm_max_epu16(lo, hi);
      // 8-lane uint16 hmax via repeated shuffles:
      mx = _mm_max_epu16(mx, _mm_srli_si128(mx, 8));
      mx = _mm_max_epu16(mx, _mm_srli_si128(mx, 4));
      mx = _mm_max_epu16(mx, _mm_srli_si128(mx, 2));
      uint16_t maxr = static_cast<uint16_t>(_mm_extract_epi16(mx, 0));
      uint32_t b_k = 0;
      while (maxr) {
        b_k++;
        maxr >>= 1;
      }
      bs_ptr[k] = static_cast<uint8_t>(b_k);

      simdpack_u16(dst, reinterpret_cast<__m256i*>(out_ptr), b_k);
      out_ptr += static_cast<size_t>(b_k) * sizeof(__m256i);
    }

    compressed.resize(out_ptr - compressed.data());
    compressed.shrink_to_fit();
  }

  void DecodeArray(uint16_t* out, const std::size_t length) override {
    using namespace simdcomp_for_detail;
    assert(length % kSubBlockSize == 0);
    const size_t num_sb = length / kSubBlockSize;

    const uint8_t* hdr = compressed.data();
    const uint32_t got_num_sb = *reinterpret_cast<const uint32_t*>(hdr);
    (void)got_num_sb;
    assert(got_num_sb == num_sb);
    const auto* anchors_ptr =
        reinterpret_cast<const uint16_t*>(hdr + sizeof(uint32_t));
    const uint8_t* bs_ptr =
        reinterpret_cast<const uint8_t*>(anchors_ptr + num_sb);
    const uint8_t* in_ptr = bs_ptr + num_sb;

    alignas(32) __m256i corrections[kOutRegsPerSub];

    __m256i sum = _mm256_setzero_si256();
    for (size_t k = 0; k < num_sb; ++k) {
      const uint16_t anchor = anchors_ptr[k];
      const __m256i anchor_bcast =
          _mm256_set1_epi16(static_cast<short>(anchor));
      for (size_t i = 0; i < kOutRegsPerSub; ++i) {
        corrections[i] = anchor_bcast;
      }
      const uint32_t b_k = bs_ptr[k];
      simdunpack_u16_corrected(reinterpret_cast<const __m256i*>(in_ptr),
                               out + k * kSubBlockSize, b_k, corrections, &sum);
      in_ptr += static_cast<size_t>(b_k) * sizeof(__m256i);
    }

    const uint32_t total = hsum_epi32_m256(sum);
    out[length] = static_cast<uint16_t>(total & 0xFFFF);
    out[length + 1] = static_cast<uint16_t>(total >> 16);
  }

  std::size_t EncodedNumValues() override { return compressed.size(); }
  std::size_t EncodedSizeValue() override { return sizeof(uint8_t); }
  virtual ~SimdCompFusedForGlobalCodecU16() {}
  std::string name() const override { return "simdcomp_fused_for_global"; }
  std::size_t GetOverflowSize(size_t) const override { return 2; }
  StatefulIntegerCodec<uint16_t>* CloneFresh() const override {
    return new SimdCompFusedForGlobalCodecU16();
  }
  void AllocEncoded(const uint16_t*, size_t length) override {
    const size_t num_sb = length / simdcomp_for_detail::kSubBlockSize;
    // Worst case: every sub-block at b=16, plus header.
    compressed.resize(sizeof(uint32_t)
                      + num_sb * sizeof(uint16_t)
                      + num_sb
                      + num_sb * 16 * sizeof(__m256i));
  }
  void clear() override {
    compressed.clear();
    compressed.shrink_to_fit();
  }
  std::vector<uint16_t>& GetEncoded() override {
    throw std::runtime_error("FoR encoded format does not match input.");
  }
};

// ── FoR-local: one anchor per OutReg (per 16 elements) ───────────────────────
//
// Layout:
//   [num_sb : uint32]
//   [anchors : uint16 × num_sb × 16]   // 16 anchors per sub-block
//   [bs : uint8 × num_sb]
//   [payload_0][payload_1] … [payload_{num_sb-1}]
//
// Per OutReg j of sub-block k:
//   anchor_{k,j} = min over the 16 elements of OutReg j
//   residual_i = value_i − anchor_{k,j}
//   b_k = bit width over the whole sub-block's residuals (256 elements)
class SimdCompFusedForLocalCodecU16 : public StatefulIntegerCodec<uint16_t> {
 public:
  std::vector<uint8_t> compressed;

  void EncodeArray(const uint16_t* in, const size_t length) override {
    using namespace simdcomp_for_detail;
    assert(length % kSubBlockSize == 0);
    const size_t num_sb = length / kSubBlockSize;

    uint8_t* hdr = compressed.data();
    *reinterpret_cast<uint32_t*>(hdr) = static_cast<uint32_t>(num_sb);
    auto* anchors_ptr = reinterpret_cast<uint16_t*>(hdr + sizeof(uint32_t));
    uint8_t* bs_ptr = reinterpret_cast<uint8_t*>(
        anchors_ptr + num_sb * kOutRegsPerSub);
    uint8_t* out_ptr = bs_ptr + num_sb;

    for (size_t k = 0; k < num_sb; ++k) {
      const uint16_t* sb_in = in + k * kSubBlockSize;
      uint16_t* dst = s_for_scratch + k * kSubBlockSize;
      uint16_t* sb_anchors = anchors_ptr + k * kOutRegsPerSub;

      __m256i max_acc = _mm256_setzero_si256();
      for (size_t j = 0; j < kOutRegsPerSub; ++j) {
        __m256i v = _mm256_loadu_si256(
            reinterpret_cast<const __m256i*>(sb_in + j * 16));
        uint16_t anchor = hmin_epu16_m256(v);
        sb_anchors[j] = anchor;
        __m256i a = _mm256_set1_epi16(static_cast<short>(anchor));
        __m256i r = _mm256_sub_epi16(v, a);
        _mm256_storeu_si256(reinterpret_cast<__m256i*>(dst + j * 16), r);
        max_acc = _mm256_max_epu16(max_acc, r);
      }

      // hmax over 16 uint16 lanes:
      __m128i lo = _mm256_castsi256_si128(max_acc);
      __m128i hi = _mm256_extracti128_si256(max_acc, 1);
      __m128i mx = _mm_max_epu16(lo, hi);
      mx = _mm_max_epu16(mx, _mm_srli_si128(mx, 8));
      mx = _mm_max_epu16(mx, _mm_srli_si128(mx, 4));
      mx = _mm_max_epu16(mx, _mm_srli_si128(mx, 2));
      uint16_t maxr = static_cast<uint16_t>(_mm_extract_epi16(mx, 0));
      uint32_t b_k = 0;
      while (maxr) {
        b_k++;
        maxr >>= 1;
      }
      bs_ptr[k] = static_cast<uint8_t>(b_k);

      simdpack_u16(dst, reinterpret_cast<__m256i*>(out_ptr), b_k);
      out_ptr += static_cast<size_t>(b_k) * sizeof(__m256i);
    }

    compressed.resize(out_ptr - compressed.data());
    compressed.shrink_to_fit();
  }

  void DecodeArray(uint16_t* out, const std::size_t length) override {
    using namespace simdcomp_for_detail;
    assert(length % kSubBlockSize == 0);
    const size_t num_sb = length / kSubBlockSize;

    const uint8_t* hdr = compressed.data();
    (void)*reinterpret_cast<const uint32_t*>(hdr);  // num_sb
    const auto* anchors_ptr =
        reinterpret_cast<const uint16_t*>(hdr + sizeof(uint32_t));
    const uint8_t* bs_ptr =
        reinterpret_cast<const uint8_t*>(anchors_ptr + num_sb * kOutRegsPerSub);
    const uint8_t* in_ptr = bs_ptr + num_sb;

    alignas(32) __m256i corrections[kOutRegsPerSub];

    __m256i sum = _mm256_setzero_si256();
    for (size_t k = 0; k < num_sb; ++k) {
      const uint16_t* sb_anchors = anchors_ptr + k * kOutRegsPerSub;
      for (size_t i = 0; i < kOutRegsPerSub; ++i) {
        corrections[i] =
            _mm256_set1_epi16(static_cast<short>(sb_anchors[i]));
      }
      const uint32_t b_k = bs_ptr[k];
      simdunpack_u16_corrected(reinterpret_cast<const __m256i*>(in_ptr),
                               out + k * kSubBlockSize, b_k, corrections, &sum);
      in_ptr += static_cast<size_t>(b_k) * sizeof(__m256i);
    }

    const uint32_t total = hsum_epi32_m256(sum);
    out[length] = static_cast<uint16_t>(total & 0xFFFF);
    out[length + 1] = static_cast<uint16_t>(total >> 16);
  }

  std::size_t EncodedNumValues() override { return compressed.size(); }
  std::size_t EncodedSizeValue() override { return sizeof(uint8_t); }
  virtual ~SimdCompFusedForLocalCodecU16() {}
  std::string name() const override { return "simdcomp_fused_for_local"; }
  std::size_t GetOverflowSize(size_t) const override { return 2; }
  StatefulIntegerCodec<uint16_t>* CloneFresh() const override {
    return new SimdCompFusedForLocalCodecU16();
  }
  void AllocEncoded(const uint16_t*, size_t length) override {
    const size_t num_sb = length / simdcomp_for_detail::kSubBlockSize;
    compressed.resize(sizeof(uint32_t)
                      + num_sb * simdcomp_for_detail::kOutRegsPerSub
                            * sizeof(uint16_t)
                      + num_sb
                      + num_sb * 16 * sizeof(__m256i));
  }
  void clear() override {
    compressed.clear();
    compressed.shrink_to_fit();
  }
  std::vector<uint16_t>& GetEncoded() override {
    throw std::runtime_error("FoR encoded format does not match input.");
  }
};

// ── FoR-hierarchical: global anchor + bit-packed local anchor deltas ─────────
//
// Layout:
//   [num_sb : uint32]
//   [global_anchors : uint16 × num_sb]   // one per sub-block
//   [local_delta_bits : uint8]            // single auto-`b` for ALL anchor
//                                         // deltas (small uniform values)
//   [packed_anchor_deltas : tight, length = num_sb * 16 values @ local_delta_bits]
//   [bs : uint8 × num_sb]                // per-sub-block residual bit width
//   [payload_0]…[payload_{num_sb-1}]
//
// Per OutReg j of sub-block k:
//   anchor_{k,j} = global_anchor_k + local_delta_{k,j}
//   residual_i = value_i − anchor_{k,j}
//
// Local-delta bit-packing uses one global `b` (auto-determined), bit-packed
// via simdcomp's existing per-256 packer. Total local deltas = num_sb × 16;
// we pack them in groups of 256 (16 sub-blocks at a time) via simdpack_u16.
class SimdCompFusedForHierarchicalCodecU16
    : public StatefulIntegerCodec<uint16_t> {
 public:
  std::vector<uint8_t> compressed;

  void EncodeArray(const uint16_t* in, const size_t length) override {
    using namespace simdcomp_for_detail;
    assert(length % kSubBlockSize == 0);
    const size_t num_sb = length / kSubBlockSize;
    const size_t total_local_anchors = num_sb * kOutRegsPerSub;
    // For convenience, require total_local_anchors to be a multiple of 256.
    // num_sb=256, kOutRegsPerSub=16 → 4096, divisible by 256. ✓
    assert(total_local_anchors % kSubBlockSize == 0);

    // Pass 1: compute global anchors (per-sub-block min) and per-OutReg
    // local anchors (per-OutReg min). Store local deltas in temp buffer.
    std::vector<uint16_t> tmp_global(num_sb);
    std::vector<uint16_t> tmp_local_deltas(total_local_anchors);

    for (size_t k = 0; k < num_sb; ++k) {
      const uint16_t* sb_in = in + k * kSubBlockSize;
      // global anchor = min over 256 elements
      __m256i acc = _mm256_set1_epi16(static_cast<short>(0xFFFFu));
      for (size_t i = 0; i < kSubBlockSize; i += 16) {
        acc = _mm256_min_epu16(
            acc, _mm256_loadu_si256(
                     reinterpret_cast<const __m256i*>(sb_in + i)));
      }
      const uint16_t global_anchor = hmin_epu16_m256(acc);
      tmp_global[k] = global_anchor;

      // per-OutReg local anchors and deltas from global
      for (size_t j = 0; j < kOutRegsPerSub; ++j) {
        __m256i v = _mm256_loadu_si256(
            reinterpret_cast<const __m256i*>(sb_in + j * 16));
        const uint16_t local_anchor = hmin_epu16_m256(v);
        tmp_local_deltas[k * kOutRegsPerSub + j] =
            static_cast<uint16_t>(local_anchor - global_anchor);
      }
    }

    // Auto-`b` for local anchor deltas.
    const uint32_t local_delta_bits =
        maxbits_length_u16(tmp_local_deltas.data(), total_local_anchors);

    // Pass 2: compute residuals (value − (global + local_delta) = value −
    // local_anchor) and per-sub-block residual `b`.
    std::vector<uint8_t> tmp_bs(num_sb);
    for (size_t k = 0; k < num_sb; ++k) {
      const uint16_t* sb_in = in + k * kSubBlockSize;
      uint16_t* dst = s_for_scratch + k * kSubBlockSize;
      const uint16_t global_anchor = tmp_global[k];
      __m256i max_acc = _mm256_setzero_si256();
      for (size_t j = 0; j < kOutRegsPerSub; ++j) {
        const uint16_t local_anchor = static_cast<uint16_t>(
            global_anchor + tmp_local_deltas[k * kOutRegsPerSub + j]);
        __m256i v = _mm256_loadu_si256(
            reinterpret_cast<const __m256i*>(sb_in + j * 16));
        __m256i a = _mm256_set1_epi16(static_cast<short>(local_anchor));
        __m256i r = _mm256_sub_epi16(v, a);
        _mm256_storeu_si256(reinterpret_cast<__m256i*>(dst + j * 16), r);
        max_acc = _mm256_max_epu16(max_acc, r);
      }
      __m128i lo = _mm256_castsi256_si128(max_acc);
      __m128i hi = _mm256_extracti128_si256(max_acc, 1);
      __m128i mx = _mm_max_epu16(lo, hi);
      mx = _mm_max_epu16(mx, _mm_srli_si128(mx, 8));
      mx = _mm_max_epu16(mx, _mm_srli_si128(mx, 4));
      mx = _mm_max_epu16(mx, _mm_srli_si128(mx, 2));
      uint16_t maxr = static_cast<uint16_t>(_mm_extract_epi16(mx, 0));
      uint32_t b_k = 0;
      while (maxr) {
        b_k++;
        maxr >>= 1;
      }
      tmp_bs[k] = static_cast<uint8_t>(b_k);
    }

    // Lay out the encoded buffer.
    uint8_t* hdr = compressed.data();
    *reinterpret_cast<uint32_t*>(hdr) = static_cast<uint32_t>(num_sb);
    auto* global_ptr = reinterpret_cast<uint16_t*>(hdr + sizeof(uint32_t));
    std::memcpy(global_ptr, tmp_global.data(), num_sb * sizeof(uint16_t));

    uint8_t* local_delta_bits_ptr =
        reinterpret_cast<uint8_t*>(global_ptr + num_sb);
    *local_delta_bits_ptr = static_cast<uint8_t>(local_delta_bits);

    // Bit-pack the local anchor deltas using the simple sequential layout
    // (see seq_pack_u16/seq_unpack_u16). Off the hot path; small data.
    uint8_t* packed_anchor_deltas_ptr = local_delta_bits_ptr + 1;
    const size_t packed_anchor_bytes =
        seq_packed_bytes(total_local_anchors, local_delta_bits);
    seq_pack_u16(tmp_local_deltas.data(), total_local_anchors,
                 local_delta_bits, packed_anchor_deltas_ptr);

    uint8_t* bs_ptr = packed_anchor_deltas_ptr + packed_anchor_bytes;
    std::memcpy(bs_ptr, tmp_bs.data(), num_sb);
    uint8_t* out_ptr = bs_ptr + num_sb;

    // Bit-pack the residuals per sub-block.
    for (size_t k = 0; k < num_sb; ++k) {
      const uint32_t b_k = tmp_bs[k];
      simdpack_u16(s_for_scratch + k * kSubBlockSize,
                   reinterpret_cast<__m256i*>(out_ptr), b_k);
      out_ptr += static_cast<size_t>(b_k) * sizeof(__m256i);
    }

    compressed.resize(out_ptr - compressed.data());
    compressed.shrink_to_fit();
  }

  void DecodeArray(uint16_t* out, const std::size_t length) override {
    using namespace simdcomp_for_detail;
    assert(length % kSubBlockSize == 0);
    const size_t num_sb = length / kSubBlockSize;
    const size_t total_local_anchors = num_sb * kOutRegsPerSub;

    const uint8_t* hdr = compressed.data();
    (void)*reinterpret_cast<const uint32_t*>(hdr);
    const auto* global_ptr =
        reinterpret_cast<const uint16_t*>(hdr + sizeof(uint32_t));
    const uint8_t local_delta_bits = *reinterpret_cast<const uint8_t*>(
        global_ptr + num_sb);
    const uint8_t* packed_anchor_deltas_ptr =
        reinterpret_cast<const uint8_t*>(global_ptr + num_sb) + 1;

    // Materialise local anchor deltas via the sequential unpack helper.
    // 4096 values per inner block, off the hot path — scalar is fine.
    std::vector<uint16_t> local_deltas(total_local_anchors);
    seq_unpack_u16(packed_anchor_deltas_ptr, total_local_anchors,
                   local_delta_bits, local_deltas.data());

    const size_t packed_anchor_bytes =
        seq_packed_bytes(total_local_anchors, local_delta_bits);
    const uint8_t* bs_ptr = packed_anchor_deltas_ptr + packed_anchor_bytes;
    const uint8_t* in_ptr = bs_ptr + num_sb;

    alignas(32) __m256i corrections[kOutRegsPerSub];
    __m256i sum = _mm256_setzero_si256();

    for (size_t k = 0; k < num_sb; ++k) {
      const uint16_t global_anchor = global_ptr[k];
      for (size_t j = 0; j < kOutRegsPerSub; ++j) {
        const uint16_t local_anchor = static_cast<uint16_t>(
            global_anchor + local_deltas[k * kOutRegsPerSub + j]);
        corrections[j] =
            _mm256_set1_epi16(static_cast<short>(local_anchor));
      }
      const uint32_t b_k = bs_ptr[k];
      simdunpack_u16_corrected(reinterpret_cast<const __m256i*>(in_ptr),
                               out + k * kSubBlockSize, b_k, corrections, &sum);
      in_ptr += static_cast<size_t>(b_k) * sizeof(__m256i);
    }

    const uint32_t total = hsum_epi32_m256(sum);
    out[length] = static_cast<uint16_t>(total & 0xFFFF);
    out[length + 1] = static_cast<uint16_t>(total >> 16);
  }

  std::size_t EncodedNumValues() override { return compressed.size(); }
  std::size_t EncodedSizeValue() override { return sizeof(uint8_t); }
  virtual ~SimdCompFusedForHierarchicalCodecU16() {}
  std::string name() const override {
    return "simdcomp_fused_for_hierarchical";
  }
  std::size_t GetOverflowSize(size_t) const override { return 2; }
  StatefulIntegerCodec<uint16_t>* CloneFresh() const override {
    return new SimdCompFusedForHierarchicalCodecU16();
  }
  void AllocEncoded(const uint16_t*, size_t length) override {
    const size_t num_sb = length / simdcomp_for_detail::kSubBlockSize;
    compressed.resize(
        sizeof(uint32_t)
        + num_sb * sizeof(uint16_t)
        + 1                                                       // local_delta_bits
        + num_sb * simdcomp_for_detail::kOutRegsPerSub * 2        // worst-case packed local deltas (16 bits each)
        + num_sb                                                  // bs
        + num_sb * 16 * sizeof(__m256i));
  }
  void clear() override {
    compressed.clear();
    compressed.shrink_to_fit();
  }
  std::vector<uint16_t>& GetEncoded() override {
    throw std::runtime_error("FoR encoded format does not match input.");
  }
};
