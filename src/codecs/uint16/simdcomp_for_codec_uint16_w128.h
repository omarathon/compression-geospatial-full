#pragma once

// ── Frame-of-Reference (FoR) fused-sum codecs, 128-bit (SSE) simdcomp ────────
//
// 128-bit counterparts of the 256-bit FoR codecs in
// simdcomp_for_codec_uint16.h. Residuals are bit-packed in fixed 128-element
// blocks with a per-block bit width `b` (chunked-b), exactly like the base
// SimdCompFusedCodecU16_128. The FoR *window* only controls anchor granularity;
// it is independent of the 128-element bit-packing block.
//
// Geometry: a 128-element block = 16 OutRegs × 8 uint16 lanes.
//           element i ↔ OutReg i/8, lane i%8.
//
// Decode is fused-sum: per OutReg we add a `corrections` broadcast (the window
// anchor) on the OutReg dependency chain — the honest per-OutReg add_anchor —
// then aggregate. No decoded values are written (sum-only), like every other
// fused codec here.
//
// Two families:
//   SimdCompFusedForCodecU16_128(window, separate)
//       window ∈ {4,8,16,32,64,128,256}. One anchor (= min) per `window`
//       elements. separate=false packs the anchor stream with the *same*
//       simdpack_u16_w128 (chunked-b) used for residuals; separate=true stores
//       anchors raw uint16.
//   SimdCompFusedForHierarchicalCodecU16_128(outer, inner)
//       outer ∈ {128,256}, inner ∈ {4..256, inner | outer}. One raw global
//       anchor per `outer`; per-`inner` anchor deltas (local_min − global_min)
//       bit-packed at a single global b. (No separate flag — the global
//       anchors are cheap to store raw, so non-separate buys nothing.)
//
// REQUIREMENT: length % 128 == 0, length % window == 0, and (for the packed
// anchor / delta streams) the stream length must be a multiple of 128 — all
// satisfied by bench_pipeline -b 256 (length 65536) for every supported window.

#include <cassert>
#include <cstdint>
#include <cstring>
#include <immintrin.h>
#include <stdexcept>
#include <string>
#include <vector>

#include "generic_codecs.h"
#include "delta_scratch_u16.h"

// 128-bit simdcomp primitives (own static lib SimdCompU16W128).
extern "C" void simdpack_u16_w128(const uint16_t* in, __m128i* out,
                                  const uint32_t bit);
// nobc kernels: aggregate_sums handles both the widen-sum and scalar anchor acc.
// Uniform: 1 anchor/block; cscalarN: shg=N, 1 anchor/OutReg group; half: 2/OutReg.
extern "C" void simdunpack_u16_w128_corrected_uniform_nobc(const __m128i*, uint16_t*, uint32_t,
                                                           const uint16_t* a, uint64_t*, __m128i*);
extern "C" void simdunpack_u16_w128_cscalar0_nobc(const __m128i*, uint16_t*, uint32_t, const uint16_t*, uint64_t*, __m128i*);
extern "C" void simdunpack_u16_w128_cscalar1_nobc(const __m128i*, uint16_t*, uint32_t, const uint16_t*, uint64_t*, __m128i*);
extern "C" void simdunpack_u16_w128_cscalar2_nobc(const __m128i*, uint16_t*, uint32_t, const uint16_t*, uint64_t*, __m128i*);
extern "C" void simdunpack_u16_w128_cscalar3_nobc(const __m128i*, uint16_t*, uint32_t, const uint16_t*, uint64_t*, __m128i*);
extern "C" void simdunpack_u16_w128_corrected_half_nobc(const __m128i*, uint16_t*, uint32_t, const uint16_t*, uint64_t*, __m128i*);
extern "C" void simdunpack_u16_w128_corrected_uniform_nobc_madd(const __m128i*, uint16_t*, uint32_t,
                                                                const uint16_t* a, uint64_t*, __m128i*);
extern "C" void simdunpack_u16_w128_cscalar0_nobc_madd(const __m128i*, uint16_t*, uint32_t, const uint16_t*, uint64_t*, __m128i*);
extern "C" void simdunpack_u16_w128_cscalar1_nobc_madd(const __m128i*, uint16_t*, uint32_t, const uint16_t*, uint64_t*, __m128i*);
extern "C" void simdunpack_u16_w128_cscalar2_nobc_madd(const __m128i*, uint16_t*, uint32_t, const uint16_t*, uint64_t*, __m128i*);
extern "C" void simdunpack_u16_w128_cscalar3_nobc_madd(const __m128i*, uint16_t*, uint32_t, const uint16_t*, uint64_t*, __m128i*);
extern "C" void simdunpack_u16_w128_corrected_half_nobc_madd(const __m128i*, uint16_t*, uint32_t, const uint16_t*, uint64_t*, __m128i*);
extern "C" void simdunpack_u16_w128_store(const __m128i* in, uint16_t* out,
                                          const uint32_t bit);
extern "C" void simdunpack_u16_w128_corrected_uniform(const __m128i* in,
                                                      uint16_t* out,
                                                      const uint32_t bit,
                                                      const __m128i anchor,
                                                      __m128i* sum);
