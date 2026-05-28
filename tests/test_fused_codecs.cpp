#include <cstdint>
#include <cstdio>
#include <memory>
#include <numeric>
#include <random>
#include <vector>

#include <gtest/gtest.h>

#include "simdcomp_fused_codec_uint16.h"
#include "simdcomp_for_codec_uint16.h"
#include "fastpfor_fused_codec_uint16.h"

// ── Helper ────────────────────────────────────────────────────────────────────
//
// Encodes `data` with `codec`, decodes it, then checks that the fused sum
// stored in the two overflow slots equals the true uint32 sum of the input.
// Both codecs store the result the same way:
//   out[n]     = low  16 bits of uint32 sum
//   out[n + 1] = high 16 bits of uint32 sum
// Reconstruction: (uint32_t)out[n] | ((uint32_t)out[n+1] << 16)

static void CheckFusedSum(const std::vector<uint16_t>& data,
                           StatefulIntegerCodec<uint16_t>& codec) {
  codec.clear();

  // Reference: plain uint32 accumulation (wraps on overflow, same as the codec).
  uint32_t expected = 0;
  for (uint16_t v : data) expected += static_cast<uint32_t>(v);

  // Encode.
  codec.AllocEncoded(data.data(), data.size());
  codec.EncodeArray(data.data(), data.size());

  // Decode into a buffer that includes the overflow slots.
  const size_t n = data.size();
  std::vector<uint16_t> out(n + codec.GetOverflowSize(n), 0xDEAD);
  codec.DecodeArray(out.data(), n);

  // Reconstruct fused sum from overflow slots.
  uint32_t fused = static_cast<uint32_t>(out[n]) |
                   (static_cast<uint32_t>(out[n + 1]) << 16);

  EXPECT_EQ(fused, expected)
      << codec.name() << ": fused sum " << fused
      << " != expected " << expected
      << " for data.size()=" << n;

  codec.clear();
}

// ── Data helpers ──────────────────────────────────────────────────────────────

// All-zero block – trivial but exercises the codec path.
static std::vector<uint16_t> MakeZeros(size_t n) {
  return std::vector<uint16_t>(n, 0);
}

// Period-16 sawtooth: [0,1,...,15, 0,1,...,15, …].
// Best-case for delta-LOCAL (anchors delta from 0 = 0; intra-window deltas = 1).
static std::vector<uint16_t> MakeSawtooth16(size_t n) {
  std::vector<uint16_t> v(n);
  for (size_t i = 0; i < n; ++i) v[i] = static_cast<uint16_t>(i & 15);
  return v;
}

// Constant block (all same value).
static std::vector<uint16_t> MakeConstant(size_t n, uint16_t val) {
  return std::vector<uint16_t>(n, val);
}

// 0, 1, 2, … (mod 65536) sequential values.
static std::vector<uint16_t> MakeSequential(size_t n) {
  std::vector<uint16_t> v(n);
  for (size_t i = 0; i < n; ++i) v[i] = static_cast<uint16_t>(i);
  return v;
}

// Pseudo-random values with a fixed seed.
static std::vector<uint16_t> MakeRandom(size_t n, uint32_t seed = 42) {
  std::mt19937 gen(seed);
  std::uniform_int_distribution<uint16_t> dist(0, 65535);
  std::vector<uint16_t> v(n);
  for (auto& x : v) x = dist(gen);
  return v;
}

// Same 256-element data that test_uint16_codecs_server uses.
static std::vector<uint16_t> MakeLargeFixed() {
  const uint16_t kMax = 65535;
  const int kN = 64;
  std::mt19937 gen(42);
  std::uniform_int_distribution<uint16_t> distr(0, kMax);
  std::vector<uint16_t> v;
  v.reserve(kN * 4);
  for (int i = 0; i < kN; ++i) {
    v.push_back(static_cast<uint16_t>(i));
    v.push_back(static_cast<uint16_t>(kMax - i));
    v.push_back(distr(gen));
    v.push_back(distr(gen));
  }
  return v;  // 256 elements
}

// ── Fixture ───────────────────────────────────────────────────────────────────

