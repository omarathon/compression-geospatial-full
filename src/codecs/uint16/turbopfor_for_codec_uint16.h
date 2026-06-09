#pragma once

// ── Frame-of-Reference (FoR) fused-sum TurboPFor codecs, 256-bit (AVX2) ──────
//
// TurboPFor counterpart of the simdcomp FoR-fused codecs
// (simdcomp_for_codec_uint16_w256.h). Same FoR framing — per-window anchor (=
// min), subtract, decode adds the anchor back on the OutReg dep chain before the
// aggregate — but the residual payload is a 256-element PFor stream (exceptions),
// not a plain chunked-b bit-pack. CR therefore matches
// Composite(FORCodecU16 / FORHierarchicalCodecU16, TurboPFor-physical) instead of
// the simdcomp physical.
//
// The anchor stream (raw for separate=true, chunked-b-packed for false,
// global+packed-deltas for hierarchical) is IDENTICAL to the simdcomp FoR codec
// — we reuse simdcomp_for_w256_detail for it. Only the residual stream differs:
//   encode: p4nenc256v16_for(residuals)   (PFor on residuals; CONST disabled)
//   decode: p4ndec256v16_for_sum(payload, …, anchors, w, sh, mode, madd)
//
// KEY DIFFERENCE from the simdcomp FoR-256 codec: simdcomp FUSES anchor-unpack
// into the residual-decode loop (anchors stay hot, never fully materialized).
// PFor blocks are variable-length and self-delimiting, so here the wrapper
// materializes the FULL per-window anchor array first (two-pass: raw / unpack /
// global+delta), then the C driver walks the PFor blocks adding anchors. For the
// coarse windows that win on latency the anchor array is tiny (≤512 B at w=256),
// so the extra pass is negligible; fine windows pay a small anchor round-trip.
//
// Both aggregate impls (FusedAggImpl::kUnpack / kMadd) are supported, with the
// same madd-safe gate as the simdcomp codecs (kMadd needs all values < 2^15).

#include <cassert>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <immintrin.h>
#include <stdexcept>
#include <string>
#include <vector>

#include "generic_codecs.h"
#include "delta_scratch_u16.h"
#include "simdcomp_for_codec_uint16_w256.h"  // simdcomp_for_w256_detail + w256 externs

// FoR-fused PFor residual codec (external/TurboPFor/lib/vp4d256v16_for_fused.c).
extern "C" size_t   p4nenc256v16_for(uint16_t* in, size_t n, unsigned char* out);
extern "C" uint32_t p4ndec256v16_for_sum(const unsigned char* in, unsigned n,
                                         const uint16_t* anchors, unsigned w,
                                         unsigned sh, int mode, int madd);
// Plain fused baseline decoder (vp4d256v16_fused.c). nobc decodes the residual
// stream with this (no anchor) and adds w·Σ(anchors) — sum(v)=sum(r)+w·Σ(anchor).
// _madd twin sums with vpmaddwd (matches simdcomp nobc_madd); used when the
// residuals are madd-safe (< 2^15).
extern "C" uint32_t p4ndec256v16_sum(const unsigned char* in, unsigned n);
extern "C" uint32_t p4ndec256v16_sum_madd(const unsigned char* in, unsigned n);
// SUM-only fast decoder: exceptions add (Σ excess)<<b scalar-ly (no per-OutReg
// pshufb merge). Default for the madd-safe nobc path; FOR_SUM_MERGE=1 forces the
// old per-position merge (p4ndec256v16_sum_madd) for A/B.
extern "C" uint32_t p4ndec256v16_sum_fast(const unsigned char* in, unsigned n);
// Conservative residual-payload byte bound (shared with the non-FoR fused codec).
extern "C" size_t   p4nbound256v16_fused(size_t n);

