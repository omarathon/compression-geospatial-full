// Microbench: isolate FoR w=256 overhead vs baseline at different bit widths.
//
// Tests (all L1-resident, NSB sub-blocks repeated):
//   baseline(b)   = simdunpack_u16_w128_madd            (no correction)
//   for_w256(b)   = simdunpack_u16_w128_corrected_uniform_madd (1 anchor/block, 16 add_epi16)
//
// Key ablations:
//   for_w256(0)   = pure anchor-aggregation cost (no real unpack, since b=0 residuals=0)
//   for_w256(b) - baseline(b) = overhead of anchor correction at this b
//   if this delta ≈ for_w256(0) across all b → adds are NOT hidden by OoO (throughput-bound)
//   if this delta shrinks as b rises → adds ARE overlapped with unpack (OoO wins)
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
// anchor is pre-broadcast by the caller (passed by value in XMM register)
void simdunpack_u16_w128_corrected_uniform_madd(const __m128i* in, uint16_t* out,
                                                uint32_t bit, const __m128i anchor,
                                                __m128i* sum);
}

static uint16_t g_scratch[128];

static constexpr int NSB  = 64;   // 64 sub-blocks × 128 elems; max packed = 64×16×16=16KB → L1
static constexpr int REPS = 30000;
static constexpr int WARMUP = 500;

using Clock = std::chrono::steady_clock;
using ns    = std::chrono::duration<double, std::nano>;

template <class F>
static double time_ns(int reps, F&& f) {
    for (int i = 0; i < WARMUP; i++) f();
    auto t0 = Clock::now();
    for (int i = 0; i < reps; i++) f();
    auto t1 = Clock::now();
    return ns(t1 - t0).count() / reps;
}

int main() {
    std::mt19937 rng(42);

    printf("%-4s | %10s %10s | %10s | overhead_vs_b0\n",
           "b", "base ns/sb", "for ns/sb", "delta ns/sb");
    printf("-----+---------------------+------------+--------------\n");

    double for_b0_ns = 0;

    for (uint32_t b : {0u, 2u, 4u, 8u, 12u, 16u}) {
        const size_t packed_words_per_sb = b; // 128 elems × b bits / 128 bits = b __m128i words

        // Aligned storage: b __m128i words per sub-block, NSB sub-blocks (+1 guard)
        std::vector<__m128i> comp((packed_words_per_sb > 0 ? NSB * packed_words_per_sb : 1),
                                  _mm_setzero_si128());
        uint16_t raw[128];
        const uint16_t mask_b = (b == 16) ? 0xFFFF : (uint16_t)((1u << b) - 1);
        for (int k = 0; k < NSB; k++) {
            for (int i = 0; i < 128; i++) raw[i] = (uint16_t)(rng() & mask_b);
            if (b > 0)
                simdpack_u16_w128(raw, comp.data() + k * packed_words_per_sb, b);
            // b=0: no pack needed, input ignored by the kernel
        }

        // Anchors: one per sub-block (w=256 → uniform, 1 anchor per 128-elem block)
        uint16_t anchors[NSB + 4];
        for (int k = 0; k < NSB + 4; k++) anchors[k] = (uint16_t)(rng() & 0x7FFF);

        // ── Baseline ──────────────────────────────────────────────────────────
        volatile uint32_t sink_base = 0;
        double t_base = time_ns(REPS, [&] {
            for (int k = 0; k < NSB; k++) {
                __m128i sum = _mm_setzero_si128();
                const __m128i* in = (b > 0) ? comp.data() + k * packed_words_per_sb : comp.data();
                simdunpack_u16_w128_madd(in, g_scratch, b, &sum);
                asm volatile("" :: "x"(sum) : "memory");
            }
        });

        // ── FoR w=256 (corrected_uniform) ─────────────────────────────────────
        volatile uint32_t sink_for = 0;
        double t_for = time_ns(REPS, [&] {
            for (int k = 0; k < NSB; k++) {
                __m128i sum = _mm_setzero_si128();
                const __m128i* in = (b > 0) ? comp.data() + k * packed_words_per_sb : comp.data();
                __m128i anchor_bc = _mm_set1_epi16((short)anchors[k]);
                simdunpack_u16_w128_corrected_uniform_madd(in, g_scratch, b, anchor_bc, &sum);
                asm volatile("" :: "x"(sum) : "memory");
            }
        });

        double base_per_sb = t_base / NSB;
        double for_per_sb  = t_for  / NSB;
        double delta       = for_per_sb - base_per_sb;

        if (b == 0) for_b0_ns = for_per_sb; // pure anchor cost (no unpack)

        if (b == 0) {
            printf("%-4u | %10.2f %10.2f | %10.2f | (reference: pure anchor cost)\n",
                   b, base_per_sb, for_per_sb, delta);
        } else {
            double ratio_to_b0 = delta / for_b0_ns;
            printf("%-4u | %10.2f %10.2f | %10.2f | %.2fx of b=0 anchor cost\n",
                   b, base_per_sb, for_per_sb, delta, ratio_to_b0);
        }
        (void)sink_base; (void)sink_for;
    }

    printf("\n");
    printf("Interpretation:\n");
    printf("  delta ≈ for(b=0)        → adds NOT hidden by OoO, throughput-bound\n");
    printf("  delta << for(b=0)       → adds ARE overlapped with unpack (OoO wins)\n");
    printf("  delta roughly constant  → pure 16-add cost, independent of b\n");

    return 0;
}