// Per-OutReg anchor broadcast straight from the raw uint16 anchor stream, one
// variant per shg = log2(w/8) (w = 8,16,32,64). shg is compile-time so repeated
// broadcasts (w>8) are CSE'd: cscalar3 (w64) emits just 2 broadcasts/block.
extern "C" void simdunpack_u16_w128_cscalar0(const __m128i*, uint16_t*, uint32_t,
                                             const uint16_t* a_block, __m128i*);
extern "C" void simdunpack_u16_w128_cscalar1(const __m128i*, uint16_t*, uint32_t,
                                             const uint16_t* a_block, __m128i*);
extern "C" void simdunpack_u16_w128_cscalar2(const __m128i*, uint16_t*, uint32_t,
                                             const uint16_t* a_block, __m128i*);
extern "C" void simdunpack_u16_w128_cscalar3(const __m128i*, uint16_t*, uint32_t,
                                             const uint16_t* a_block, __m128i*);
// w==4: each OutReg's correction is half a_block[2v], half a_block[2v+1],
// built inline (two broadcasts + blend) — no corrections array.
extern "C" void simdunpack_u16_w128_corrected_half(const __m128i*, uint16_t*,
                                                   uint32_t,
                                                   const uint16_t* a_block,
                                                   __m128i*);
// w==4 shuffle variant: vpshufb+vmovq instead of 2×vpbroadcastw+vpblendw.
// Saves 2 port-5 correction ops per OutReg (3→1). 128-bit only.
extern "C" void simdunpack_u16_w128_corrected_half_shuf(const __m128i*, uint16_t*,
                                                        uint32_t,
                                                        const uint16_t* a_block,
                                                        __m128i*);
// w>=8 shuffle variants: vmovq+vpshufb(word0) instead of vpbroadcastw.
// Tests whether batched 8-byte loads beat scalar 2-byte loads for cscalar modes.
extern "C" void simdunpack_u16_w128_cscalar_shuf0(const __m128i*, uint16_t*, uint32_t,
                                                  const uint16_t*, __m128i*);
extern "C" void simdunpack_u16_w128_cscalar_shuf1(const __m128i*, uint16_t*, uint32_t,
                                                  const uint16_t*, __m128i*);
extern "C" void simdunpack_u16_w128_cscalar_shuf2(const __m128i*, uint16_t*, uint32_t,
                                                  const uint16_t*, __m128i*);
extern "C" void simdunpack_u16_w128_cscalar_shuf3(const __m128i*, uint16_t*, uint32_t,
                                                  const uint16_t*, __m128i*);
// madd-widen aggregate variants (~1.5× decode): valid only when all decoded
// values < 2^15. The codec stores a 1-byte flag and dispatches on it.
extern "C" void simdunpack_u16_w128_corrected_uniform_madd(const __m128i*, uint16_t*,
                                                           uint32_t, const __m128i, __m128i*);
extern "C" void simdunpack_u16_w128_cscalar0_madd(const __m128i*, uint16_t*, uint32_t,
                                                  const uint16_t*, __m128i*);
extern "C" void simdunpack_u16_w128_cscalar1_madd(const __m128i*, uint16_t*, uint32_t,
                                                  const uint16_t*, __m128i*);
extern "C" void simdunpack_u16_w128_cscalar2_madd(const __m128i*, uint16_t*, uint32_t,
                                                  const uint16_t*, __m128i*);
extern "C" void simdunpack_u16_w128_cscalar3_madd(const __m128i*, uint16_t*, uint32_t,
                                                  const uint16_t*, __m128i*);
extern "C" void simdunpack_u16_w128_corrected_half_madd(const __m128i*, uint16_t*,
                                                        uint32_t, const uint16_t*, __m128i*);
extern "C" void simdunpack_u16_w128_corrected_half_shuf_madd(const __m128i*, uint16_t*,
                                                             uint32_t, const uint16_t*, __m128i*);
extern "C" void simdunpack_u16_w128_cscalar_shuf0_madd(const __m128i*, uint16_t*, uint32_t,
                                                       const uint16_t*, __m128i*);
extern "C" void simdunpack_u16_w128_cscalar_shuf1_madd(const __m128i*, uint16_t*, uint32_t,
                                                       const uint16_t*, __m128i*);
extern "C" void simdunpack_u16_w128_cscalar_shuf2_madd(const __m128i*, uint16_t*, uint32_t,
                                                       const uint16_t*, __m128i*);
extern "C" void simdunpack_u16_w128_cscalar_shuf3_madd(const __m128i*, uint16_t*, uint32_t,
                                                       const uint16_t*, __m128i*);
#ifdef FOR_DECODE_NOAGG  // benchmark-only: produce OutReg, skip the widening sum
extern "C" void simdunpack_u16_w128_corrected_uniform_noagg(const __m128i*, uint16_t*,
                                                            uint32_t, const __m128i, __m128i*);
extern "C" void simdunpack_u16_w128_cscalar0_noagg(const __m128i*, uint16_t*, uint32_t, const uint16_t*, __m128i*);
extern "C" void simdunpack_u16_w128_cscalar1_noagg(const __m128i*, uint16_t*, uint32_t, const uint16_t*, __m128i*);
extern "C" void simdunpack_u16_w128_cscalar2_noagg(const __m128i*, uint16_t*, uint32_t, const uint16_t*, __m128i*);
extern "C" void simdunpack_u16_w128_cscalar3_noagg(const __m128i*, uint16_t*, uint32_t, const uint16_t*, __m128i*);
extern "C" void simdunpack_u16_w128_corrected_half_noagg(const __m128i*, uint16_t*, uint32_t, const uint16_t*, __m128i*);
#endif