class FusedSumTest : public ::testing::Test {
 protected:
  // simdpack_length_u16 / FastPFor both handle non-multiples of 256, but
  // simdcomp is most natural with multiples of 256.  Use 256 and 1024.
  static constexpr size_t kSmall  = 256;
  static constexpr size_t kMedium = 1024;
};

// ── SimdComp fused ────────────────────────────────────────────────────────────

TEST_F(FusedSumTest, SimdCompFused_Zeros) {
  SimdCompFusedCodecU16 c;
  CheckFusedSum(MakeZeros(kSmall), c);
  CheckFusedSum(MakeZeros(kMedium), c);
}

TEST_F(FusedSumTest, SimdCompFused_Constant) {
  SimdCompFusedCodecU16 c;
  CheckFusedSum(MakeConstant(kSmall,  1), c);
  CheckFusedSum(MakeConstant(kSmall,  65535), c);
  CheckFusedSum(MakeConstant(kMedium, 100), c);
}

TEST_F(FusedSumTest, SimdCompFused_Sequential) {
  SimdCompFusedCodecU16 c;
  CheckFusedSum(MakeSequential(kSmall), c);
  CheckFusedSum(MakeSequential(kMedium), c);
}

TEST_F(FusedSumTest, SimdCompFused_Random) {
  SimdCompFusedCodecU16 c;
  CheckFusedSum(MakeRandom(kSmall,  42), c);
  CheckFusedSum(MakeRandom(kMedium, 99), c);
}

TEST_F(FusedSumTest, SimdCompFused_FixedBlock) {
  SimdCompFusedCodecU16 c;
  CheckFusedSum(MakeLargeFixed(), c);
}

// ── FastPFor fused ────────────────────────────────────────────────────────────

TEST_F(FusedSumTest, FastPForFused_Zeros) {
  FastPForFusedCodecU16 c;
  CheckFusedSum(MakeZeros(kSmall), c);
  CheckFusedSum(MakeZeros(kMedium), c);
}

TEST_F(FusedSumTest, FastPForFused_Constant) {
  FastPForFusedCodecU16 c;
  CheckFusedSum(MakeConstant(kSmall,  1), c);
  CheckFusedSum(MakeConstant(kSmall,  65535), c);
  CheckFusedSum(MakeConstant(kMedium, 100), c);
}

TEST_F(FusedSumTest, FastPForFused_Sequential) {
  FastPForFusedCodecU16 c;
  CheckFusedSum(MakeSequential(kSmall), c);
  CheckFusedSum(MakeSequential(kMedium), c);
}

TEST_F(FusedSumTest, FastPForFused_Random) {
  FastPForFusedCodecU16 c;
  CheckFusedSum(MakeRandom(kSmall,  42), c);
  CheckFusedSum(MakeRandom(kMedium, 99), c);
}

TEST_F(FusedSumTest, FastPForFused_FixedBlock) {
  FastPForFusedCodecU16 c;
  CheckFusedSum(MakeLargeFixed(), c);
}

// ── FastPFor fused (corrected decode path) — parameterized on useGlobalB ─────

class FastPForFusedCorrectedTest : public ::testing::TestWithParam<bool> {
 protected:
  static constexpr size_t kSmall  = 256;
  static constexpr size_t kMedium = 1024;
};

TEST_P(FastPForFusedCorrectedTest, Zeros) {
  FastPForFusedCorrectedCodecU16 c(GetParam());
  CheckFusedSum(MakeZeros(kSmall), c);
  CheckFusedSum(MakeZeros(kMedium), c);
}

TEST_P(FastPForFusedCorrectedTest, Constant) {
  FastPForFusedCorrectedCodecU16 c(GetParam());
  CheckFusedSum(MakeConstant(kSmall,  1), c);
  CheckFusedSum(MakeConstant(kSmall,  65535), c);
  CheckFusedSum(MakeConstant(kMedium, 100), c);
}

TEST_P(FastPForFusedCorrectedTest, Sequential) {
  FastPForFusedCorrectedCodecU16 c(GetParam());
  CheckFusedSum(MakeSequential(kSmall), c);
  CheckFusedSum(MakeSequential(kMedium), c);
}

TEST_P(FastPForFusedCorrectedTest, Random) {
  FastPForFusedCorrectedCodecU16 c(GetParam());
  CheckFusedSum(MakeRandom(kSmall,  42), c);
  CheckFusedSum(MakeRandom(kMedium, 99), c);
}

