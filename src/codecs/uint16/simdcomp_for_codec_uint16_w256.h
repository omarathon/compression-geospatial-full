#pragma once

// ── Frame-of-Reference (FoR) fused-sum codecs, 256-bit (AVX2) simdcomp ──────
//
// 256-bit counterparts of the 128-bit FoR codecs in
// simdcomp_for_codec_uint16_w128.h. Residuals are bit-packed in fixed
// 256-element blocks with a per-block bit width `b` (chunked-b), exactly like
// the base SimdCompFusedCodecU16. The FoR *window* only controls anchor
// granularity; it is independent of the 256-element bit-packing block.
//
// Geometry: a 256-element block = 16 OutRegs × 16 uint16 lanes.
//           element i ↔ OutReg i/16, lane i%16.
//
// Decode is fused-sum: per OutReg we add a broadcast (the window anchor) on the
// OutReg dependency chain — the honest per-OutReg add_anchor — then aggregate.
// No decoded values are written (sum-only), like every other fused codec here.
//
// This file is a near-mechanical width-port of the 128-bit version. The only
// STRUCTURAL difference (everything else is constants / instruction widths):
//   * The 128 had two sub-block anchor modes besides scalar/uniform: a "half"
//     mode for w==4 (each 8-lane OutReg straddles two 4-elem windows). At
//     256-width an OutReg is 16 lanes, so the split shifts UP a level:
//       - w==8  → "half"    (two 8-elem windows: lanes 0-7 / 8-15)
//       - w==4  → "quarter"  (four 4-elem windows: lanes 0-3/4-7/8-11/12-15) —
//         a NEW kernel mode (simdunpack_u16_w256_corrected_quarter) with no
//         128-bit analogue. See gen_simdbitpacking_u16.py `corrected_quarter`.
//
// The aggregate implementation (unpack-widen vs madd) is selectable per codec
// via FusedAggImpl; kMadd falls back to kUnpack when the data isn't madd-safe.
//
// REQUIREMENT: length % 256 == 0, length % window == 0, and (for the packed
// anchor / delta streams) the stream length must be a multiple of 256 — all
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

// 256-bit simdcomp primitives (own static lib SimdCompU16W256).
extern "C" void simdpack_u16_w256(const uint16_t* in, __m256i* out,
                                  const uint32_t bit);
extern "C" void simdunpack_u16_w256_store(const __m256i* in, uint16_t* out,
                                          const uint32_t bit);
extern "C" void simdunpack_u16_w256_corrected_uniform(const __m256i* in,
                                                      uint16_t* out,
                                                      const uint32_t bit,
                                                      const __m256i anchor,
                                                      __m256i* sum);
// Per-OutReg anchor broadcast straight from the raw uint16 anchor stream, one
// variant per shg = log2(w/16) (w = 16,32,64,128). shg is compile-time so
// repeated broadcasts (w>16) are CSE'd: cscalar3 (w128) emits just 2/block.
extern "C" void simdunpack_u16_w256_cscalar0(const __m256i*, uint16_t*, uint32_t,
                                             const uint16_t* a_block, __m256i*);
extern "C" void simdunpack_u16_w256_cscalar1(const __m256i*, uint16_t*, uint32_t,
                                             const uint16_t* a_block, __m256i*);
extern "C" void simdunpack_u16_w256_cscalar2(const __m256i*, uint16_t*, uint32_t,
                                             const uint16_t* a_block, __m256i*);
extern "C" void simdunpack_u16_w256_cscalar3(const __m256i*, uint16_t*, uint32_t,
                                             const uint16_t* a_block, __m256i*);
// w==8: each OutReg's correction is half a_block[2v] (lanes 0-7), half
// a_block[2v+1] (lanes 8-15), built inline (two broadcasts + blend) — no array.
extern "C" void simdunpack_u16_w256_corrected_half(const __m256i*, uint16_t*,
                                                   uint32_t,
                                                   const uint16_t* a_block,
                                                   __m256i*);
// shuf variant: vbroadcasti128+vpshufb instead of 2×vpbroadcastw+vpblendd
// (3→2 port-5 ops per OutReg).
extern "C" void simdunpack_u16_w256_corrected_half_shuf(const __m256i*, uint16_t*,
                                                        uint32_t,
                                                        const uint16_t* a_block,
                                                        __m256i*);
// w==4: each OutReg covers four 4-lane windows a_block[4v..4v+3], built inline
// (two SSE blends + set_m128i) — no corrections array. NEW at 256-width.
extern "C" void simdunpack_u16_w256_corrected_quarter(const __m256i*, uint16_t*,
                                                      uint32_t,
                                                      const uint16_t* a_block,
                                                      __m256i*);
