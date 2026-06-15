// Two-band lock-step fused NDVI decode (256-bit, 16-lane, independent b).
//
// The 2-band analog of the single-band fused-sum kernel: for each 256-elem
// sub-block, OutReg j of band A and OutReg j of band B are produced IN REGISTERS
// (immediate-shift unpack) and handed straight to an NDVI aggregate — never
// materialised to a buffer (that store+reload is what kills the L1-temp variant).
//
// Independent b: bit-widths bA, bB are compile-time per kernel, so the shifts are
// immediates. One out-of-line kernel per (bA,bB) pair (17x17=289), dispatched by
// g_tbl[bA][bB] — exactly like single-band's switch(b), so only the (bA,bB) pairs
// that actually occur go hot in I-cache.
//
// Built into its own lib with -mno-avx512f (AVX2-only target; the server SIGILLs
// on EVEX-encoded YMM that -march=native would otherwise emit).

#include <immintrin.h>
#include <cstdint>
#include <cstddef>
#include <utility>

namespace {

// OutReg j of a 256-block at bit-width B (compile-time): bits [j*B, j*B+B) of
// each 16-bit lane's stream, straddling into the next 16-bit word when
// (j*B)%16 + B > 16. B==0 -> all-zero OutReg (mask 0; stray load is harmless).
template <int B, int J>
static inline __m256i ex(const __m256i* in) {
  constexpr int o = J * B, w = o >> 4, s = o & 15;
  __m256i v = _mm256_srli_epi16(_mm256_loadu_si256(in + w), s);
  if constexpr (s + B > 16)
    v = _mm256_or_si256(v, _mm256_slli_epi16(_mm256_loadu_si256(in + w + 1),
                                             16 - s));
  constexpr int m = (B >= 16) ? 0xFFFF : ((1 << B) - 1);
  return _mm256_and_si256(v, _mm256_set1_epi16((short)m));
}

// Kernel ladder (op selects the per-OutReg-pair aggregate), so the cost deltas
// isolate decode vs widen vs divide — the 2-band analog of the single-band ladder.
enum { OP_NOOP = 0, OP_ADD = 1, OP_DIV = 2, OP_RCP = 3, OP_RCPRAW = 4 };

template <int OP>
static inline void acc_op(__m256i va, __m256i vb, __m256& accf, __m256i& accx) {
  if constexpr (OP == OP_NOOP) {
    accx = _mm256_xor_si256(accx, _mm256_xor_si256(va, vb));
    return;
  }
  if constexpr (OP == OP_ADD) {
    // Integer widen-sum of both bands — the 2-band analog of single-band sum's
    // aggregate_sums_u16 (unpacklo/hi widen + add). NOT float (that widen would
    // dominate and hide the bandwidth win).
    const __m256i z = _mm256_setzero_si256();
    accx = _mm256_add_epi32(accx, _mm256_unpacklo_epi16(va, z));
    accx = _mm256_add_epi32(accx, _mm256_unpackhi_epi16(va, z));
    accx = _mm256_add_epi32(accx, _mm256_unpacklo_epi16(vb, z));
    accx = _mm256_add_epi32(accx, _mm256_unpackhi_epi16(vb, z));
    return;
  }
  // OP_DIV / OP_RCP: both widen to float first; differ only in how they divide.
  // OP_DIV: plain a/b via vdivps (exact, slow divide unit).
  // OP_RCP: a * rcp(b) with one Newton-Raphson step (vrcpps+NR, no divide unit).
  // Neither applies NDVI formula — the goal is to isolate the divide vs rcp cost.
  __m128i alo = _mm256_castsi256_si128(va), ahi = _mm256_extracti128_si256(va, 1);
  __m128i blo = _mm256_castsi256_si128(vb), bhi = _mm256_extracti128_si256(vb, 1);
  __m256 a0 = _mm256_cvtepi32_ps(_mm256_cvtepu16_epi32(alo));
  __m256 b0 = _mm256_cvtepi32_ps(_mm256_cvtepu16_epi32(blo));
  __m256 a1 = _mm256_cvtepi32_ps(_mm256_cvtepu16_epi32(ahi));
  __m256 b1 = _mm256_cvtepi32_ps(_mm256_cvtepu16_epi32(bhi));
  if constexpr (OP == OP_DIV) {
    accf = _mm256_add_ps(accf, _mm256_div_ps(a0, b0));
    accf = _mm256_add_ps(accf, _mm256_div_ps(a1, b1));
  }
  if constexpr (OP == OP_RCP) {
    __m256 r0 = _mm256_rcp_ps(b0), r1 = _mm256_rcp_ps(b1);
    r0 = _mm256_mul_ps(r0, _mm256_sub_ps(_mm256_set1_ps(2.f), _mm256_mul_ps(b0, r0)));
    r1 = _mm256_mul_ps(r1, _mm256_sub_ps(_mm256_set1_ps(2.f), _mm256_mul_ps(b1, r1)));
    accf = _mm256_add_ps(accf, _mm256_mul_ps(a0, r0));
    accf = _mm256_add_ps(accf, _mm256_mul_ps(a1, r1));
  }
  if constexpr (OP == OP_RCPRAW) {
    // vrcpps only, no NR step — ~12-bit accuracy, saves 3 dependent mul/sub ops.
    accf = _mm256_add_ps(accf, _mm256_mul_ps(a0, _mm256_rcp_ps(b0)));
    accf = _mm256_add_ps(accf, _mm256_mul_ps(a1, _mm256_rcp_ps(b1)));
  }
}

// One sub-block, both bands, lock-step (16 OutRegs). Out-of-line per (bA,bB,OP).
template <int OP, int bA, int bB>
__attribute__((noinline)) static void sub_op(const __m256i* inA,
                                             const __m256i* inB, __m256* accf,
                                             __m256i* accx) {
  __m256 f = *accf;
  __m256i x = *accx;
  [&]<int... J>(std::integer_sequence<int, J...>) {
    ((acc_op<OP>(ex<bA, J>(inA), ex<bB, J>(inB), f, x)), ...);
  }(std::make_integer_sequence<int, 16>{});
  *accf = f;
  *accx = x;
}

using Fn = void (*)(const __m256i*, const __m256i*, __m256*, __m256i*);
Fn g_tbl[5][17][17];

template <int OP, int A>
static void reg_row() {
  [&]<int... B>(std::integer_sequence<int, B...>) {
    ((g_tbl[OP][A][B] = &sub_op<OP, A, B>), ...);
  }(std::make_integer_sequence<int, 17>{});
}

template <int OP>
static void reg_op() {
  [&]<int... A>(std::integer_sequence<int, A...>) {
    (reg_row<OP, A>(), ...);
  }(std::make_integer_sequence<int, 17>{});
}

struct Init {
  Init() {
    reg_op<OP_NOOP>();
    reg_op<OP_ADD>();
    reg_op<OP_DIV>();
    reg_op<OP_RCP>();
    reg_op<OP_RCPRAW>();
  }
} g_init;

static inline double hsum_ps(__m256 v) {
  __m128 lo = _mm256_castps256_ps128(v), hi = _mm256_extractf128_ps(v, 1);
  __m128 s = _mm_add_ps(lo, hi);
  s = _mm_hadd_ps(s, s);
  s = _mm_hadd_ps(s, s);
  return _mm_cvtss_f32(s);
}

static inline double xreduce(__m256i x) {
  alignas(32) int32_t v[8];
  _mm256_store_si256((__m256i*)v, x);
  int64_t r = 0;
  for (int i = 0; i < 8; ++i) r ^= v[i];
  return (double)(r & 0xFFFF);
}

// Uncompressed baseline: same per-OutReg ladder over RAW uint16 (no decode).
template <int OP>
static double raw_loop(const uint16_t* a, const uint16_t* b, size_t length) {
  __m256 f = _mm256_setzero_ps();
  __m256i x = _mm256_setzero_si256();
  for (size_t i = 0; i < length; i += 16)
    acc_op<OP>(_mm256_loadu_si256((const __m256i*)(a + i)),
               _mm256_loadu_si256((const __m256i*)(b + i)), f, x);
  return hsum_ps(f) + xreduce(x);
}

}  // namespace