TEST_P(FastPForFusedCorrectedTest, FixedBlock) {
  FastPForFusedCorrectedCodecU16 c(GetParam());
  CheckFusedSum(MakeLargeFixed(), c);
}

INSTANTIATE_TEST_SUITE_P(
    GlobalAndAdaptiveB, FastPForFusedCorrectedTest,
    ::testing::Values(true, false),
    [](const ::testing::TestParamInfo<bool>& info) {
      return info.param ? "GlobalB" : "AdaptiveB";
    });

// ── SimdComp fused delta-local ────────────────────────────────────────────────

TEST_F(FusedSumTest, SimdCompFusedDeltaLocal_Zeros) {
  SimdCompFusedDeltaLocalCodecU16 c;
  CheckFusedSum(MakeZeros(kSmall), c);
  CheckFusedSum(MakeZeros(kMedium), c);
}

TEST_F(FusedSumTest, SimdCompFusedDeltaLocal_Constant) {
  SimdCompFusedDeltaLocalCodecU16 c;
  CheckFusedSum(MakeConstant(kSmall, 1), c);
  CheckFusedSum(MakeConstant(kSmall, 65535), c);
  CheckFusedSum(MakeConstant(kMedium, 100), c);
}

TEST_F(FusedSumTest, SimdCompFusedDeltaLocal_Sequential) {
  SimdCompFusedDeltaLocalCodecU16 c;
  CheckFusedSum(MakeSequential(kSmall), c);
  CheckFusedSum(MakeSequential(kMedium), c);
}

TEST_F(FusedSumTest, SimdCompFusedDeltaLocal_Random) {
  SimdCompFusedDeltaLocalCodecU16 c;
  CheckFusedSum(MakeRandom(kSmall, 42), c);
  CheckFusedSum(MakeRandom(kMedium, 99), c);
}

TEST_F(FusedSumTest, SimdCompFusedDeltaLocal_FixedBlock) {
  SimdCompFusedDeltaLocalCodecU16 c;
  CheckFusedSum(MakeLargeFixed(), c);
}

// ── SimdComp fused delta-carry ────────────────────────────────────────────────

TEST_F(FusedSumTest, SimdCompFusedDeltaCarry_Zeros) {
  SimdCompFusedDeltaCarryCodecU16 c;
  CheckFusedSum(MakeZeros(kSmall), c);
  CheckFusedSum(MakeZeros(kMedium), c);
}

TEST_F(FusedSumTest, SimdCompFusedDeltaCarry_Constant) {
  SimdCompFusedDeltaCarryCodecU16 c;
  CheckFusedSum(MakeConstant(kSmall, 1), c);
  CheckFusedSum(MakeConstant(kSmall, 65535), c);
  CheckFusedSum(MakeConstant(kMedium, 100), c);
}

TEST_F(FusedSumTest, SimdCompFusedDeltaCarry_Sequential) {
  SimdCompFusedDeltaCarryCodecU16 c;
  CheckFusedSum(MakeSequential(kSmall), c);
  CheckFusedSum(MakeSequential(kMedium), c);
}

TEST_F(FusedSumTest, SimdCompFusedDeltaCarry_Random) {
  SimdCompFusedDeltaCarryCodecU16 c;
  CheckFusedSum(MakeRandom(kSmall, 42), c);
  CheckFusedSum(MakeRandom(kMedium, 99), c);
}

TEST_F(FusedSumTest, SimdCompFusedDeltaCarry_FixedBlock) {
  SimdCompFusedDeltaCarryCodecU16 c;
  CheckFusedSum(MakeLargeFixed(), c);
}

// ── SimdComp fused FoR-global ─────────────────────────────────────────────────

TEST_F(FusedSumTest, SimdCompFusedForGlobal_Zeros) {
  SimdCompFusedForGlobalCodecU16 c;
  CheckFusedSum(MakeZeros(kSmall), c);
  CheckFusedSum(MakeZeros(kMedium), c);
}

TEST_F(FusedSumTest, SimdCompFusedForGlobal_Constant) {
  SimdCompFusedForGlobalCodecU16 c;
  CheckFusedSum(MakeConstant(kSmall, 1), c);
  CheckFusedSum(MakeConstant(kSmall, 65535), c);
  CheckFusedSum(MakeConstant(kMedium, 100), c);
}