// shuf variant: vbroadcasti128+vpshufb instead of 7 port-5 ops per OutReg.
extern "C" void simdunpack_u16_w256_corrected_quarter_shuf(const __m256i*, uint16_t*,
                                                           uint32_t,
                                                           const uint16_t* a_block,
                                                           __m256i*);
// shuf variants for cscalar (w=16..128): uses vbroadcasti128+vpshufb instead
// of a single vpbroadcastw — adds 1 extra port-5 op; expected worse, but bench.
extern "C" void simdunpack_u16_w256_cscalar_shuf0(const __m256i*, uint16_t*, uint32_t,
                                                  const uint16_t*, __m256i*);
extern "C" void simdunpack_u16_w256_cscalar_shuf1(const __m256i*, uint16_t*, uint32_t,
                                                  const uint16_t*, __m256i*);
extern "C" void simdunpack_u16_w256_cscalar_shuf2(const __m256i*, uint16_t*, uint32_t,
                                                  const uint16_t*, __m256i*);
extern "C" void simdunpack_u16_w256_cscalar_shuf3(const __m256i*, uint16_t*, uint32_t,
                                                  const uint16_t*, __m256i*);
// madd-widen aggregate variants (~1.5× decode): valid only when all decoded
// values < 2^15. The codec stores a 1-byte flag and dispatches on it.
extern "C" void simdunpack_u16_w256_corrected_uniform_madd(const __m256i*, uint16_t*,
                                                           uint32_t, const __m256i, __m256i*);
extern "C" void simdunpack_u16_w256_cscalar0_madd(const __m256i*, uint16_t*, uint32_t,
                                                  const uint16_t*, __m256i*);
extern "C" void simdunpack_u16_w256_cscalar1_madd(const __m256i*, uint16_t*, uint32_t,
                                                  const uint16_t*, __m256i*);
extern "C" void simdunpack_u16_w256_cscalar2_madd(const __m256i*, uint16_t*, uint32_t,
                                                  const uint16_t*, __m256i*);
extern "C" void simdunpack_u16_w256_cscalar3_madd(const __m256i*, uint16_t*, uint32_t,
                                                  const uint16_t*, __m256i*);
extern "C" void simdunpack_u16_w256_cscalar_shuf0_madd(const __m256i*, uint16_t*, uint32_t,
                                                       const uint16_t*, __m256i*);
extern "C" void simdunpack_u16_w256_cscalar_shuf1_madd(const __m256i*, uint16_t*, uint32_t,
                                                       const uint16_t*, __m256i*);
extern "C" void simdunpack_u16_w256_cscalar_shuf2_madd(const __m256i*, uint16_t*, uint32_t,
                                                       const uint16_t*, __m256i*);
extern "C" void simdunpack_u16_w256_cscalar_shuf3_madd(const __m256i*, uint16_t*, uint32_t,
                                                       const uint16_t*, __m256i*);
extern "C" void simdunpack_u16_w256_corrected_half_madd(const __m256i*, uint16_t*,
                                                        uint32_t, const uint16_t*, __m256i*);
extern "C" void simdunpack_u16_w256_corrected_half_shuf_madd(const __m256i*, uint16_t*,
                                                             uint32_t, const uint16_t*, __m256i*);
extern "C" void simdunpack_u16_w256_corrected_quarter_madd(const __m256i*, uint16_t*,
                                                           uint32_t, const uint16_t*, __m256i*);
extern "C" void simdunpack_u16_w256_corrected_quarter_shuf_madd(const __m256i*, uint16_t*,
                                                                uint32_t, const uint16_t*, __m256i*);
#ifdef FOR_DECODE_NOAGG  // benchmark-only: produce OutReg, skip the widening sum
extern "C" void simdunpack_u16_w256_corrected_uniform_noagg(const __m256i*, uint16_t*,
                                                            uint32_t, const __m256i, __m256i*);