namespace turbopfor_for_detail {
// SUM-fast vs old merge for the madd nobc residual decode (A/B via env, read once).
static inline bool use_sum_fast() {
  static int v = -1;
  if (v < 0) { const char* e = std::getenv("FOR_SUM_MERGE"); v = (e && *e && *e != '0') ? 0 : 1; }
  return v != 0;
}
// FoR anchor granularity mode, matching vp4d256v16_for_fused.c's FOR_* enum
// (NOTE: a different numbering than simdcomp_for_w256_detail::decode_mode).
enum { FOR_UNIFORM = 0, FOR_SCALAR = 1, FOR_HALF = 2, FOR_QUARTER = 3 };
static inline int driver_mode(size_t w) {
  if (w >= 256) return FOR_UNIFORM;
  if (w >= 16) return FOR_SCALAR;
  return (w == 8) ? FOR_HALF : FOR_QUARTER;
}
}  // namespace turbopfor_for_detail

// ── Regular FoR (TurboPFor) ──────────────────────────────────────────────────
//
// Layout (separate=true):
//   [num_blk u32][madd_safe u8][anchors u16 × num_anchors][PFor residual payload]
// Layout (separate=false):
//   [num_blk u32][madd_safe u8][anchor_bs u8 × (num_anchors/256)][anchor payloads]
//   [PFor residual payload]
class TurboPForFusedForCodecU16 : public StatefulIntegerCodec<uint16_t> {
 public:
  std::vector<uint8_t> compressed;
  size_t window_;
  bool separate_;
  FusedAggImpl agg_;
  bool nobc_;  // nobc: anchor accumulated in a scalar (off port 5), not added SIMD