namespace simdcomp_for_w128_detail {

static constexpr size_t kBlk = 128;       // residual bit-pack block
static constexpr size_t kOutRegs = 16;    // 128 / 8 lanes
static constexpr size_t kLanes = 8;       // uint16 lanes per __m128i

// Residuals (encode) and materialised anchors / deltas (decode). Separate
// thread-local buffers so encode and decode never alias. 128 KB each.
static thread_local uint16_t s_res[256 * 256];
static thread_local uint16_t s_anchor[256 * 256];  // local anchors per inner window
static thread_local uint16_t s_delta[256 * 256];   // hierarchical anchor deltas
static inline uint32_t bits_u16(uint16_t v) {
  return v ? (uint32_t)(32 - __builtin_clz((uint32_t)v)) : 0u;
}

// max bit width over n contiguous uint16 (OR-reduce, scalar — off hot path).
static inline uint32_t maxbits_n(const uint16_t* p, size_t n) {
  uint16_t orall = 0;
  for (size_t i = 0; i < n; ++i) orall |= p[i];
  return bits_u16(orall);
}

// horizontal max of 8 uint16 lanes.
static inline uint16_t hmax8(__m128i m) {
  m = _mm_max_epu16(m, _mm_srli_si128(m, 8));
  m = _mm_max_epu16(m, _mm_srli_si128(m, 4));
  m = _mm_max_epu16(m, _mm_srli_si128(m, 2));
  return (uint16_t)_mm_extract_epi16(m, 0);
}

// horizontal sum of 4 int32 lanes.
static inline uint32_t hsum4(__m128i s) {
  s = _mm_add_epi32(s, _mm_shuffle_epi32(s, _MM_SHUFFLE(1, 0, 3, 2)));
  s = _mm_add_epi32(s, _mm_shuffle_epi32(s, _MM_SHUFFLE(2, 3, 0, 1)));
  return (uint32_t)_mm_cvtsi128_si32(s);
}

// Build the 16 per-OutReg correction broadcasts for 128-block `k`.
// `anchors` is the per-window anchor array (regular: window mins; hierarchical:
// materialised local anchors). OutReg j covers elements [128k+8j, +8).
//   w >= 8 : each OutReg lies entirely in one window → one broadcast.
//   w == 4 : each OutReg straddles two windows → lanes 0-3 = anchor of the low
//            window, lanes 4-7 = anchor of the high window.
// All supported windows are powers of two, so the window index is `elem >> sh`
// (sh = log2 w) — never a runtime integer division on the hot path.
static inline void build_corrections(__m128i corr[16], const uint16_t* anchors,
                                     size_t k, size_t w) {
  const size_t base = k * kBlk;
  const unsigned sh = (unsigned)__builtin_ctzll((unsigned long long)w);
  if (w >= kLanes) {
    for (size_t j = 0; j < kOutRegs; ++j)
      corr[j] = _mm_set1_epi16((short)anchors[(base + kLanes * j) >> sh]);
  } else {  // w == 4: two windows per OutReg (lanes 0-3 / 4-7)
    for (size_t j = 0; j < kOutRegs; ++j) {
      const size_t elem = base + kLanes * j;
      const short aLo = (short)anchors[elem >> sh];
      const short aHi = (short)anchors[(elem + 4) >> sh];
      corr[j] = _mm_set_epi16(aHi, aHi, aHi, aHi, aLo, aLo, aLo, aLo);
    }
  }
}

// Decode mode, chosen once per decode from the window `w`:
//   kModeUniform (w >= 128): one anchor per 128-block → single broadcast.
//   kModeScalar  (8 <= w < 128): per-OutReg anchor, broadcast straight from the
//       raw uint16 anchor stream (no __m128i corrections array to store/reload).
//   kModeArray   (w == 4): two anchors per OutReg → materialise corrections[16].
enum { kModeArray = 0, kModeScalar = 1, kModeUniform = 2 };
static inline int decode_mode(size_t w) {
  return (w >= kBlk) ? kModeUniform : (w >= kLanes ? kModeScalar : kModeArray);
}

// Decode one 128-block. `sh` = log2(w). All FoR overhead beyond plain bitpack
// lives here (the per-OutReg add). Three kernels, by anchor granularity:
//   uniform (w>=128): one broadcast / block; scalar (8<=w<64): one rolling
//   broadcast per window group; half (w==4): inline [a×4,b×4] per OutReg.
// None materialise a corrections array on the decode hot path.
// `madd`: use the faster madd-widen aggregate (set by the encoder when all
// decoded values < 2^15). `sh` and `mode` are loop-invariant per decode, so the
// branches are perfectly predicted.
static inline void decode_block(const __m128i*& in_ptr, uint16_t* out_k,
                                const uint16_t* anchors, size_t k, size_t w,
                                unsigned sh, int mode, bool madd, uint32_t b_k,
                                __m128i* sum, bool shuf = false,
                                uint64_t* scalar_acc = nullptr) {
  if (scalar_acc) {
    // nobc path: aggregate_sums_u16_nobc* handles both the widen-sum and the
    // scalar anchor accumulation inline, per OutReg.
    if (mode == kModeUniform) {
      const uint16_t* a = anchors + ((k * kBlk) >> sh);
      if (madd) simdunpack_u16_w128_corrected_uniform_nobc_madd(in_ptr, out_k, b_k, a, scalar_acc, sum);
      else      simdunpack_u16_w128_corrected_uniform_nobc(in_ptr, out_k, b_k, a, scalar_acc, sum);
    } else if (mode == kModeScalar) {
      const uint16_t* a_block = anchors + ((k * kBlk) >> sh);
      if (madd) {
        switch (sh) {
          case 3:  simdunpack_u16_w128_cscalar0_nobc_madd(in_ptr, out_k, b_k, a_block, scalar_acc, sum); break;
          case 4:  simdunpack_u16_w128_cscalar1_nobc_madd(in_ptr, out_k, b_k, a_block, scalar_acc, sum); break;
          case 5:  simdunpack_u16_w128_cscalar2_nobc_madd(in_ptr, out_k, b_k, a_block, scalar_acc, sum); break;
          default: simdunpack_u16_w128_cscalar3_nobc_madd(in_ptr, out_k, b_k, a_block, scalar_acc, sum); break;
        }
      } else {
        switch (sh) {
          case 3:  simdunpack_u16_w128_cscalar0_nobc(in_ptr, out_k, b_k, a_block, scalar_acc, sum); break;
          case 4:  simdunpack_u16_w128_cscalar1_nobc(in_ptr, out_k, b_k, a_block, scalar_acc, sum); break;
          case 5:  simdunpack_u16_w128_cscalar2_nobc(in_ptr, out_k, b_k, a_block, scalar_acc, sum); break;
          default: simdunpack_u16_w128_cscalar3_nobc(in_ptr, out_k, b_k, a_block, scalar_acc, sum); break;
        }
      }
    } else {  // kModeArray (w == 4) — half correction
      const uint16_t* a_block = anchors + ((k * kBlk) >> sh);
      if (madd) simdunpack_u16_w128_corrected_half_nobc_madd(in_ptr, out_k, b_k, a_block, scalar_acc, sum);
      else      simdunpack_u16_w128_corrected_half_nobc(in_ptr, out_k, b_k, a_block, scalar_acc, sum);
    }
    in_ptr += b_k;
    return;
  }
#ifdef FOR_DECODE_NOAGG
  (void)madd;  // benchmark-only: produce OutReg, XOR sink (sums are wrong)
  if (mode == kModeUniform) {
    const __m128i a = _mm_set1_epi16((short)anchors[(k * kBlk) >> sh]);
    simdunpack_u16_w128_corrected_uniform_noagg(in_ptr, out_k, b_k, a, sum);
  } else if (mode == kModeScalar) {
    const uint16_t* a_block = anchors + ((k * kBlk) >> sh);
    switch (sh) {
      case 3:  simdunpack_u16_w128_cscalar0_noagg(in_ptr, out_k, b_k, a_block, sum); break;
      case 4:  simdunpack_u16_w128_cscalar1_noagg(in_ptr, out_k, b_k, a_block, sum); break;
      case 5:  simdunpack_u16_w128_cscalar2_noagg(in_ptr, out_k, b_k, a_block, sum); break;
      default: simdunpack_u16_w128_cscalar3_noagg(in_ptr, out_k, b_k, a_block, sum); break;
    }
  } else {
    const uint16_t* a_block = anchors + ((k * kBlk) >> sh);
    simdunpack_u16_w128_corrected_half_noagg(in_ptr, out_k, b_k, a_block, sum);
  }
  in_ptr += b_k;
  return;
#endif
  if (mode == kModeUniform) {
    const __m128i a = _mm_set1_epi16((short)anchors[(k * kBlk) >> sh]);
    if (madd) simdunpack_u16_w128_corrected_uniform_madd(in_ptr, out_k, b_k, a, sum);
    else      simdunpack_u16_w128_corrected_uniform(in_ptr, out_k, b_k, a, sum);
  } else if (mode == kModeScalar) {
    const uint16_t* a_block = anchors + ((k * kBlk) >> sh);
    if (shuf) {
      if (madd) {
        switch (sh) {
          case 3:  simdunpack_u16_w128_cscalar_shuf0_madd(in_ptr, out_k, b_k, a_block, sum); break;
          case 4:  simdunpack_u16_w128_cscalar_shuf1_madd(in_ptr, out_k, b_k, a_block, sum); break;
          case 5:  simdunpack_u16_w128_cscalar_shuf2_madd(in_ptr, out_k, b_k, a_block, sum); break;
          default: simdunpack_u16_w128_cscalar_shuf3_madd(in_ptr, out_k, b_k, a_block, sum); break;
        }
      } else {
        switch (sh) {
          case 3:  simdunpack_u16_w128_cscalar_shuf0(in_ptr, out_k, b_k, a_block, sum); break;
          case 4:  simdunpack_u16_w128_cscalar_shuf1(in_ptr, out_k, b_k, a_block, sum); break;
          case 5:  simdunpack_u16_w128_cscalar_shuf2(in_ptr, out_k, b_k, a_block, sum); break;
          default: simdunpack_u16_w128_cscalar_shuf3(in_ptr, out_k, b_k, a_block, sum); break;
        }
      }
    } else if (madd) {
      switch (sh) {
        case 3:  simdunpack_u16_w128_cscalar0_madd(in_ptr, out_k, b_k, a_block, sum); break;
        case 4:  simdunpack_u16_w128_cscalar1_madd(in_ptr, out_k, b_k, a_block, sum); break;
        case 5:  simdunpack_u16_w128_cscalar2_madd(in_ptr, out_k, b_k, a_block, sum); break;
        default: simdunpack_u16_w128_cscalar3_madd(in_ptr, out_k, b_k, a_block, sum); break;
      }
    } else {
      switch (sh) {
        case 3:  simdunpack_u16_w128_cscalar0(in_ptr, out_k, b_k, a_block, sum); break;
        case 4:  simdunpack_u16_w128_cscalar1(in_ptr, out_k, b_k, a_block, sum); break;
        case 5:  simdunpack_u16_w128_cscalar2(in_ptr, out_k, b_k, a_block, sum); break;
        default: simdunpack_u16_w128_cscalar3(in_ptr, out_k, b_k, a_block, sum); break;
      }
    }
  } else {  // kModeArray: w == 4 — half/half or shuffle correction, no array
    const uint16_t* a_block = anchors + ((k * kBlk) >> sh);
    if (shuf) {
      if (madd) simdunpack_u16_w128_corrected_half_shuf_madd(in_ptr, out_k, b_k, a_block, sum);
      else      simdunpack_u16_w128_corrected_half_shuf(in_ptr, out_k, b_k, a_block, sum);
    } else {
      if (madd) simdunpack_u16_w128_corrected_half_madd(in_ptr, out_k, b_k, a_block, sum);
      else      simdunpack_u16_w128_corrected_half(in_ptr, out_k, b_k, a_block, sum);
    }
  }
  in_ptr += b_k;
}

}  // namespace simdcomp_for_w128_detail