// Uncompressed 2-band aggregate over raw uint16 grids (same op ladder).
extern "C" double ndvi2_raw(const uint16_t* a, const uint16_t* b, size_t length,
                            int op) {
  return op == 0 ? raw_loop<0>(a, b, length)
       : op == 1 ? raw_loop<1>(a, b, length)
       : op == 2 ? raw_loop<2>(a, b, length)
       : op == 3 ? raw_loop<3>(a, b, length)
                 : raw_loop<4>(a, b, length);
}

// Decode a block-pair (encoded simdcomp_fused: [madd_safe:1][bs:num_sb][payload])
// in lock-step and aggregate. op = OP_NOOP|OP_ADD|OP_DIV. length = elems/block.
extern "C" double ndvi2_indep(const uint8_t* encA, const uint8_t* encB,
                              size_t length, int op) {
  const size_t num_sb = length / 256;
  const uint8_t* bsA = encA + 1;
  const uint8_t* inA = bsA + num_sb;
  const uint8_t* bsB = encB + 1;
  const uint8_t* inB = bsB + num_sb;
  const Fn* tbl = &g_tbl[op][0][0];
  __m256 f = _mm256_setzero_ps();
  __m256i x = _mm256_setzero_si256();
  for (size_t k = 0; k < num_sb; ++k) {
    const uint32_t ba = bsA[k], bb = bsB[k];
    tbl[ba * 17 + bb](reinterpret_cast<const __m256i*>(inA),
                      reinterpret_cast<const __m256i*>(inB), &f, &x);
    inA += (size_t)ba * sizeof(__m256i);
    inB += (size_t)bb * sizeof(__m256i);
  }
  return hsum_ps(f) + xreduce(x);
}