  explicit TurboPForFusedForCodecU16(size_t window = 16, bool separate = false,
                                     FusedAggImpl agg = FusedAggImpl::kMadd,
                                     bool nobc = false)
      : window_(window), separate_(separate), agg_(agg), nobc_(nobc) {
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

    // 1. Per-window anchors (= min).
    uint16_t* anchors = s_anchor;
    for (size_t a = 0; a < num_anchors; ++a) {
      const uint16_t* p = in + a * w;
      uint16_t m = p[0];
      for (size_t i = 1; i < w; ++i)
        if (p[i] < m) m = p[i];
      anchors[a] = m;
    }

    // 2. Header + anchor region into scratch. madd_safe is filled in step 3
    //    (after residuals): bc aggregates the full value, nobc aggregates only the
    //    residual, so the < 2^15 check is on different data per variant.
    auto& scratch = GetPackScratch();
    uint8_t* base = scratch.data();
    *reinterpret_cast<uint32_t*>(base) = (uint32_t)num_blk;
    uint8_t* cur = base + sizeof(uint32_t) + 1;
    if (separate_) {
      std::memcpy(cur, anchors, num_anchors * sizeof(uint16_t));
      cur += num_anchors * sizeof(uint16_t);
    } else {
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

    // 3. Residuals (value − window anchor) into s_res, then PFor-encode them.
    //    Track the OR of residuals (for nobc madd-safety) and of values (for bc).
    __m256i resor = _mm256_setzero_si256();
    uint16_t orall = 0;
    for (size_t i = 0; i < length; ++i) orall |= in[i];
    for (size_t k = 0; k < num_blk; ++k) {
      __m256i corr[16];
      build_corrections(corr, anchors, k, w);
      uint16_t* dst = s_res + k * kBlk;
      for (size_t j = 0; j < kOutRegs; ++j) {
        __m256i v = _mm256_loadu_si256(
            reinterpret_cast<const __m256i*>(in + k * kBlk + j * kLanes));
        __m256i r = _mm256_sub_epi16(v, corr[j]);
        _mm256_storeu_si256(reinterpret_cast<__m256i*>(dst + j * kLanes), r);
        resor = _mm256_or_si256(resor, r);
      }
    }
    // madd_safe: nobc checks max residual < 2^15 (no residual lane has bit 15),
    // bc checks max value < 2^15. testz==1 ⇔ (resor & 0x8000) is all-zero.
    const bool res_safe =
        _mm256_testz_si256(resor, _mm256_set1_epi16((short)0x8000)) != 0;
    base[sizeof(uint32_t)] = (nobc_ ? res_safe : (orall < 0x8000u)) ? 1 : 0;
    assert((size_t)(cur - base) + p4nbound256v16_fused(length) <= scratch.size());
    size_t pbytes = p4nenc256v16_for(s_res, length, cur);
    cur += pbytes;

    const size_t actual = (size_t)(cur - scratch.data());
    assert(actual <= scratch.size());
    compressed.assign(scratch.data(), scratch.data() + actual);
  }

  void DecodeArray(uint16_t* out, const std::size_t length) override {
    using namespace simdcomp_for_w256_detail;
    using namespace turbopfor_for_detail;
    const size_t w = (window_ >= length) ? length : window_;
    const size_t num_blk = length / kBlk;
    const size_t num_anchors = length / w;

    const uint8_t* p = compressed.data();
    const uint32_t got = *reinterpret_cast<const uint32_t*>(p);
    (void)got;
    assert(got == num_blk);
    p += sizeof(uint32_t);
    const bool madd_safe = *p++ != 0;
    const int madd = ((agg_ == FusedAggImpl::kMadd) && madd_safe) ? 1 : 0;

    const unsigned sh = (unsigned)__builtin_ctzll((unsigned long long)w);
    const int mode = driver_mode(w);

    const uint16_t* anchors;
    const unsigned char* payload;
    if (separate_) {
      anchors = reinterpret_cast<const uint16_t*>(p);
      payload = reinterpret_cast<const unsigned char*>(p + num_anchors * sizeof(uint16_t));
    } else {
      // Materialize the packed anchor stream into s_anchor (two-pass). NB: 2 pass not great...
      const size_t na_blk = (num_anchors + kBlk - 1) / kBlk;
      const uint8_t* abs = p;
      const uint8_t* apay = abs + na_blk;
      const uint8_t* acur = apay;
      for (size_t ab = 0; ab < na_blk; ++ab) {
        simdunpack_u16_w256_store(reinterpret_cast<const __m256i*>(acur),
                                  s_anchor + ab * kBlk, abs[ab]);
        acur += (size_t)abs[ab] * sizeof(__m256i);
      }
      anchors = s_anchor;
      payload = reinterpret_cast<const unsigned char*>(acur);
    }

    uint32_t total;
    if (nobc_) {
      // Decoupled: sum(value) = sum(residual) + w·Σ(window anchors). Decode the
      // residual stream with the plain baseline (fast, no anchor), add the scalar.
      uint64_t asum = 0;
      for (size_t a = 0; a < num_anchors; ++a) asum += anchors[a];
      const uint32_t rsum =
          madd ? (turbopfor_for_detail::use_sum_fast()
                      ? p4ndec256v16_sum_fast(payload, (unsigned)length)
                      : p4ndec256v16_sum_madd(payload, (unsigned)length))
               : p4ndec256v16_sum(payload, (unsigned)length);
      total = rsum + (uint32_t)((uint64_t)w * asum);
    } else {
      total = p4ndec256v16_for_sum(payload, (unsigned)length, anchors,
                                   (unsigned)w, sh, mode, madd);
    }
    out[length] = (uint16_t)(total & 0xFFFF);
    out[length + 1] = (uint16_t)(total >> 16);
  }

  std::size_t EncodedNumValues() override { return compressed.size(); }
  std::size_t EncodedSizeValue() override { return sizeof(uint8_t); }
  virtual ~TurboPForFusedForCodecU16() {}
  std::string name() const override {
    std::string n = "TurboPFor_fused_for_256_w" + std::to_string(window_);
    if (separate_) n += "_sep";
    if (nobc_) n += "_nobc";
    n += (agg_ == FusedAggImpl::kMadd) ? "_madd" : "_unpack";
    return n;
  }
  std::size_t GetOverflowSize(size_t) const override { return 64; }
  StatefulIntegerCodec<uint16_t>* CloneFresh() const override {
    return new TurboPForFusedForCodecU16(window_, separate_, agg_, nobc_);
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

// ── Hierarchical FoR (TurboPFor) ─────────────────────────────────────────────
//
// Layout:
//   [num_blk u32][madd_safe u8]
//   [global anchors u16 × num_outer][b_local u8][delta payload @ b_local]
//   [PFor residual payload]
class TurboPForFusedForHierarchicalCodecU16
    : public StatefulIntegerCodec<uint16_t> {
 public:
  std::vector<uint8_t> compressed;
  size_t outer_;
  size_t inner_;
  FusedAggImpl agg_;
  bool nobc_;

  TurboPForFusedForHierarchicalCodecU16(size_t outer = 256, size_t inner = 16,
                                        FusedAggImpl agg = FusedAggImpl::kMadd,
                                        bool nobc = false)
      : outer_(outer), inner_(inner), agg_(agg), nobc_(nobc) {
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
    const size_t ratio = W / w;

    uint16_t* gmin = s_delta;       // globals (num_outer)
    uint16_t* lanchor = s_anchor;   // local anchors (num_inner)
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

    uint16_t orall = 0;
    for (size_t i = 0; i < length; ++i) orall |= in[i];
    auto& scratch = GetPackScratch();
    uint8_t* basep = scratch.data();
    *reinterpret_cast<uint32_t*>(basep) = (uint32_t)num_blk;
    // madd_safe filled after residuals (see regular codec note).
    uint8_t* cur = basep + sizeof(uint32_t) + 1;
    std::memcpy(cur, gmin, num_outer * sizeof(uint16_t));
    cur += num_outer * sizeof(uint16_t);
    *cur++ = (uint8_t)b_local;

    uint16_t* deltas = s_res;  // num_inner (residual pass not started yet)
    for (size_t l = 0; l < num_inner; ++l)
      deltas[l] = (uint16_t)(lanchor[l] - gmin[l / ratio]);
    const size_t nd_blk = (num_inner + kBlk - 1) / kBlk;
    for (size_t i = num_inner; i < nd_blk * kBlk; ++i) deltas[i] = 0;
    for (size_t db = 0; db < nd_blk; ++db) {
      simdpack_u16_w256(deltas + db * kBlk, reinterpret_cast<__m256i*>(cur),
                        b_local);
      cur += (size_t)b_local * sizeof(__m256i);
    }

    // Residuals (value − local anchor) into s_res, then PFor-encode.
    __m256i resor = _mm256_setzero_si256();
    for (size_t k = 0; k < num_blk; ++k) {
      __m256i corr[16];
      build_corrections(corr, lanchor, k, w);
      uint16_t* dst = s_res + k * kBlk;  // deltas already packed
      for (size_t j = 0; j < kOutRegs; ++j) {
        __m256i v = _mm256_loadu_si256(
            reinterpret_cast<const __m256i*>(in + k * kBlk + j * kLanes));
        __m256i r = _mm256_sub_epi16(v, corr[j]);
        _mm256_storeu_si256(reinterpret_cast<__m256i*>(dst + j * kLanes), r);
        resor = _mm256_or_si256(resor, r);
      }
    }
    const bool res_safe =
        _mm256_testz_si256(resor, _mm256_set1_epi16((short)0x8000)) != 0;
    basep[sizeof(uint32_t)] = (nobc_ ? res_safe : (orall < 0x8000u)) ? 1 : 0;
    assert((size_t)(cur - basep) + p4nbound256v16_fused(length) <= scratch.size());
    size_t pbytes = p4nenc256v16_for(s_res, length, cur);
    cur += pbytes;

    const size_t actual = (size_t)(cur - scratch.data());
    assert(actual <= scratch.size());
    compressed.assign(scratch.data(), scratch.data() + actual);
  }

  void DecodeArray(uint16_t* out, const std::size_t length) override {
    using namespace simdcomp_for_w256_detail;
    using namespace turbopfor_for_detail;
    const size_t W = (outer_ >= length) ? length : outer_;
    const size_t w = (inner_ >= length) ? length : inner_;
    const size_t num_outer = length / W;
    const size_t num_inner = length / w;
    const size_t ratio = W / w;

    const uint8_t* p = compressed.data();
    p += sizeof(uint32_t);
    const bool madd_safe = *p++ != 0;
    const int madd = ((agg_ == FusedAggImpl::kMadd) && madd_safe) ? 1 : 0;

    const uint16_t* gmin = reinterpret_cast<const uint16_t*>(p);
    p += num_outer * sizeof(uint16_t);
    const uint32_t b_local = *p++;

    // Materialize local anchors L = global + delta into s_anchor (two-pass).
    const size_t nd_blk = (num_inner + kBlk - 1) / kBlk;
    const uint8_t* dpay = p;
    const unsigned char* payload =
        reinterpret_cast<const unsigned char*>(dpay + nd_blk * (size_t)b_local * sizeof(__m256i));
    const unsigned shr = (unsigned)__builtin_ctzll((unsigned long long)ratio);
    const uint8_t* dcur = dpay;
    for (size_t ab = 0; ab < nd_blk; ++ab) {
      uint16_t* a = s_anchor + ab * kBlk;
      simdunpack_u16_w256_store(reinterpret_cast<const __m256i*>(dcur), a, b_local);
      dcur += (size_t)b_local * sizeof(__m256i);
      const size_t l0 = ab * kBlk;
      const size_t lend = (l0 + kBlk < num_inner) ? (l0 + kBlk) : num_inner;
      for (size_t l = l0; l < lend; ++l)
        a[l - l0] = (uint16_t)(a[l - l0] + gmin[l >> shr]);
    }

    const unsigned sh = (unsigned)__builtin_ctzll((unsigned long long)w);
    const int mode = driver_mode(w);
    uint32_t total;
    if (nobc_) {
      // Decoupled: each inner window (w elems) contributes w·local_anchor.
      uint64_t asum = 0;
      for (size_t l = 0; l < num_inner; ++l) asum += s_anchor[l];
      const uint32_t rsum =
          madd ? (turbopfor_for_detail::use_sum_fast()
                      ? p4ndec256v16_sum_fast(payload, (unsigned)length)
                      : p4ndec256v16_sum_madd(payload, (unsigned)length))
               : p4ndec256v16_sum(payload, (unsigned)length);
      total = rsum + (uint32_t)((uint64_t)w * asum);
    } else {
      total = p4ndec256v16_for_sum(payload, (unsigned)length, s_anchor,
                                   (unsigned)w, sh, mode, madd);
    }
    out[length] = (uint16_t)(total & 0xFFFF);
    out[length + 1] = (uint16_t)(total >> 16);
  }

  std::size_t EncodedNumValues() override { return compressed.size(); }
  std::size_t EncodedSizeValue() override { return sizeof(uint8_t); }
  virtual ~TurboPForFusedForHierarchicalCodecU16() {}
  std::string name() const override {
    std::string n = "TurboPFor_fused_for_hier_256_g" + std::to_string(outer_) +
                    "_l" + std::to_string(inner_);
    if (nobc_) n += "_nobc";
    n += (agg_ == FusedAggImpl::kMadd) ? "_madd" : "_unpack";
    return n;
  }
  std::size_t GetOverflowSize(size_t) const override { return 64; }
  StatefulIntegerCodec<uint16_t>* CloneFresh() const override {
    return new TurboPForFusedForHierarchicalCodecU16(outer_, inner_, agg_, nobc_);
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