// ── Regular FoR ──────────────────────────────────────────────────────────────
//
// Layout (separate=true):
//   [num_blk : uint32][anchors : uint16 × num_anchors]
//   [bs : uint8 × num_blk][payload_0]…[payload_{num_blk-1}]
// Layout (separate=false):
//   [num_blk : uint32][anchor_bs : uint8 × (num_anchors/128)][anchor payloads]
//   [bs : uint8 × num_blk][payload_0]…
class SimdCompFusedForCodecU16_128 : public StatefulIntegerCodec<uint16_t> {
 public:
  std::vector<uint8_t> compressed;
  size_t window_;
  bool separate_;
  FusedAggImpl agg_;
  bool shuf_;  // use vpshufb correction (all windows): saves port-5 ops/OutReg
  bool nobc_;  // scalar-anchor: sum(v+a)=sum(v)+w*sum(a); no SIMD correction

  explicit SimdCompFusedForCodecU16_128(size_t window = 8, bool separate = false,
                                        FusedAggImpl agg = FusedAggImpl::kMadd,
                                        bool shuf = false, bool nobc = false)
      : window_(window), separate_(separate), agg_(agg), shuf_(shuf), nobc_(nobc) {
    assert(window == 4 || window == 8 || window == 16 || window == 32 ||
           window == 64 || window == 128 || window == 256);
  }