extern "C" void simdunpack_u16_w256_cscalar0_noagg(const __m256i*, uint16_t*, uint32_t, const uint16_t*, __m256i*);
extern "C" void simdunpack_u16_w256_cscalar1_noagg(const __m256i*, uint16_t*, uint32_t, const uint16_t*, __m256i*);
extern "C" void simdunpack_u16_w256_cscalar2_noagg(const __m256i*, uint16_t*, uint32_t, const uint16_t*, __m256i*);
extern "C" void simdunpack_u16_w256_cscalar3_noagg(const __m256i*, uint16_t*, uint32_t, const uint16_t*, __m256i*);
extern "C" void simdunpack_u16_w256_cscalar_shuf0_noagg(const __m256i*, uint16_t*, uint32_t, const uint16_t*, __m256i*);
extern "C" void simdunpack_u16_w256_cscalar_shuf1_noagg(const __m256i*, uint16_t*, uint32_t, const uint16_t*, __m256i*);
extern "C" void simdunpack_u16_w256_cscalar_shuf2_noagg(const __m256i*, uint16_t*, uint32_t, const uint16_t*, __m256i*);
extern "C" void simdunpack_u16_w256_cscalar_shuf3_noagg(const __m256i*, uint16_t*, uint32_t, const uint16_t*, __m256i*);
extern "C" void simdunpack_u16_w256_corrected_half_noagg(const __m256i*, uint16_t*, uint32_t, const uint16_t*, __m256i*);
extern "C" void simdunpack_u16_w256_corrected_half_shuf_noagg(const __m256i*, uint16_t*, uint32_t, const uint16_t*, __m256i*);
extern "C" void simdunpack_u16_w256_corrected_quarter_noagg(const __m256i*, uint16_t*, uint32_t, const uint16_t*, __m256i*);
extern "C" void simdunpack_u16_w256_corrected_quarter_shuf_noagg(const __m256i*, uint16_t*, uint32_t, const uint16_t*, __m256i*);
#endif

