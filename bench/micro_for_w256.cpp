// Microbench: isolate FoR w=256 overhead into two parts — per-block preamble
// (broadcast) vs. inside the kernel (16 add_epi16 anchor corrections).
//
// Four variants per b value (all L1-resident, NSB sub-blocks):
//   A  baseline:   simdunpack_u16_w128_madd          (no correction, no broadcast)
//   B  full FoR:   set1(anchors[k]) + corrected_uniform_madd  (broadcast + kernel adds)
//   C  bcast-only: set1(anchors[k]) + simdunpack_u16_w128_madd  (broadcast cost only)
//   D  adds-only:  corrected_uniform_madd(fixed anchor)         (kernel adds, no per-iter bcast)
//
// B-A = total FoR overhead
// C-A = per-block broadcast overhead only
// D-A = kernel add_epi16 overhead only (anchor pre-broadcast, amortised out)
// C-A + D-A should ≈ B-A (sanity check)
//
// Build (on server, from repo root):
//   g++ -O3 -march=native -o /tmp/micro_for_w256 bench/micro_for_w256.cpp \
//       build/libSimdCompU16W128.a

#include <immintrin.h>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <random>
#include <vector>
#include <cstring>

extern "C" {
void simdpack_u16_w128(const uint16_t* in, __m128i* out, uint32_t bit);
void simdunpack_u16_w128_madd(const __m128i* in, uint16_t* out, uint32_t bit, __m128i* sum);
// anchor passed by value (pre-broadcast __m128i in XMM register)
void simdunpack_u16_w128_corrected_uniform_madd(const __m128i* in, uint16_t* out,
                                                uint32_t bit, const __m128i anchor,
                                                __m128i* sum);
}

static uint16_t g_scratch[128];

static constexpr int NSB   = 64;    // 64×128 elems; max packed 64×16×16=16KB → fits L1
static constexpr int REPS  = 40000;
static constexpr int WARMUP = 1000;

using Clock = std::chrono::steady_clock;
using ns_dur = std::chrono::duration<double, std::nano>;

template <class F>
static double time_ns(F&& f) {
    for (int i = 0; i < WARMUP; i++) f();
    auto t0 = Clock::now();
    for (int i = 0; i < REPS; i++) f();
    auto t1 = Clock::now();
    return ns_dur(t1 - t0).count() / REPS;
}

int main() {
    std::mt19937 rng(42);

    printf("%-4s | %8s %8s %8s %8s | %8s %8s %8s | %8s\n",
           "b", "A base", "B full", "C bcast", "D adds",
           "B-A tot", "C-A bcast", "D-A adds", "sum_ok?");
    printf("-----+------------------------------------------+---------------------------+---------\n");

    for (uint32_t b : {0u, 1u, 2u, 4u, 8u, 12u, 16u}) {
        const size_t wpb = b; // __m128i words per sub-block

        std::vector<__m128i> comp(wpb > 0 ? NSB * wpb : 1, _mm_setzero_si128());
        uint16_t raw[128];
        const uint16_t mask_b = (b == 16) ? 0xFFFF : (uint16_t)((1u << b) - 1);
        for (int k = 0; k < NSB; k++) {
            for (int i = 0; i < 128; i++) raw[i] = (uint16_t)(rng() & mask_b);
            if (b > 0) simdpack_u16_w128(raw, comp.data() + k * wpb, b);
        }

        uint16_t anchors[NSB + 4];
        for (int k = 0; k < NSB + 4; k++) anchors[k] = (uint16_t)(rng() & 0x7FFF);

        // Fixed anchor (pre-broadcast outside the timed loop) for variant D
        const __m128i fixed_anchor = _mm_set1_epi16((short)anchors[0]);

        // ── A: baseline (no broadcast, no adds) ───────────────────────────────
        double tA = time_ns([&] {
            for (int k = 0; k < NSB; k++) {
                __m128i sum = _mm_setzero_si128();
                const __m128i* in = wpb > 0 ? comp.data() + k * wpb : comp.data();
                simdunpack_u16_w128_madd(in, g_scratch, b, &sum);
                asm volatile("" :: "x"(sum) : "memory");
            }
        });

        // ── B: full FoR (per-iter broadcast + corrected kernel) ───────────────
        double tB = time_ns([&] {
            for (int k = 0; k < NSB; k++) {
                __m128i sum = _mm_setzero_si128();
                const __m128i* in = wpb > 0 ? comp.data() + k * wpb : comp.data();
                __m128i bc = _mm_set1_epi16((short)anchors[k]);
                simdunpack_u16_w128_corrected_uniform_madd(in, g_scratch, b, bc, &sum);
                asm volatile("" :: "x"(sum) : "memory");
            }
        });

        // ── C: broadcast cost only (broadcast forced, then plain kernel) ──────
        double tC = time_ns([&] {
            for (int k = 0; k < NSB; k++) {
                __m128i sum = _mm_setzero_si128();
                const __m128i* in = wpb > 0 ? comp.data() + k * wpb : comp.data();
                __m128i bc = _mm_set1_epi16((short)anchors[k]);
                asm volatile("" :: "x"(bc));  // force the broadcast, discard result
                simdunpack_u16_w128_madd(in, g_scratch, b, &sum);
                asm volatile("" :: "x"(sum) : "memory");
            }
        });

        // ── D: kernel adds only (fixed pre-broadcast anchor, amortised) ───────
        double tD = time_ns([&] {
            for (int k = 0; k < NSB; k++) {
                __m128i sum = _mm_setzero_si128();
                const __m128i* in = wpb > 0 ? comp.data() + k * wpb : comp.data();
                simdunpack_u16_w128_corrected_uniform_madd(in, g_scratch, b, fixed_anchor, &sum);
                asm volatile("" :: "x"(sum) : "memory");
            }
        });

        double pA = tA / NSB, pB = tB / NSB, pC = tC / NSB, pD = tD / NSB;
        double tot   = pB - pA;
        double bcast = pC - pA;
        double adds  = pD - pA;
        double sum_check = bcast + adds;

        printf("%-4u | %8.2f %8.2f %8.2f %8.2f | %8.2f %8.2f %8.2f | %6.2f%s\n",
               b, pA, pB, pC, pD,
               tot, bcast, adds, sum_check,
               (sum_check > 0.8*tot && sum_check < 1.2*tot) ? " OK" : " !");
    }

    printf("\n");
    printf("All times ns/sub-block (128 elements). NSB=%d, REPS=%d.\n", NSB, REPS);
    printf("C-A=broadcast cost; D-A=kernel add_epi16 cost; B-A=total. sum_ok if (C-A)+(D-A)~=B-A.\n");

    return 0;
}