  void EncodeArray(const uint16_t* in, const size_t length) override {
    using namespace simdcomp_for_w128_detail;
    const size_t w = (window_ >= length) ? length : window_;
    assert(length % kBlk == 0);
    assert(length % w == 0);
    const size_t num_blk = length / kBlk;
    const size_t num_anchors = length / w;

    // 1. Per-window anchors (= min). Scalar one-pass; off the decode hot path.
    uint16_t* anchors = s_anchor;
    for (size_t a = 0; a < num_anchors; ++a) {
      const uint16_t* p = in + a * w;
      uint16_t m = p[0];
      for (size_t i = 1; i < w; ++i)
        if (p[i] < m) m = p[i];
      anchors[a] = m;
    }

    // 2. Lay out header + anchor region into the shared pack scratch.
    //    Layout: [num_blk u32][madd_safe u8][anchor region][bs][payloads].
    //    madd_safe = all decoded values (= originals) < 2^15.
    uint16_t orall = 0;
    for (size_t i = 0; i < length; ++i) orall |= in[i];
    auto& scratch = GetPackScratch();
    uint8_t* base = scratch.data();
    *reinterpret_cast<uint32_t*>(base) = (uint32_t)num_blk;
    base[sizeof(uint32_t)] = (orall < 0x8000u) ? 1 : 0;
    uint8_t* cur = base + sizeof(uint32_t) + 1;
    if (separate_) {
      std::memcpy(cur, anchors, num_anchors * sizeof(uint16_t));
      cur += num_anchors * sizeof(uint16_t);
    } else {
      // Pack the anchor stream with the same simdpack_u16_w128 used for
      // residuals (chunked-b). Pad to a 128-multiple with zeros (never read
      // back) so any length works.
      const size_t na_blk = (num_anchors + kBlk - 1) / kBlk;
      for (size_t i = num_anchors; i < na_blk * kBlk; ++i) anchors[i] = 0;
      uint8_t* abs = cur;
      uint8_t* apay = abs + na_blk;
      for (size_t ab = 0; ab < na_blk; ++ab) {
        const uint16_t* ablk = anchors + ab * kBlk;
        const uint32_t ab_b = maxbits_n(ablk, kBlk);
        abs[ab] = (uint8_t)ab_b;
        simdpack_u16_w128(ablk, reinterpret_cast<__m128i*>(apay), ab_b);
        apay += (size_t)ab_b * sizeof(__m128i);
      }
      cur = apay;
    }

    // 3. Residuals per 128-block + chunked b; bit-pack into scratch.
    uint8_t* bs = cur;
    uint8_t* out_ptr = bs + num_blk;
    for (size_t k = 0; k < num_blk; ++k) {
      __m128i corr[16];
      build_corrections(corr, anchors, k, w);
      uint16_t* dst = s_res + k * kBlk;
      __m128i max_acc = _mm_setzero_si128();
      for (size_t j = 0; j < kOutRegs; ++j) {
        __m128i v = _mm_loadu_si128(
            reinterpret_cast<const __m128i*>(in + k * kBlk + j * kLanes));
        __m128i r = _mm_sub_epi16(v, corr[j]);
        _mm_storeu_si128(reinterpret_cast<__m128i*>(dst + j * kLanes), r);
        max_acc = _mm_max_epu16(max_acc, r);
      }
      const uint32_t b_k = bits_u16(hmax8(max_acc));
      bs[k] = (uint8_t)b_k;
      simdpack_u16_w128(dst, reinterpret_cast<__m128i*>(out_ptr), b_k);
      out_ptr += (size_t)b_k * sizeof(__m128i);
    }

    const size_t actual = (size_t)(out_ptr - scratch.data());
    assert(actual <= scratch.size());
    compressed.assign(scratch.data(), scratch.data() + actual);
  }