TEST_F(FusedSumTest, SimdCompFusedForGlobal_Sequential) {
  SimdCompFusedForGlobalCodecU16 c;
  CheckFusedSum(MakeSequential(kSmall), c);
  CheckFusedSum(MakeSequential(kMedium), c);
}

TEST_F(FusedSumTest, SimdCompFusedForGlobal_Random) {
  SimdCompFusedForGlobalCodecU16 c;
  CheckFusedSum(MakeRandom(kSmall, 42), c);
  CheckFusedSum(MakeRandom(kMedium, 99), c);
}

TEST_F(FusedSumTest, SimdCompFusedForGlobal_FixedBlock) {
  SimdCompFusedForGlobalCodecU16 c;
  CheckFusedSum(MakeLargeFixed(), c);
}

// ── SimdComp fused FoR-local ──────────────────────────────────────────────────

TEST_F(FusedSumTest, SimdCompFusedForLocal_Zeros) {
  SimdCompFusedForLocalCodecU16 c;
  CheckFusedSum(MakeZeros(kSmall), c);
  CheckFusedSum(MakeZeros(kMedium), c);
}

TEST_F(FusedSumTest, SimdCompFusedForLocal_Constant) {
  SimdCompFusedForLocalCodecU16 c;
  CheckFusedSum(MakeConstant(kSmall, 1), c);
  CheckFusedSum(MakeConstant(kSmall, 65535), c);
  CheckFusedSum(MakeConstant(kMedium, 100), c);
}

TEST_F(FusedSumTest, SimdCompFusedForLocal_Sequential) {
  SimdCompFusedForLocalCodecU16 c;
  CheckFusedSum(MakeSequential(kSmall), c);
  CheckFusedSum(MakeSequential(kMedium), c);
}

TEST_F(FusedSumTest, SimdCompFusedForLocal_Random) {
  SimdCompFusedForLocalCodecU16 c;
  CheckFusedSum(MakeRandom(kSmall, 42), c);
  CheckFusedSum(MakeRandom(kMedium, 99), c);
}

TEST_F(FusedSumTest, SimdCompFusedForLocal_Sawtooth) {
  SimdCompFusedForLocalCodecU16 c;
  CheckFusedSum(MakeSawtooth16(kSmall), c);
  CheckFusedSum(MakeSawtooth16(kMedium), c);
}

TEST_F(FusedSumTest, SimdCompFusedForLocal_FixedBlock) {
  SimdCompFusedForLocalCodecU16 c;
  CheckFusedSum(MakeLargeFixed(), c);
}

// ── SimdComp fused FoR-hierarchical ───────────────────────────────────────────

// TEST_F(FusedSumTest, SimdCompFusedForHierarchical_Zeros) {
//   SimdCompFusedForHierarchicalCodecU16 c;
//   CheckFusedSum(MakeZeros(kSmall), c);
//   CheckFusedSum(MakeZeros(kMedium), c);
// }

// TEST_F(FusedSumTest, SimdCompFusedForHierarchical_Constant) {
//   SimdCompFusedForHierarchicalCodecU16 c;
//   CheckFusedSum(MakeConstant(kSmall, 1), c);
//   CheckFusedSum(MakeConstant(kSmall, 65535), c);
//   CheckFusedSum(MakeConstant(kMedium, 100), c);
// }

// TEST_F(FusedSumTest, SimdCompFusedForHierarchical_Sequential) {
//   SimdCompFusedForHierarchicalCodecU16 c;
//   CheckFusedSum(MakeSequential(kSmall), c);
//   CheckFusedSum(MakeSequential(kMedium), c);
// }

// TEST_F(FusedSumTest, SimdCompFusedForHierarchical_Random) {
//   SimdCompFusedForHierarchicalCodecU16 c;
//   CheckFusedSum(MakeRandom(kSmall, 42), c);
//   CheckFusedSum(MakeRandom(kMedium, 99), c);
// }