namespace simdcomp_for_w256_detail {

static constexpr size_t kBlk = 256;       // residual bit-pack block
static constexpr size_t kOutRegs = 16;    // 256 / 16 lanes
static constexpr size_t kLanes = 16;      // uint16 lanes per __m256i

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

// horizontal max of 16 uint16 lanes.
static inline uint16_t hmax16(__m256i m) {
  __m128i lo = _mm256_castsi256_si128(m);
  __m128i hi = _mm256_extracti128_si256(m, 1);
  __m128i x = _mm_max_epu16(lo, hi);
  x = _mm_max_epu16(x, _mm_srli_si128(x, 8));
  x = _mm_max_epu16(x, _mm_srli_si128(x, 4));
  x = _mm_max_epu16(x, _mm_srli_si128(x, 2));
  return (uint16_t)_mm_extract_epi16(x, 0);
}

// horizontal sum of 8 int32 lanes.
static inline uint32_t hsum8(__m256i sum) {
  __m128i lo = _mm256_castsi256_si128(sum);
  __m128i hi = _mm256_extracti128_si256(sum, 1);
  __m128i s = _mm_add_epi32(lo, hi);
  s = _mm_add_epi32(s, _mm_shuffle_epi32(s, _MM_SHUFFLE(1, 0, 3, 2)));
  s = _mm_add_epi32(s, _mm_shuffle_epi32(s, _MM_SHUFFLE(2, 3, 0, 1)));
  return (uint32_t)_mm_cvtsi128_si32(s);
}

// Build the 16 per-OutReg correction broadcasts for 256-block `k`.
// `anchors` is the per-window anchor array (regular: window mins; hierarchical:
// materialised local anchors). OutReg j covers elements [256k+16j, +16).
//   w >= 16 : each OutReg lies entirely in one window → one broadcast.
//   w == 8  : each OutReg straddles two windows → lanes 0-7 = anchor of the low
//             window, lanes 8-15 = anchor of the high window.
//   w == 4  : each OutReg straddles four windows → 4 lanes each.
// All supported windows are powers of two, so the window index is `elem >> sh`
// (sh = log2 w) — never a runtime integer division on the hot path.
static inline void build_corrections(__m256i corr[16], const uint16_t* anchors,
                                     size_t k, size_t w) {
  const size_t base = k * kBlk;
  const unsigned sh = (unsigned)__builtin_ctzll((unsigned long long)w);
  if (w >= kLanes) {
    for (size_t j = 0; j < kOutRegs; ++j)
      corr[j] = _mm256_set1_epi16((short)anchors[(base + kLanes * j) >> sh]);
  } else if (w == 8) {  // two windows per OutReg (lanes 0-7 / 8-15)
    for (size_t j = 0; j < kOutRegs; ++j) {
      const size_t elem = base + kLanes * j;
      const short aLo = (short)anchors[elem >> sh];
      const short aHi = (short)anchors[(elem + 8) >> sh];
      corr[j] = _mm256_set_m128i(_mm_set1_epi16(aHi), _mm_set1_epi16(aLo));
    }
  } else {  // w == 4: four windows per OutReg (4 lanes each)
    for (size_t j = 0; j < kOutRegs; ++j) {
      const size_t elem = base + kLanes * j;
      const short a0 = (short)anchors[elem >> sh];
      const short a1 = (short)anchors[(elem + 4) >> sh];
      const short a2 = (short)anchors[(elem + 8) >> sh];
      const short a3 = (short)anchors[(elem + 12) >> sh];
      const __m128i lo = _mm_blend_epi16(_mm_set1_epi16(a0), _mm_set1_epi16(a1), 0xF0);
      const __m128i hi = _mm_blend_epi16(_mm_set1_epi16(a2), _mm_set1_epi16(a3), 0xF0);
      corr[j] = _mm256_set_m128i(hi, lo);
    }
  }
}

// Decode mode, chosen once per decode from the window `w`:
//   kModeUniform (w >= 256): one anchor per 256-block → single broadcast.
//   kModeScalar  (16 <= w < 256): per-OutReg anchor, broadcast straight from the
//       raw uint16 anchor stream (no __m256i corrections array to store/reload).
//   kModeHalf    (w == 8): two anchors per OutReg (lanes 0-7 / 8-15).
//   kModeQuarter (w == 4): four anchors per OutReg (4 lanes each).
enum { kModeQuarter = 0, kModeHalf = 1, kModeScalar = 2, kModeUniform = 3 };
static inline int decode_mode(size_t w) {
  if (w >= kBlk) return kModeUniform;
  if (w >= kLanes) return kModeScalar;
  return (w == 8) ? kModeHalf : kModeQuarter;
}

// Decode one 256-block. `sh` = log2(w). All FoR overhead beyond plain bitpack
// lives here (the per-OutReg add). `madd`: use the faster madd-widen aggregate
// (set by the codec when kMadd is requested and the data is madd-safe). `shuf`:
// use vpshufb correction for kModeHalf (w=8) and kModeQuarter (w=4) — saves
// 3→2 and 7→2 port-5 ops per OutReg respectively. `sh` and `mode` are
// loop-invariant per decode, so the branches are perfectly predicted.
static inline void decode_block(const __m256i*& in_ptr, uint16_t* out_k,
                                const uint16_t* anchors, size_t k, size_t w,
                                unsigned sh, int mode, bool madd, bool shuf,
                                uint32_t b_k, __m256i* sum) {
#ifdef FOR_DECODE_NOAGG
  (void)madd;  // benchmark-only: produce OutReg, XOR sink (sums are wrong)
  if (mode == kModeUniform) {
    const __m256i a = _mm256_set1_epi16((short)anchors[(k * kBlk) >> sh]);
    simdunpack_u16_w256_corrected_uniform_noagg(in_ptr, out_k, b_k, a, sum);
  } else if (mode == kModeScalar) {
    const uint16_t* a_block = anchors + ((k * kBlk) >> sh);
    if (shuf) {
      switch (sh) {
        case 4:  simdunpack_u16_w256_cscalar_shuf0_noagg(in_ptr, out_k, b_k, a_block, sum); break;
        case 5:  simdunpack_u16_w256_cscalar_shuf1_noagg(in_ptr, out_k, b_k, a_block, sum); break;
        case 6:  simdunpack_u16_w256_cscalar_shuf2_noagg(in_ptr, out_k, b_k, a_block, sum); break;
        default: simdunpack_u16_w256_cscalar_shuf3_noagg(in_ptr, out_k, b_k, a_block, sum); break;
      }
    } else {
      switch (sh) {
        case 4:  simdunpack_u16_w256_cscalar0_noagg(in_ptr, out_k, b_k, a_block, sum); break;
        case 5:  simdunpack_u16_w256_cscalar1_noagg(in_ptr, out_k, b_k, a_block, sum); break;
        case 6:  simdunpack_u16_w256_cscalar2_noagg(in_ptr, out_k, b_k, a_block, sum); break;
        default: simdunpack_u16_w256_cscalar3_noagg(in_ptr, out_k, b_k, a_block, sum); break;
      }
    }
  } else if (mode == kModeHalf) {
    const uint16_t* a_block = anchors + ((k * kBlk) >> sh);
    if (shuf) simdunpack_u16_w256_corrected_half_shuf_noagg(in_ptr, out_k, b_k, a_block, sum);
    else      simdunpack_u16_w256_corrected_half_noagg(in_ptr, out_k, b_k, a_block, sum);
  } else {
    const uint16_t* a_block = anchors + ((k * kBlk) >> sh);
    if (shuf) simdunpack_u16_w256_corrected_quarter_shuf_noagg(in_ptr, out_k, b_k, a_block, sum);
    else      simdunpack_u16_w256_corrected_quarter_noagg(in_ptr, out_k, b_k, a_block, sum);
  }
  in_ptr += b_k;
  return;
#endif
  if (mode == kModeUniform) {
    const __m256i a = _mm256_set1_epi16((short)anchors[(k * kBlk) >> sh]);
    if (madd) simdunpack_u16_w256_corrected_uniform_madd(in_ptr, out_k, b_k, a, sum);
    else      simdunpack_u16_w256_corrected_uniform(in_ptr, out_k, b_k, a, sum);
  } else if (mode == kModeScalar) {
    const uint16_t* a_block = anchors + ((k * kBlk) >> sh);
    if (shuf) {
      if (madd) {
        switch (sh) {
          case 4:  simdunpack_u16_w256_cscalar_shuf0_madd(in_ptr, out_k, b_k, a_block, sum); break;
          case 5:  simdunpack_u16_w256_cscalar_shuf1_madd(in_ptr, out_k, b_k, a_block, sum); break;
          case 6:  simdunpack_u16_w256_cscalar_shuf2_madd(in_ptr, out_k, b_k, a_block, sum); break;
          default: simdunpack_u16_w256_cscalar_shuf3_madd(in_ptr, out_k, b_k, a_block, sum); break;
        }
      } else {
        switch (sh) {
          case 4:  simdunpack_u16_w256_cscalar_shuf0(in_ptr, out_k, b_k, a_block, sum); break;
          case 5:  simdunpack_u16_w256_cscalar_shuf1(in_ptr, out_k, b_k, a_block, sum); break;
          case 6:  simdunpack_u16_w256_cscalar_shuf2(in_ptr, out_k, b_k, a_block, sum); break;
          default: simdunpack_u16_w256_cscalar_shuf3(in_ptr, out_k, b_k, a_block, sum); break;
        }
      }
    } else if (madd) {
      switch (sh) {
        case 4:  simdunpack_u16_w256_cscalar0_madd(in_ptr, out_k, b_k, a_block, sum); break;
        case 5:  simdunpack_u16_w256_cscalar1_madd(in_ptr, out_k, b_k, a_block, sum); break;
        case 6:  simdunpack_u16_w256_cscalar2_madd(in_ptr, out_k, b_k, a_block, sum); break;
        default: simdunpack_u16_w256_cscalar3_madd(in_ptr, out_k, b_k, a_block, sum); break;
      }
    } else {
      switch (sh) {
        case 4:  simdunpack_u16_w256_cscalar0(in_ptr, out_k, b_k, a_block, sum); break;
        case 5:  simdunpack_u16_w256_cscalar1(in_ptr, out_k, b_k, a_block, sum); break;
        case 6:  simdunpack_u16_w256_cscalar2(in_ptr, out_k, b_k, a_block, sum); break;
        default: simdunpack_u16_w256_cscalar3(in_ptr, out_k, b_k, a_block, sum); break;
      }
    }
  } else if (mode == kModeHalf) {  // w == 8 — two windows per OutReg, inline
    const uint16_t* a_block = anchors + ((k * kBlk) >> sh);
    if (shuf) {
      if (madd) simdunpack_u16_w256_corrected_half_shuf_madd(in_ptr, out_k, b_k, a_block, sum);
      else      simdunpack_u16_w256_corrected_half_shuf(in_ptr, out_k, b_k, a_block, sum);
    } else {
      if (madd) simdunpack_u16_w256_corrected_half_madd(in_ptr, out_k, b_k, a_block, sum);
      else      simdunpack_u16_w256_corrected_half(in_ptr, out_k, b_k, a_block, sum);
    }
  } else {  // kModeQuarter: w == 4 — four windows per OutReg, inline
    const uint16_t* a_block = anchors + ((k * kBlk) >> sh);
    if (shuf) {
      if (madd) simdunpack_u16_w256_corrected_quarter_shuf_madd(in_ptr, out_k, b_k, a_block, sum);
      else      simdunpack_u16_w256_corrected_quarter_shuf(in_ptr, out_k, b_k, a_block, sum);
    } else {
      if (madd) simdunpack_u16_w256_corrected_quarter_madd(in_ptr, out_k, b_k, a_block, sum);
      else      simdunpack_u16_w256_corrected_quarter(in_ptr, out_k, b_k, a_block, sum);
    }
  }
  in_ptr += b_k;
}

}  // namespace simdcomp_for_w256_detail