  void DecodeArray(uint16_t* out, const std::size_t length) override {
    using namespace simdcomp_for_w128_detail;
    const size_t w = (window_ >= length) ? length : window_;
    const size_t num_blk = length / kBlk;
    const size_t num_anchors = length / w;

    const uint8_t* p = compressed.data();
    const uint32_t got = *reinterpret_cast<const uint32_t*>(p);
    (void)got;
    assert(got == num_blk);
    p += sizeof(uint32_t);
    const bool madd_safe = *p++ != 0;
    const bool madd = (agg_ == FusedAggImpl::kMadd) && madd_safe;

    const unsigned sh = (unsigned)__builtin_ctzll((unsigned long long)w);
    const int mode = decode_mode(w);
    __m128i sum = _mm_setzero_si128();
    uint64_t scalar_acc = 0;
    uint64_t* const sacc = nobc_ ? &scalar_acc : nullptr;

    if (separate_) {
      // Anchors raw, in place — no materialisation pass.
      const uint16_t* anchors = reinterpret_cast<const uint16_t*>(p);
      p += num_anchors * sizeof(uint16_t);
      const uint8_t* bs = p;
      const __m128i* in_ptr = reinterpret_cast<const __m128i*>(bs + num_blk);
      for (size_t k = 0; k < num_blk; ++k)
        decode_block(in_ptr, out + k * kBlk, anchors, k, w, sh, mode, madd,
                     bs[k], &sum, shuf_, sacc);
    } else {
      // Anchors SIMD-packed (chunked-b). FUSED: unpack each 128-anchor block,
      // then immediately decode the `w` residual blocks it feeds — the anchors
      // stay hot in L1 instead of being swept twice through a full s_anchor.
      // One full anchor block (128 windows) spans exactly `w` 128-element
      // residual blocks; the last block may be padded so k1 clamps to num_blk.
      const size_t na_blk = (num_anchors + kBlk - 1) / kBlk;
      const uint8_t* abs = p;
      const uint8_t* apay = abs + na_blk;
      size_t apay_bytes = 0;
      for (size_t ab = 0; ab < na_blk; ++ab)
        apay_bytes += (size_t)abs[ab] * sizeof(__m128i);
      const uint8_t* bs = apay + apay_bytes;
      const __m128i* in_ptr = reinterpret_cast<const __m128i*>(bs + num_blk);

      const uint8_t* acur = apay;
      for (size_t ab = 0; ab < na_blk; ++ab) {
        simdunpack_u16_w128_store(reinterpret_cast<const __m128i*>(acur),
                                  s_anchor + ab * kBlk, abs[ab]);
        acur += (size_t)abs[ab] * sizeof(__m128i);
        const size_t k0 = ab * w;
        const size_t k1 = ((ab + 1) * w < num_blk) ? (ab + 1) * w : num_blk;
        for (size_t k = k0; k < k1; ++k)
          decode_block(in_ptr, out + k * kBlk, s_anchor, k, w, sh, mode, madd,
                       bs[k], &sum, shuf_, sacc);
      }
    }

    const uint32_t simd_total = hsum4(sum);
    const uint32_t total = nobc_ ? (simd_total + (uint32_t)scalar_acc) : simd_total;
    out[length] = (uint16_t)(total & 0xFFFF);
    out[length + 1] = (uint16_t)(total >> 16);
  }