// TEST_F(FusedSumTest, SimdCompFusedForHierarchical_Sawtooth) {
//   SimdCompFusedForHierarchicalCodecU16 c;
//   CheckFusedSum(MakeSawtooth16(kSmall), c);
//   CheckFusedSum(MakeSawtooth16(kMedium), c);
// }

// TEST_F(FusedSumTest, SimdCompFusedForHierarchical_FixedBlock) {
//   SimdCompFusedForHierarchicalCodecU16 c;
//   CheckFusedSum(MakeLargeFixed(), c);
// }

// // ── FastPFor fused corrected delta-local ──────────────────────────────────────

// TEST_F(FusedSumTest, FastPForFusedCorrectedDeltaLocal_Zeros) {
//   FastPForFusedCorrectedDeltaLocalCodecU16 c;
//   CheckFusedSum(MakeZeros(kSmall), c);
//   CheckFusedSum(MakeZeros(kMedium), c);
// }

TEST_F(FusedSumTest, FastPForFusedCorrectedDeltaLocal_Constant) {
  FastPForFusedCorrectedDeltaLocalCodecU16 c;
  CheckFusedSum(MakeConstant(kSmall, 1), c);
  CheckFusedSum(MakeConstant(kSmall, 65535), c);
  CheckFusedSum(MakeConstant(kMedium, 100), c);
}

TEST_F(FusedSumTest, FastPForFusedCorrectedDeltaLocal_Sequential) {
  FastPForFusedCorrectedDeltaLocalCodecU16 c;
  CheckFusedSum(MakeSequential(kSmall), c);
  CheckFusedSum(MakeSequential(kMedium), c);
}

TEST_F(FusedSumTest, FastPForFusedCorrectedDeltaLocal_Random) {
  FastPForFusedCorrectedDeltaLocalCodecU16 c;
  CheckFusedSum(MakeRandom(kSmall, 42), c);
  CheckFusedSum(MakeRandom(kMedium, 99), c);
}

TEST_F(FusedSumTest, FastPForFusedCorrectedDeltaLocal_FixedBlock) {
  FastPForFusedCorrectedDeltaLocalCodecU16 c;
  CheckFusedSum(MakeLargeFixed(), c);
}

// ── FastPFor fused corrected delta-carry ──────────────────────────────────────

TEST_F(FusedSumTest, FastPForFusedCorrectedDeltaCarry_Zeros) {
  FastPForFusedCorrectedDeltaCarryCodecU16 c;
  CheckFusedSum(MakeZeros(kSmall), c);
  CheckFusedSum(MakeZeros(kMedium), c);
}

TEST_F(FusedSumTest, FastPForFusedCorrectedDeltaCarry_Constant) {
  FastPForFusedCorrectedDeltaCarryCodecU16 c;
  CheckFusedSum(MakeConstant(kSmall, 1), c);
  CheckFusedSum(MakeConstant(kSmall, 65535), c);
  CheckFusedSum(MakeConstant(kMedium, 100), c);
}

TEST_F(FusedSumTest, FastPForFusedCorrectedDeltaCarry_Sequential) {
  FastPForFusedCorrectedDeltaCarryCodecU16 c;
  CheckFusedSum(MakeSequential(kSmall), c);
  CheckFusedSum(MakeSequential(kMedium), c);
}

TEST_F(FusedSumTest, FastPForFusedCorrectedDeltaCarry_Random) {
  FastPForFusedCorrectedDeltaCarryCodecU16 c;
  CheckFusedSum(MakeRandom(kSmall, 42), c);
  CheckFusedSum(MakeRandom(kMedium, 99), c);
}

TEST_F(FusedSumTest, FastPForFusedCorrectedDeltaCarry_FixedBlock) {
  FastPForFusedCorrectedDeltaCarryCodecU16 c;
  CheckFusedSum(MakeLargeFixed(), c);
}

// ── FastPFor FoR-global — parameterized on useGlobalB ────────────────────────

class FastPForFusedForGlobalTest : public ::testing::TestWithParam<bool> {
 protected:
  static constexpr size_t kSmall  = 256;
  static constexpr size_t kMedium = 1024;
};

TEST_P(FastPForFusedForGlobalTest, Zeros) {
  FastPForFusedCorrectedForGlobalCodecU16 c(GetParam());
  CheckFusedSum(MakeZeros(kSmall), c);
  CheckFusedSum(MakeZeros(kMedium), c);
}