// ── Regular FoR ──────────────────────────────────────────────────────────────
//
// Layout (separate=true):
//   [num_blk : uint32][madd_safe : uint8][anchors : uint16 × num_anchors]
//   [bs : uint8 × num_blk][payload_0]…[payload_{num_blk-1}]
// Layout (separate=false):
//   [num_blk : uint32][madd_safe : uint8]
//   [anchor_bs : uint8 × (num_anchors/256)][anchor payloads]
//   [bs : uint8 × num_blk][payload_0]…
class SimdCompFusedForCodecU16_256 : public StatefulIntegerCodec<uint16_t> {
 public:
  std::vector<uint8_t> compressed;
  size_t window_;
  bool separate_;
  FusedAggImpl agg_;
  bool shuf_;  // use vpshufb correction for kModeHalf (w=8) and kModeQuarter (w=4)

  explicit SimdCompFusedForCodecU16_256(size_t window = 16, bool separate = false,
                                        FusedAggImpl agg = FusedAggImpl::kMadd,
                                        bool shuf = false)
      : window_(window), separate_(separate), agg_(agg), shuf_(shuf) {
    assert(window == 4 || window == 8 || window == 16 || window == 32 ||
           window == 64 || window == 128 || window == 256);
  }

  void EncodeArray(const uint16_t* in, const size_t length) override {
    using namespace simdcomp_for_w256_detail;
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
      // Pack the anchor stream with the same simdpack_u16_w256 used for
      // residuals (chunked-b). Pad to a 256-multiple with zeros (never read
      // back) so any length works.
      const size_t na_blk = (num_anchors + kBlk - 1) / kBlk;
      for (size_t i = num_anchors; i < na_blk * kBlk; ++i) anchors[i] = 0;
      uint8_t* abs = cur;
      uint8_t* apay = abs + na_blk;
      for (size_t ab = 0; ab < na_blk; ++ab) {
        const uint16_t* ablk = anchors + ab * kBlk;
        const uint32_t ab_b = maxbits_n(ablk, kBlk);
        abs[ab] = (uint8_t)ab_b;
        simdpack_u16_w256(ablk, reinterpret_cast<__m256i*>(apay), ab_b);
        apay += (size_t)ab_b * sizeof(__m256i);
      }
      cur = apay;
    }