  std::size_t EncodedNumValues() override { return compressed.size(); }
  std::size_t EncodedSizeValue() override { return sizeof(uint8_t); }
  virtual ~SimdCompFusedForCodecU16_128() {}
  std::string name() const override {
    std::string n = "simdcomp_fused_for_128_w" + std::to_string(window_);
    if (separate_) n += "_sep";
    if (nobc_) n += "_nobc";
    else if (shuf_) n += "_shuf";
    n += (agg_ == FusedAggImpl::kMadd) ? "_madd" : "_unpack";
    return n;
  }
  std::size_t GetOverflowSize(size_t) const override { return 2; }
  StatefulIntegerCodec<uint16_t>* CloneFresh() const override {
    return new SimdCompFusedForCodecU16_128(window_, separate_, agg_, shuf_, nobc_);
  }
  void AllocEncoded(const uint16_t*, size_t) override {}
  void clear() override {
    compressed.clear();
    compressed.shrink_to_fit();
  }
  std::vector<uint16_t>& GetEncoded() override {
    throw std::runtime_error("FoR encoded format does not match input.");
  }
};

// ── Hierarchical FoR ─────────────────────────────────────────────────────────
//
// Layout:
//   [num_blk : uint32]
//   [global anchors : uint16 × num_outer]     // raw (cheap)
//   [b_local : uint8]                          // single bit width for all deltas
//   [delta payload : (num_inner/128) blocks @ b_local]   // value = local−global
//   [bs : uint8 × num_blk][payload_0]…
class SimdCompFusedForHierarchicalCodecU16_128
    : public StatefulIntegerCodec<uint16_t> {
 public:
  std::vector<uint8_t> compressed;
  size_t outer_;
  size_t inner_;
  FusedAggImpl agg_;

  SimdCompFusedForHierarchicalCodecU16_128(size_t outer = 256, size_t inner = 8,
                                           FusedAggImpl agg = FusedAggImpl::kMadd)
      : outer_(outer), inner_(inner), agg_(agg) {
    assert(outer == 128 || outer == 256);
    assert(inner == 4 || inner == 8 || inner == 16 || inner == 32 ||
           inner == 64 || inner == 128 || inner == 256);
    assert(inner <= outer && outer % inner == 0);
  }

  void EncodeArray(const uint16_t* in, const size_t length) override {
    using namespace simdcomp_for_w128_detail;
    const size_t W = (outer_ >= length) ? length : outer_;
    const size_t w = (inner_ >= length) ? length : inner_;
    assert(length % kBlk == 0);
    assert(length % W == 0 && length % w == 0 && W % w == 0);
    const size_t num_blk = length / kBlk;
    const size_t num_outer = length / W;
    const size_t num_inner = length / w;
    const size_t ratio = W / w;  // inner windows per outer window

    // 1. Global anchors (per-outer min) and local anchors (per-inner min) →
    //    deltas. Local anchors kept in s_anchor for the residual pass.
    uint16_t* gmin = s_delta;            // borrow s_delta for globals (num_outer)
    uint16_t* lanchor = s_anchor;        // local anchors (num_inner)
    for (size_t g = 0; g < num_outer; ++g) {
      const uint16_t* gp = in + g * W;
      uint16_t gm = gp[0];
      for (size_t i = 1; i < W; ++i)
        if (gp[i] < gm) gm = gp[i];
      gmin[g] = gm;
    }
    uint16_t b_local_max = 0;
    for (size_t l = 0; l < num_inner; ++l) {
      const uint16_t* lp = in + l * w;
      uint16_t lm = lp[0];
      for (size_t i = 1; i < w; ++i)
        if (lp[i] < lm) lm = lp[i];
      lanchor[l] = lm;
      const uint16_t d = (uint16_t)(lm - gmin[l / ratio]);
      if (d > b_local_max) b_local_max = d;
    }
    const uint32_t b_local = bits_u16(b_local_max);

    // 2. Header: num_blk, madd_safe, raw globals, b_local, packed deltas.
    uint16_t orall = 0;
    for (size_t i = 0; i < length; ++i) orall |= in[i];
    auto& scratch = GetPackScratch();
    uint8_t* basep = scratch.data();
    *reinterpret_cast<uint32_t*>(basep) = (uint32_t)num_blk;
    basep[sizeof(uint32_t)] = (orall < 0x8000u) ? 1 : 0;
    uint8_t* cur = basep + sizeof(uint32_t) + 1;
    std::memcpy(cur, gmin, num_outer * sizeof(uint16_t));
    cur += num_outer * sizeof(uint16_t);
    *cur++ = (uint8_t)b_local;

    // Pack deltas (single b_local) per 128-block into s_delta-derived temp.
    // Build the contiguous delta array first (reuse s_res as scratch).
    uint16_t* deltas = s_res;  // num_inner values (residual pass not started yet)
    for (size_t l = 0; l < num_inner; ++l)
      deltas[l] = (uint16_t)(lanchor[l] - gmin[l / ratio]);
    const size_t nd_blk = (num_inner + kBlk - 1) / kBlk;
    for (size_t i = num_inner; i < nd_blk * kBlk; ++i) deltas[i] = 0;  // pad
    for (size_t db = 0; db < nd_blk; ++db) {
      simdpack_u16_w128(deltas + db * kBlk, reinterpret_cast<__m128i*>(cur),
                        b_local);
      cur += (size_t)b_local * sizeof(__m128i);
    }

    // 3. Residuals per 128-block (value − local_anchor) + chunked b.
    uint8_t* bs = cur;
    uint8_t* out_ptr = bs + num_blk;
    for (size_t k = 0; k < num_blk; ++k) {
      __m128i corr[16];
      build_corrections(corr, lanchor, k, w);
      uint16_t* dst = s_res + k * kBlk;  // safe: deltas already consumed/packed
      __m128i max_acc = _mm_setzero_si128();
      for (size_t j = 0; j < kOutRegs; ++j) {
        __m128i v = _mm_loadu_si128(
            reinterpret_cast<const __m128i*>(in + k * kBlk + j * kLanes));
        __m128i r = _mm_sub_epi16(v, corr[j]);
        _mm_storeu_si128(reinterpret_cast<__m128i*>(dst + j * kLanes), r);
        max_acc = _mm_max_epu16(max_acc, r);
      }
      const uint32_t b_k = bits_u16(hmax8(max_acc));
      bs[k] = (uint8_t)b_k;
      simdpack_u16_w128(dst, reinterpret_cast<__m128i*>(out_ptr), b_k);
      out_ptr += (size_t)b_k * sizeof(__m128i);
    }

    const size_t actual = (size_t)(out_ptr - scratch.data());
    assert(actual <= scratch.size());
    compressed.assign(scratch.data(), scratch.data() + actual);
  }

  void DecodeArray(uint16_t* out, const std::size_t length) override {
    using namespace simdcomp_for_w128_detail;
    const size_t W = (outer_ >= length) ? length : outer_;
    const size_t w = (inner_ >= length) ? length : inner_;
    const size_t num_blk = length / kBlk;
    const size_t num_outer = length / W;
    const size_t num_inner = length / w;
    const size_t ratio = W / w;

    const uint8_t* p = compressed.data();
    const uint32_t got = *reinterpret_cast<const uint32_t*>(p);
    (void)got;
    assert(got == num_blk);
    p += sizeof(uint32_t);
    const bool madd_safe = *p++ != 0;
    const bool madd = (agg_ == FusedAggImpl::kMadd) && madd_safe;

    const uint16_t* gmin = reinterpret_cast<const uint16_t*>(p);
    p += num_outer * sizeof(uint16_t);
    const uint32_t b_local = *p++;

    // FUSED decode. Delta blocks use a single b_local, so the bs/residual
    // region is at a fixed offset — locate it up front, then for each 128-delta
    // block: store-unpack the deltas, add the global anchor in place (forming
    // the local anchors L = global + delta, hot in L1), and immediately decode
    // the `w` residual blocks that block feeds. One full delta block (128 inner
    // windows) spans exactly `w` 128-element residual blocks; the last block may
    // be padded so both the gmin add (→ num_inner) and k1 (→ num_blk) clamp.
    const size_t nd_blk = (num_inner + kBlk - 1) / kBlk;
    const uint8_t* dpay = p;
    const uint8_t* bs = dpay + nd_blk * (size_t)b_local * sizeof(__m128i);
    const __m128i* in_ptr = reinterpret_cast<const __m128i*>(bs + num_blk);

    // ratio = W/w is a power of two → gmin index is l >> shr (no division).
    const unsigned shr = (unsigned)__builtin_ctzll((unsigned long long)ratio);
    const unsigned sh = (unsigned)__builtin_ctzll((unsigned long long)w);
    const int mode = decode_mode(w);
    __m128i sum = _mm_setzero_si128();
    const uint8_t* dcur = dpay;
    for (size_t ab = 0; ab < nd_blk; ++ab) {
      uint16_t* a = s_anchor + ab * kBlk;
      simdunpack_u16_w128_store(reinterpret_cast<const __m128i*>(dcur), a,
                                b_local);
      dcur += (size_t)b_local * sizeof(__m128i);
      const size_t l0 = ab * kBlk;
      const size_t lend = (l0 + kBlk < num_inner) ? (l0 + kBlk) : num_inner;
      for (size_t l = l0; l < lend; ++l)
        a[l - l0] = (uint16_t)(a[l - l0] + gmin[l >> shr]);

      const size_t k0 = ab * w;
      const size_t k1 = ((ab + 1) * w < num_blk) ? (ab + 1) * w : num_blk;
      for (size_t k = k0; k < k1; ++k)
        decode_block(in_ptr, out + k * kBlk, s_anchor, k, w, sh, mode, madd,
                     bs[k], &sum, false);
    }

    const uint32_t total = hsum4(sum);
    out[length] = (uint16_t)(total & 0xFFFF);
    out[length + 1] = (uint16_t)(total >> 16);
  }

  std::size_t EncodedNumValues() override { return compressed.size(); }
  std::size_t EncodedSizeValue() override { return sizeof(uint8_t); }
  virtual ~SimdCompFusedForHierarchicalCodecU16_128() {}
  std::string name() const override {
    std::string n = "simdcomp_fused_for_hier_128_g" + std::to_string(outer_) +
                    "_l" + std::to_string(inner_);
    n += (agg_ == FusedAggImpl::kMadd) ? "_madd" : "_unpack";
    return n;
  }
  std::size_t GetOverflowSize(size_t) const override { return 2; }
  StatefulIntegerCodec<uint16_t>* CloneFresh() const override {
    return new SimdCompFusedForHierarchicalCodecU16_128(outer_, inner_, agg_);
  }
  void AllocEncoded(const uint16_t*, size_t) override {}
  void clear() override {
    compressed.clear();
    compressed.shrink_to_fit();
  }
  std::vector<uint16_t>& GetEncoded() override {
    throw std::runtime_error("FoR encoded format does not match input.");
  }
};