TEST_P(FastPForFusedForGlobalTest, Constant) {
  FastPForFusedCorrectedForGlobalCodecU16 c(GetParam());
  CheckFusedSum(MakeConstant(kSmall, 1), c);
  CheckFusedSum(MakeConstant(kSmall, 65535), c);
  CheckFusedSum(MakeConstant(kMedium, 100), c);
}

TEST_P(FastPForFusedForGlobalTest, Sequential) {
  FastPForFusedCorrectedForGlobalCodecU16 c(GetParam());
  CheckFusedSum(MakeSequential(kSmall), c);
  CheckFusedSum(MakeSequential(kMedium), c);
}

TEST_P(FastPForFusedForGlobalTest, Random) {
  FastPForFusedCorrectedForGlobalCodecU16 c(GetParam());
  CheckFusedSum(MakeRandom(kSmall, 42), c);
  CheckFusedSum(MakeRandom(kMedium, 99), c);
}

TEST_P(FastPForFusedForGlobalTest, FixedBlock) {
  FastPForFusedCorrectedForGlobalCodecU16 c(GetParam());
  CheckFusedSum(MakeLargeFixed(), c);
}

INSTANTIATE_TEST_SUITE_P(
    GlobalAndAdaptiveB, FastPForFusedForGlobalTest,
    ::testing::Values(true, false),
    [](const ::testing::TestParamInfo<bool>& info) {
      return info.param ? "GlobalB" : "AdaptiveB";
    });

// ── Compression-ratio tests ───────────────────────────────────────────────────
//
// Verify that the delta variants actually achieve better compression than the
// non-delta baseline on data that clearly favours delta coding.
//
// simdcomp uses maxbits of the transformed stream, so:
//   • CARRY on [0,1,...,n-1]: transform = [zigzag(0), zigzag(1), zigzag(1), ...]
//     = [0, 2, 2, ...] → maxbits=2  (non-delta: max=n-1 → 10 bits for n=1024)
//   • LOCAL on sawtooth [0..15]: every anchor sees in[16k]=0, delta=0, zigzag=0;
//     intra-window delta=1, zigzag=2 → maxbits=2  (non-delta: max=15 → 4 bits)
//
// PFor is exception-based so [0,1,...,n-1] works for LOCAL too:
// ~64 anchor exceptions vs 960 values of zigzag=2 << 10-bit non-delta.

static void CheckCompressionRatio(const std::vector<uint16_t>& data,
                                  StatefulIntegerCodec<uint16_t>& base,
                                  StatefulIntegerCodec<uint16_t>& delta) {
  base.clear();
  base.AllocEncoded(data.data(), data.size());
  base.EncodeArray(data.data(), data.size());
  const size_t base_bytes = base.EncodedNumValues() * base.EncodedSizeValue();
  base.clear();

  delta.clear();
  delta.AllocEncoded(data.data(), data.size());
  delta.EncodeArray(data.data(), data.size());
  const size_t delta_bytes = delta.EncodedNumValues() * delta.EncodedSizeValue();
  delta.clear();

  const size_t uncompressed_bytes = data.size() * sizeof(uint16_t);
  std::printf("  %-52s  %5zu B  (%.2f bpv)\n",
              base.name().c_str(), base_bytes,
              8.0 * base_bytes / data.size());
  std::printf("  %-52s  %5zu B  (%.2f bpv)  ratio vs base: %.2fx\n",
              delta.name().c_str(), delta_bytes,
              8.0 * delta_bytes / data.size(),
              static_cast<double>(base_bytes) / delta_bytes);
  std::printf("  uncompressed: %zu B  (%.2f bpv)\n\n",
              uncompressed_bytes, 8.0 * uncompressed_bytes / data.size());

  EXPECT_LT(delta_bytes, base_bytes)
      << delta.name() << " should compress " << data.size()
      << " elements to fewer bytes than " << base.name()
      << " (delta=" << delta_bytes << " B  base=" << base_bytes << " B)";
}

class CompressionRatioTest : public ::testing::Test {
 protected:
  static constexpr size_t kN = 1024;
};