    // 3. Residuals per 256-block + chunked b; bit-pack into scratch.
    uint8_t* bs = cur;
    uint8_t* out_ptr = bs + num_blk;
    for (size_t k = 0; k < num_blk; ++k) {
      __m256i corr[16];
      build_corrections(corr, anchors, k, w);
      uint16_t* dst = s_res + k * kBlk;
      __m256i max_acc = _mm256_setzero_si256();
      for (size_t j = 0; j < kOutRegs; ++j) {
        __m256i v = _mm256_loadu_si256(
            reinterpret_cast<const __m256i*>(in + k * kBlk + j * kLanes));
        __m256i r = _mm256_sub_epi16(v, corr[j]);
        _mm256_storeu_si256(reinterpret_cast<__m256i*>(dst + j * kLanes), r);
        max_acc = _mm256_max_epu16(max_acc, r);
      }
      const uint32_t b_k = bits_u16(hmax16(max_acc));
      bs[k] = (uint8_t)b_k;
      simdpack_u16_w256(dst, reinterpret_cast<__m256i*>(out_ptr), b_k);
      out_ptr += (size_t)b_k * sizeof(__m256i);
    }

    const size_t actual = (size_t)(out_ptr - scratch.data());
    assert(actual <= scratch.size());
    compressed.assign(scratch.data(), scratch.data() + actual);
  }

  void DecodeArray(uint16_t* out, const std::size_t length) override {
    using namespace simdcomp_for_w256_detail;
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
    const bool shuf = shuf_;  // applies to kModeScalar, kModeHalf, kModeQuarter
    __m256i sum = _mm256_setzero_si256();

    if (separate_) {
      // Anchors raw, in place — no materialisation pass.
      const uint16_t* anchors = reinterpret_cast<const uint16_t*>(p);
      p += num_anchors * sizeof(uint16_t);
      const uint8_t* bs = p;
      const __m256i* in_ptr = reinterpret_cast<const __m256i*>(bs + num_blk);
      for (size_t k = 0; k < num_blk; ++k)
        decode_block(in_ptr, out + k * kBlk, anchors, k, w, sh, mode, madd, shuf,
                     bs[k], &sum);
    } else {
      // Anchors SIMD-packed (chunked-b). FUSED: unpack each 256-anchor block,
      // then immediately decode the `w` residual blocks it feeds — the anchors
      // stay hot in L1 instead of being swept twice through a full s_anchor.
      // One full anchor block (256 windows) spans exactly `w` 256-element
      // residual blocks; the last block may be padded so k1 clamps to num_blk.
      const size_t na_blk = (num_anchors + kBlk - 1) / kBlk;
      const uint8_t* abs = p;
      const uint8_t* apay = abs + na_blk;
      size_t apay_bytes = 0;
      for (size_t ab = 0; ab < na_blk; ++ab)
        apay_bytes += (size_t)abs[ab] * sizeof(__m256i);
      const uint8_t* bs = apay + apay_bytes;
      const __m256i* in_ptr = reinterpret_cast<const __m256i*>(bs + num_blk);

      const uint8_t* acur = apay;
      for (size_t ab = 0; ab < na_blk; ++ab) {
        simdunpack_u16_w256_store(reinterpret_cast<const __m256i*>(acur),
                                  s_anchor + ab * kBlk, abs[ab]);
        acur += (size_t)abs[ab] * sizeof(__m256i);
        const size_t k0 = ab * w;
        const size_t k1 = ((ab + 1) * w < num_blk) ? (ab + 1) * w : num_blk;
        for (size_t k = k0; k < k1; ++k)
          decode_block(in_ptr, out + k * kBlk, s_anchor, k, w, sh, mode, madd, shuf,
                       bs[k], &sum);
      }
    }

    const uint32_t total = hsum8(sum);
    out[length] = (uint16_t)(total & 0xFFFF);
    out[length + 1] = (uint16_t)(total >> 16);
  }

  std::size_t EncodedNumValues() override { return compressed.size(); }
  std::size_t EncodedSizeValue() override { return sizeof(uint8_t); }
  virtual ~SimdCompFusedForCodecU16_256() {}
  std::string name() const override {
    std::string n = "simdcomp_fused_for_256_w" + std::to_string(window_);
    if (separate_) n += "_sep";
    if (shuf_) n += "_shuf";
    n += (agg_ == FusedAggImpl::kMadd) ? "_madd" : "_unpack";
    return n;
  }
  std::size_t GetOverflowSize(size_t) const override { return 2; }
  StatefulIntegerCodec<uint16_t>* CloneFresh() const override {
    return new SimdCompFusedForCodecU16_256(window_, separate_, agg_, shuf_);
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
//   [num_blk : uint32][madd_safe : uint8]
//   [global anchors : uint16 × num_outer]     // raw (cheap)
//   [b_local : uint8]                          // single bit width for all deltas
//   [delta payload : (num_inner/256) blocks @ b_local]   // value = local−global
//   [bs : uint8 × num_blk][payload_0]…
class SimdCompFusedForHierarchicalCodecU16_256
    : public StatefulIntegerCodec<uint16_t> {
 public:
  std::vector<uint8_t> compressed;
  size_t outer_;
  size_t inner_;
  FusedAggImpl agg_;

  SimdCompFusedForHierarchicalCodecU16_256(size_t outer = 256, size_t inner = 16,
                                           FusedAggImpl agg = FusedAggImpl::kMadd)
      : outer_(outer), inner_(inner), agg_(agg) {
    assert(outer == 128 || outer == 256);
    assert(inner == 4 || inner == 8 || inner == 16 || inner == 32 ||
           inner == 64 || inner == 128 || inner == 256);
    assert(inner <= outer && outer % inner == 0);
  }

  void EncodeArray(const uint16_t* in, const size_t length) override {
    using namespace simdcomp_for_w256_detail;
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

    // Pack deltas (single b_local) per 256-block into a contiguous temp first
    // (reuse s_res as scratch — residual pass not started yet).
    uint16_t* deltas = s_res;  // num_inner values
    for (size_t l = 0; l < num_inner; ++l)
      deltas[l] = (uint16_t)(lanchor[l] - gmin[l / ratio]);
    const size_t nd_blk = (num_inner + kBlk - 1) / kBlk;
    for (size_t i = num_inner; i < nd_blk * kBlk; ++i) deltas[i] = 0;  // pad
    for (size_t db = 0; db < nd_blk; ++db) {
      simdpack_u16_w256(deltas + db * kBlk, reinterpret_cast<__m256i*>(cur),
                        b_local);
      cur += (size_t)b_local * sizeof(__m256i);
    }

    // 3. Residuals per 256-block (value − local_anchor) + chunked b.
    uint8_t* bs = cur;
    uint8_t* out_ptr = bs + num_blk;
    for (size_t k = 0; k < num_blk; ++k) {
      __m256i corr[16];
      build_corrections(corr, lanchor, k, w);
      uint16_t* dst = s_res + k * kBlk;  // safe: deltas already consumed/packed
      __m256i max_acc = _mm256_setzero_si256();
      for (size_t j = 0; j < kOutRegs; ++j) {
        __m256i v = _mm256_loadu_si256(
            reinterpret_cast<const __m256i*>(in + k * kBlk + j * kLanes));
        __m256i r = _mm256_sub_epi16(v, corr[j]);
        _mm256_storeu_si256(reinterpret_cast<__m256i*>(dst + j * kLanes), r);
        max_acc = _mm256_max_epu16(max_acc, r);
      }
      const uint32_t b_k = bits_u16(hmax16(max_acc));
      bs[k] = (uint8_t)b_k;
      simdpack_u16_w256(dst, reinterpret_cast<__m256i*>(out_ptr), b_k);
      out_ptr += (size_t)b_k * sizeof(__m256i);
    }

    const size_t actual = (size_t)(out_ptr - scratch.data());
    assert(actual <= scratch.size());
    compressed.assign(scratch.data(), scratch.data() + actual);
  }

  void DecodeArray(uint16_t* out, const std::size_t length) override {
    using namespace simdcomp_for_w256_detail;
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
    // region is at a fixed offset — locate it up front, then for each 256-delta
    // block: store-unpack the deltas, add the global anchor in place (forming
    // the local anchors L = global + delta, hot in L1), and immediately decode
    // the `w` residual blocks that block feeds. One full delta block (256 inner
    // windows) spans exactly `w` 256-element residual blocks; the last block may
    // be padded so both the gmin add (→ num_inner) and k1 (→ num_blk) clamp.
    const size_t nd_blk = (num_inner + kBlk - 1) / kBlk;
    const uint8_t* dpay = p;
    const uint8_t* bs = dpay + nd_blk * (size_t)b_local * sizeof(__m256i);
    const __m256i* in_ptr = reinterpret_cast<const __m256i*>(bs + num_blk);

    // ratio = W/w is a power of two → gmin index is l >> shr (no division).
    const unsigned shr = (unsigned)__builtin_ctzll((unsigned long long)ratio);
    const unsigned sh = (unsigned)__builtin_ctzll((unsigned long long)w);
    const int mode = decode_mode(w);
    __m256i sum = _mm256_setzero_si256();
    const uint8_t* dcur = dpay;
    for (size_t ab = 0; ab < nd_blk; ++ab) {
      uint16_t* a = s_anchor + ab * kBlk;
      simdunpack_u16_w256_store(reinterpret_cast<const __m256i*>(dcur), a,
                                b_local);
      dcur += (size_t)b_local * sizeof(__m256i);
      const size_t l0 = ab * kBlk;
      const size_t lend = (l0 + kBlk < num_inner) ? (l0 + kBlk) : num_inner;
      for (size_t l = l0; l < lend; ++l)
        a[l - l0] = (uint16_t)(a[l - l0] + gmin[l >> shr]);

      const size_t k0 = ab * w;
      const size_t k1 = ((ab + 1) * w < num_blk) ? (ab + 1) * w : num_blk;
      for (size_t k = k0; k < k1; ++k)
        decode_block(in_ptr, out + k * kBlk, s_anchor, k, w, sh, mode, madd,
                     /*shuf=*/false, bs[k], &sum);
    }

    const uint32_t total = hsum8(sum);
    out[length] = (uint16_t)(total & 0xFFFF);
    out[length + 1] = (uint16_t)(total >> 16);
  }

  std::size_t EncodedNumValues() override { return compressed.size(); }
  std::size_t EncodedSizeValue() override { return sizeof(uint8_t); }
  virtual ~SimdCompFusedForHierarchicalCodecU16_256() {}
  std::string name() const override {
    std::string n = "simdcomp_fused_for_hier_256_g" + std::to_string(outer_) +
                    "_l" + std::to_string(inner_);
    n += (agg_ == FusedAggImpl::kMadd) ? "_madd" : "_unpack";
    return n;
  }
  std::size_t GetOverflowSize(size_t) const override { return 2; }
  StatefulIntegerCodec<uint16_t>* CloneFresh() const override {
    return new SimdCompFusedForHierarchicalCodecU16_256(outer_, inner_, agg_);
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