// simdcomp CARRY: sequential [0..1023], delta→maxbits=2 vs base→maxbits=10.
TEST_F(CompressionRatioTest, SimdCompCarry_BetterThanBase_Sequential) {
  SimdCompFusedCodecU16 base;
  SimdCompFusedDeltaCarryCodecU16 delta;
  CheckCompressionRatio(MakeSequential(kN), base, delta);
}

// simdcomp LOCAL: sawtooth [0..15], delta→maxbits=2 vs base→maxbits=4.
TEST_F(CompressionRatioTest, SimdCompLocal_BetterThanBase_Sawtooth16) {
  SimdCompFusedCodecU16 base;
  SimdCompFusedDeltaLocalCodecU16 delta;
  CheckCompressionRatio(MakeSawtooth16(kN), base, delta);
}

// PFor CARRY: sequential [0..1023], delta→2-bit base, 0 exceptions.
TEST_F(CompressionRatioTest, PForCarry_BetterThanBase_Sequential) {
  FastPForFusedCorrectedCodecU16 base;
  FastPForFusedCorrectedDeltaCarryCodecU16 delta;
  CheckCompressionRatio(MakeSequential(kN), base, delta);
}

// PFor LOCAL: sawtooth [0..15] repeated.
// Non-delta: 4-bit base, 0 exceptions.  LOCAL: 2-bit base, 0 exceptions → 2x better.
// (Sequential data does NOT work for LOCAL: anchor deltas grow to zigzag(1008)=2016
//  and PFor's exception overhead for 64 non-uniform anchors exceeds the 2-bit base
//  savings. LOCAL is designed for windowed/banded data, not globally sequential.)
TEST_F(CompressionRatioTest, PForLocal_BetterThanBase_Sawtooth16) {
  FastPForFusedCorrectedCodecU16 base;
  FastPForFusedCorrectedDeltaLocalCodecU16 delta;
  CheckCompressionRatio(MakeSawtooth16(kN), base, delta);
}

// ── FoR vs base: high-floor data ──────────────────────────────────────────────
//
// FoR's win condition: data has a meaningful non-zero floor (the min). The
// base codec must encode the floor in every value's bit width; FoR subtracts
// it once. We use a constant-base-plus-small-noise pattern so the min is
// significantly above 0.

// Constant offset of 30000 + small variation in [0,15]. Base needs b=15 to
// encode the high-magnitude value; FoR-global subtracts the per-sub-block min
// (≈30000) and only needs b=4 for the residual.
// static std::vector<uint16_t> MakeHighFloor(size_t n) {
//   std::vector<uint16_t> v(n);
//   std::mt19937 gen(7);
//   std::uniform_int_distribution<uint16_t> dist(0, 15);
//   for (size_t i = 0; i < n; ++i)
//     v[i] = static_cast<uint16_t>(30000) + dist(gen);
//   return v;
// }

// TEST_F(CompressionRatioTest, SimdCompForGlobal_BetterThanBase_HighFloor) {
//   SimdCompFusedCodecU16 base;
//   SimdCompFusedForGlobalCodecU16 forc;
//   CheckCompressionRatio(MakeHighFloor(kN), base, forc);
// }

// TEST_F(CompressionRatioTest, SimdCompForLocal_BetterThanBase_HighFloor) {
//   SimdCompFusedCodecU16 base;
//   SimdCompFusedForLocalCodecU16 forc;
//   CheckCompressionRatio(MakeHighFloor(kN), base, forc);
// }

// TEST_F(CompressionRatioTest, SimdCompForHierarchical_BetterThanBase_HighFloor) {
//   SimdCompFusedCodecU16 base;
//   SimdCompFusedForHierarchicalCodecU16 forc;
//   CheckCompressionRatio(MakeHighFloor(kN), base, forc);
// }

// // FoR-local should also beat base on tile-banded data — each OutReg's anchor
// // is its own min, so per-OutReg range is tiny.  16-element sawtooth has the
// // exact OutReg-period structure that FoR-local was designed for.
// TEST_F(CompressionRatioTest, SimdCompForLocal_BetterThanBase_Sawtooth16) {
//   SimdCompFusedCodecU16 base;
//   SimdCompFusedForLocalCodecU16 forc;
//   CheckCompressionRatio(MakeSawtooth16(kN), base, forc);
// }
