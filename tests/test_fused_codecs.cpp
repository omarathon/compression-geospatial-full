#include <cmath>
#include <cstdint>
#include <cstdio>
#include <memory>
#include <numeric>
#include <random>
#include <vector>

#include <gtest/gtest.h>

#include "simdcomp_fused_codec_uint16.h"
#include "simdcomp_fused_extract_codec_uint16.h"
#include "simdcomp_for_codec_uint16.h"
#include "fastpfor_fused_codec_uint16.h"
#include "turbopfor_fused_codec_uint16.h"
#include "turbopfor_fused_256_codec_uint16.h"
#include "simdcomp_fused_codec_uint16_w128.h"
#include "simdcomp_for_codec_uint16_w128.h"
#include "simdcomp_for_codec_uint16_w256.h"
#include "turbopfor_for_codec_uint16.h"
#include "custom_unvec_logic_codecs_u16.h"

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

// Spiky: bulk in [0, bulk_max] with a sparse fraction of large outliers.
static std::vector<uint16_t> MakeSpiky(size_t n, uint32_t seed = 7,
                                       double spike_frac = 0.05,
                                       uint16_t bulk_max = 3,
                                       uint16_t spike_max = 12000) {
  std::mt19937 gen(seed);
  std::uniform_int_distribution<uint16_t> bulk(0, bulk_max);
  std::uniform_int_distribution<uint16_t> spike(
      static_cast<uint16_t>(bulk_max + 1), spike_max);
  std::bernoulli_distribution is_spike(spike_frac);
  std::vector<uint16_t> v(n);
  for (auto& x : v) x = is_spike(gen) ? spike(gen) : bulk(gen);
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

// ── SimdComp fused EXTRACT (per-OutReg generic-b decode) ─────────────────────
// Must produce byte-exact sums vs the reference across the full b range:
// zeros (b=0), constant 65535 (b=16), sequential/random (mid/high b), spiky.
TEST_F(FusedSumTest, SimdCompFusedExtract_AllPatterns) {
  SimdCompFusedExtractCodecU16 c;
  CheckFusedSum(MakeZeros(kSmall), c);
  CheckFusedSum(MakeZeros(kMedium), c);
  CheckFusedSum(MakeConstant(kSmall,  1), c);       // b=1
  CheckFusedSum(MakeConstant(kSmall,  255), c);     // b=8
  CheckFusedSum(MakeConstant(kSmall,  65535), c);   // b=16
  CheckFusedSum(MakeConstant(kMedium, 100), c);
  CheckFusedSum(MakeSequential(kSmall), c);
  CheckFusedSum(MakeSequential(kMedium), c);
  CheckFusedSum(MakeRandom(kSmall,  42), c);
  CheckFusedSum(MakeRandom(kMedium, 99), c);
  CheckFusedSum(MakeSpiky(kMedium), c);
  CheckFusedSum(MakeLargeFixed(), c);
}

TEST_F(FusedSumTest, SimdCompFusedExtractImm_AllPatterns) {
  SimdCompFusedExtractImmCodecU16 c;
  CheckFusedSum(MakeZeros(kSmall), c);
  CheckFusedSum(MakeConstant(kSmall, 1), c);
  CheckFusedSum(MakeConstant(kSmall, 255), c);
  CheckFusedSum(MakeConstant(kSmall, 65535), c);
  CheckFusedSum(MakeSequential(kMedium), c);
  CheckFusedSum(MakeRandom(kSmall, 42), c);
  CheckFusedSum(MakeRandom(kMedium, 99), c);
  CheckFusedSum(MakeSpiky(kMedium), c);
  CheckFusedSum(MakeLargeFixed(), c);
}

TEST_F(FusedSumTest, SimdCompFusedL1Temp_AllPatterns) {
  SimdCompFusedL1TempCodecU16 c;
  CheckFusedSum(MakeZeros(kSmall), c);
  CheckFusedSum(MakeConstant(kSmall, 1), c);
  CheckFusedSum(MakeConstant(kSmall, 65535), c);
  CheckFusedSum(MakeSequential(kMedium), c);
  CheckFusedSum(MakeRandom(kSmall, 42), c);
  CheckFusedSum(MakeRandom(kMedium, 99), c);
  CheckFusedSum(MakeSpiky(kMedium), c);
  CheckFusedSum(MakeLargeFixed(), c);
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

// ── TurboPFor fused sum (128v16) — oracle is the true input sum ───────────────
// Prototype requires multiples of 128. MakeSpiky exercises the exception merge;
// MakeZeros/MakeConstant exercise the all-equal RLE branch.
TEST_F(FusedSumTest, TurboPForFused128_Sum) {
  TurboPForFusedCodecU16 c;
  CheckFusedSum(MakeZeros(256), c);
  CheckFusedSum(MakeConstant(256, 7), c);
  CheckFusedSum(MakeConstant(1024, 12345), c);
  CheckFusedSum(MakeSequential(256), c);
  CheckFusedSum(MakeSequential(1024), c);
  CheckFusedSum(MakeRandom(256, 42), c);
  CheckFusedSum(MakeRandom(1024, 99), c);
  CheckFusedSum(MakeSpiky(65536, 7, 0.05), c);
}

TEST_F(FusedSumTest, SimdCompFused128_Sum) {
  SimdCompFusedCodecU16_128 c;
  CheckFusedSum(MakeZeros(256), c);
  CheckFusedSum(MakeConstant(256, 7), c);
  CheckFusedSum(MakeConstant(1024, 12345), c);
  CheckFusedSum(MakeSequential(256), c);
  CheckFusedSum(MakeSequential(1024), c);
  CheckFusedSum(MakeRandom(256, 42), c);
  CheckFusedSum(MakeRandom(1024, 99), c);
  CheckFusedSum(MakeSpiky(65536, 7, 0.05), c);
}

// ── 128-bit fused FoR (regular + hierarchical) — fused sum round-trip ─────────
// Every window × sep, plus hierarchical outer×inner, over patterns that stress
// the FoR path (DEM-like locally-varying baseline, spikes, constants, zeros).
TEST_F(FusedSumTest, SimdCompFusedFor128_Regular_Sum) {
  const std::vector<std::vector<uint16_t>> data = {
      MakeZeros(65536),          MakeConstant(256, 7),
      MakeConstant(1024, 12345), MakeSequential(1024),
      MakeRandom(65536, 42),     MakeSpiky(65536, 7, 0.05),
  };
  for (size_t w : {4u, 8u, 16u, 32u, 64u, 128u, 256u}) {
    for (bool sep : {false, true}) {
      for (bool nobc : {false, true}) {
        SimdCompFusedForCodecU16_128 c(w, sep, FusedAggImpl::kMadd, /*shuf=*/false, nobc);
        for (const auto& d : data)
          if (d.size() % w == 0) CheckFusedSum(d, c);
      }
    }
  }
}

TEST_F(FusedSumTest, SimdCompFusedFor128_Hierarchical_Sum) {
  const std::vector<std::vector<uint16_t>> data = {
      MakeZeros(65536),       MakeConstant(1024, 12345),
      MakeSequential(65536),  MakeRandom(65536, 99),
      MakeSpiky(65536, 7, 0.05),
  };
  for (size_t gw : {128u, 256u}) {
    for (size_t lw : {4u, 8u, 16u, 32u, 64u, 128u, 256u}) {
      if (lw > gw || gw % lw) continue;
      SimdCompFusedForHierarchicalCodecU16_128 c(gw, lw);
      for (const auto& d : data)
        if (d.size() % gw == 0) CheckFusedSum(d, c);
    }
  }
}

// 256-bit counterparts. Exercise both aggregate impls (kUnpack always; kMadd
// falls back to kUnpack on the MakeRandom case where values reach 2^15).
TEST_F(FusedSumTest, SimdCompFusedFor256_Regular_Sum) {
  const std::vector<std::vector<uint16_t>> data = {
      MakeZeros(65536),          MakeConstant(256, 7),
      MakeConstant(1024, 12345), MakeSequential(1024),
      MakeRandom(65536, 42),     MakeSpiky(65536, 7, 0.05),
  };
  for (FusedAggImpl agg : {FusedAggImpl::kUnpack, FusedAggImpl::kMadd}) {
    for (size_t w : {4u, 8u, 16u, 32u, 64u, 128u, 256u}) {
      for (bool sep : {false, true}) {
        for (bool shuf : {false, true}) {
          SimdCompFusedForCodecU16_256 c(w, sep, agg, shuf);
          for (const auto& d : data)
            if (d.size() % w == 0) CheckFusedSum(d, c);
        }
        SimdCompFusedForCodecU16_256 cn(w, sep, agg, /*shuf=*/false, /*nobc=*/true);
        for (const auto& d : data)
          if (d.size() % w == 0) CheckFusedSum(d, cn);
      }
    }
  }
}

TEST_F(FusedSumTest, SimdCompFusedFor256_Hierarchical_Sum) {
  const std::vector<std::vector<uint16_t>> data = {
      MakeZeros(65536),       MakeConstant(1024, 12345),
      MakeSequential(65536),  MakeRandom(65536, 99),
      MakeSpiky(65536, 7, 0.05),
  };
  for (FusedAggImpl agg : {FusedAggImpl::kUnpack, FusedAggImpl::kMadd}) {
    for (size_t gw : {128u, 256u}) {
      for (size_t lw : {4u, 8u, 16u, 32u, 64u, 128u, 256u}) {
        if (lw > gw || gw % lw) continue;
        SimdCompFusedForHierarchicalCodecU16_256 c(gw, lw, agg);
        for (const auto& d : data)
          if (d.size() % gw == 0) CheckFusedSum(d, c);
      }
    }
  }
}

// TurboPFor FoR-fused (PFor residuals + per-window anchor). Spiky data exercises
// the exception path; the others hit PLAIN / b==0 corrected paths.
TEST_F(FusedSumTest, TurboPForFusedFor256_Regular_Sum) {
  const std::vector<std::vector<uint16_t>> data = {
      MakeZeros(65536),          MakeConstant(256, 7),
      MakeConstant(1024, 12345), MakeSequential(1024),
      MakeRandom(65536, 42),     MakeSpiky(65536, 7, 0.05),
  };
  for (FusedAggImpl agg : {FusedAggImpl::kUnpack, FusedAggImpl::kMadd}) {
    for (bool nobc : {false, true}) {
      for (size_t w : {4u, 8u, 16u, 32u, 64u, 128u, 256u}) {
        for (bool sep : {false, true}) {
          TurboPForFusedForCodecU16 c(w, sep, agg, nobc);
          for (const auto& d : data)
            if (d.size() % w == 0) CheckFusedSum(d, c);
        }
      }
    }
  }
}

TEST_F(FusedSumTest, TurboPForFusedFor256_Hierarchical_Sum) {
  const std::vector<std::vector<uint16_t>> data = {
      MakeZeros(65536),       MakeConstant(1024, 12345),
      MakeSequential(65536),  MakeRandom(65536, 99),
      MakeSpiky(65536, 7, 0.05),
  };
  for (FusedAggImpl agg : {FusedAggImpl::kUnpack, FusedAggImpl::kMadd}) {
    for (bool nobc : {false, true}) {
      for (size_t gw : {128u, 256u}) {
        for (size_t lw : {4u, 8u, 16u, 32u, 64u, 128u, 256u}) {
          if (lw > gw || gw % lw) continue;
          TurboPForFusedForHierarchicalCodecU16 c(gw, lw, agg, nobc);
          for (const auto& d : data)
            if (d.size() % gw == 0) CheckFusedSum(d, c);
        }
      }
    }
  }
}

TEST_F(FusedSumTest, TurboPForFused256_Sum) {
  TurboPForFused256CodecU16 c;
  CheckFusedSum(MakeZeros(256), c);
  CheckFusedSum(MakeConstant(256, 7), c);
  CheckFusedSum(MakeConstant(1024, 12345), c);
  CheckFusedSum(MakeSequential(256), c);
  CheckFusedSum(MakeSequential(1024), c);
  CheckFusedSum(MakeRandom(256, 42), c);
  CheckFusedSum(MakeRandom(1024, 99), c);
  CheckFusedSum(MakeSpiky(65536, 7, 0.05), c);
}

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

// ── SimdComp fused FoR-global (sub-block window sizes) ───────────────────────

class SimdCompFusedForGlobalWinTest
    : public ::testing::TestWithParam<size_t> {
 protected:
  static constexpr size_t kSmall  = 256;
  static constexpr size_t kMedium = 1024;
};

TEST_P(SimdCompFusedForGlobalWinTest, Zeros) {
  SimdCompFusedForGlobalCodecU16 c(GetParam());
  CheckFusedSum(MakeZeros(kSmall), c);
  CheckFusedSum(MakeZeros(kMedium), c);
}

TEST_P(SimdCompFusedForGlobalWinTest, Constant) {
  SimdCompFusedForGlobalCodecU16 c(GetParam());
  CheckFusedSum(MakeConstant(kSmall, 1), c);
  CheckFusedSum(MakeConstant(kSmall, 65535), c);
  CheckFusedSum(MakeConstant(kMedium, 100), c);
}

TEST_P(SimdCompFusedForGlobalWinTest, Sequential) {
  SimdCompFusedForGlobalCodecU16 c(GetParam());
  CheckFusedSum(MakeSequential(kSmall), c);
  CheckFusedSum(MakeSequential(kMedium), c);
}

TEST_P(SimdCompFusedForGlobalWinTest, Random) {
  SimdCompFusedForGlobalCodecU16 c(GetParam());
  CheckFusedSum(MakeRandom(kSmall, 42), c);
  CheckFusedSum(MakeRandom(kMedium, 99), c);
}

TEST_P(SimdCompFusedForGlobalWinTest, FixedBlock) {
  SimdCompFusedForGlobalCodecU16 c(GetParam());
  CheckFusedSum(MakeLargeFixed(), c);
}

INSTANTIATE_TEST_SUITE_P(
    WindowSizes, SimdCompFusedForGlobalWinTest,
    ::testing::Values(32u, 64u, 128u),
    [](const ::testing::TestParamInfo<size_t>& info) {
      return "w" + std::to_string(info.param);
    });

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

// ── FastPFor FoR-global — parameterized on (useGlobalB, exceptionPenalty) ────

using ForGlobalParam = std::pair<bool, double>;

class FastPForFusedForGlobalTest
    : public ::testing::TestWithParam<ForGlobalParam> {
 protected:
  static constexpr size_t kSmall  = 256;
  static constexpr size_t kMedium = 1024;
};

TEST_P(FastPForFusedForGlobalTest, Zeros) {
  auto [useGlobalB, penalty] = GetParam();
  FastPForFusedCorrectedForGlobalCodecU16 c(useGlobalB, penalty);
  CheckFusedSum(MakeZeros(kSmall), c);
  CheckFusedSum(MakeZeros(kMedium), c);
}

TEST_P(FastPForFusedForGlobalTest, Constant) {
  auto [useGlobalB, penalty] = GetParam();
  FastPForFusedCorrectedForGlobalCodecU16 c(useGlobalB, penalty);
  CheckFusedSum(MakeConstant(kSmall, 1), c);
  CheckFusedSum(MakeConstant(kSmall, 65535), c);
  CheckFusedSum(MakeConstant(kMedium, 100), c);
}

TEST_P(FastPForFusedForGlobalTest, Sequential) {
  auto [useGlobalB, penalty] = GetParam();
  FastPForFusedCorrectedForGlobalCodecU16 c(useGlobalB, penalty);
  CheckFusedSum(MakeSequential(kSmall), c);
  CheckFusedSum(MakeSequential(kMedium), c);
}

TEST_P(FastPForFusedForGlobalTest, Random) {
  auto [useGlobalB, penalty] = GetParam();
  FastPForFusedCorrectedForGlobalCodecU16 c(useGlobalB, penalty);
  CheckFusedSum(MakeRandom(kSmall, 42), c);
  CheckFusedSum(MakeRandom(kMedium, 99), c);
}

TEST_P(FastPForFusedForGlobalTest, FixedBlock) {
  auto [useGlobalB, penalty] = GetParam();
  FastPForFusedCorrectedForGlobalCodecU16 c(useGlobalB, penalty);
  CheckFusedSum(MakeLargeFixed(), c);
}

INSTANTIATE_TEST_SUITE_P(
    Variants, FastPForFusedForGlobalTest,
    ::testing::Values(
        ForGlobalParam{true,  16.0},   // global_b
        ForGlobalParam{false, 16.0},   // adaptive_b p16
        ForGlobalParam{false, 32.0},   // adaptive_b p32
        ForGlobalParam{false, 64.0},    // adaptive_b p64
        ForGlobalParam{false, 128.0},   // adaptive_b p128
        ForGlobalParam{false, 256.0},   // adaptive_b p256
        ForGlobalParam{false, 512.0},   // adaptive_b p512
        ForGlobalParam{false, 1024.0},  // adaptive_b p1024
        ForGlobalParam{false, 2048.0},  // adaptive_b p2048
        ForGlobalParam{false, 4096.0},  // adaptive_b p4096
        ForGlobalParam{false, 8192.0}   // adaptive_b p8192
    ),
    [](const ::testing::TestParamInfo<ForGlobalParam>& info) -> std::string {
      if (info.param.first) return "GlobalB";
      return "AdaptiveB_p" + std::to_string(static_cast<int>(info.param.second));
    });

// ── FastPFor FoR-global (adaptive_b, sub-block window sizes) ─────────────────

class FastPForFusedForGlobalWinTest
    : public ::testing::TestWithParam<size_t> {
 protected:
  static constexpr size_t kSmall  = 256;
  static constexpr size_t kMedium = 1024;
};

TEST_P(FastPForFusedForGlobalWinTest, Zeros) {
  FastPForFusedCorrectedForGlobalCodecU16 c(false, 16.0, GetParam());
  CheckFusedSum(MakeZeros(kSmall), c);
  CheckFusedSum(MakeZeros(kMedium), c);
}

TEST_P(FastPForFusedForGlobalWinTest, Constant) {
  FastPForFusedCorrectedForGlobalCodecU16 c(false, 16.0, GetParam());
  CheckFusedSum(MakeConstant(kSmall, 1), c);
  CheckFusedSum(MakeConstant(kSmall, 65535), c);
  CheckFusedSum(MakeConstant(kMedium, 100), c);
}

TEST_P(FastPForFusedForGlobalWinTest, Sequential) {
  FastPForFusedCorrectedForGlobalCodecU16 c(false, 16.0, GetParam());
  CheckFusedSum(MakeSequential(kSmall), c);
  CheckFusedSum(MakeSequential(kMedium), c);
}

TEST_P(FastPForFusedForGlobalWinTest, Random) {
  FastPForFusedCorrectedForGlobalCodecU16 c(false, 16.0, GetParam());
  CheckFusedSum(MakeRandom(kSmall, 42), c);
  CheckFusedSum(MakeRandom(kMedium, 99), c);
}

TEST_P(FastPForFusedForGlobalWinTest, FixedBlock) {
  FastPForFusedCorrectedForGlobalCodecU16 c(false, 16.0, GetParam());
  CheckFusedSum(MakeLargeFixed(), c);
}

INSTANTIATE_TEST_SUITE_P(
    WindowSizes, FastPForFusedForGlobalWinTest,
    ::testing::Values(32u, 64u, 128u),
    [](const ::testing::TestParamInfo<size_t>& info) {
      return "w" + std::to_string(info.param);
    });

// ── FastPFor corrected (adaptive_b, sub-block window sizes) ─────────────────
// Exercises the flat-format windowed decode and the uint16-packed exception
// stream (Random/kMedium produces exceptions).

class FastPForFusedCorrectedWinTest
    : public ::testing::TestWithParam<size_t> {
 protected:
  static constexpr size_t kSmall  = 256;
  static constexpr size_t kMedium = 1024;
};

TEST_P(FastPForFusedCorrectedWinTest, Zeros) {
  FastPForFusedCorrectedCodecU16 c(false, GetParam());
  CheckFusedSum(MakeZeros(kSmall), c);
  CheckFusedSum(MakeZeros(kMedium), c);
}

TEST_P(FastPForFusedCorrectedWinTest, Constant) {
  FastPForFusedCorrectedCodecU16 c(false, GetParam());
  CheckFusedSum(MakeConstant(kSmall, 1), c);
  CheckFusedSum(MakeConstant(kSmall, 65535), c);
  CheckFusedSum(MakeConstant(kMedium, 100), c);
}

TEST_P(FastPForFusedCorrectedWinTest, Sequential) {
  FastPForFusedCorrectedCodecU16 c(false, GetParam());
  CheckFusedSum(MakeSequential(kSmall), c);
  CheckFusedSum(MakeSequential(kMedium), c);
}

TEST_P(FastPForFusedCorrectedWinTest, Random) {
  FastPForFusedCorrectedCodecU16 c(false, GetParam());
  CheckFusedSum(MakeRandom(kSmall, 42), c);
  CheckFusedSum(MakeRandom(kMedium, 99), c);
}

TEST_P(FastPForFusedCorrectedWinTest, FixedBlock) {
  FastPForFusedCorrectedCodecU16 c(false, GetParam());
  CheckFusedSum(MakeLargeFixed(), c);
}

INSTANTIATE_TEST_SUITE_P(
    WindowSizes, FastPForFusedCorrectedWinTest,
    ::testing::Values(32u, 64u, 128u, 256u),
    [](const ::testing::TestParamInfo<size_t>& info) {
      return "w" + std::to_string(info.param);
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
  const size_t base_bytes = base.EncodedNumValues() * base.EncodedSizeValue()
                           + base.ExtraEncodedBytes();
  base.clear();

  delta.clear();
  delta.AllocEncoded(data.data(), data.size());
  delta.EncodeArray(data.data(), data.size());
  const size_t delta_bytes = delta.EncodedNumValues() * delta.EncodedSizeValue()
                            + delta.ExtraEncodedBytes();
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

// ── FOR round-trip tests (flat + hierarchical, sep=false and sep=true) ────────

static void CheckRoundTrip(const std::vector<uint16_t>& data,
                            StatefulIntegerCodec<uint16_t>& codec) {
  const size_t n = data.size();
  codec.clear();
  codec.AllocEncoded(data.data(), n);
  codec.EncodeArray(data.data(), n);
  std::vector<uint16_t> out(n, 0xDEAD);
  codec.DecodeArray(out.data(), n);
  for (size_t i = 0; i < n; ++i)
    ASSERT_EQ(out[i], data[i]) << codec.name() << " mismatch at i=" << i;
}

// Parameterized on separate_metadata (false = mixed stream, true = sep).
class FORRoundTripTest : public ::testing::TestWithParam<bool> {};

TEST_P(FORRoundTripTest, Flat_Zeros) {
  const bool sep = GetParam();
  for (size_t w : {2u, 4u, 8u, 16u, 32u}) {
    FORCodecU16 c(w, sep);
    CheckRoundTrip(MakeZeros(256), c);
    CheckRoundTrip(MakeZeros(1024), c);
  }
}

TEST_P(FORRoundTripTest, Flat_Constant) {
  const bool sep = GetParam();
  FORCodecU16 c(8, sep);
  CheckRoundTrip(MakeConstant(256, 1), c);
  CheckRoundTrip(MakeConstant(256, 65535), c);
  CheckRoundTrip(MakeConstant(1024, 30000), c);
}

TEST_P(FORRoundTripTest, Flat_Sequential) {
  const bool sep = GetParam();
  FORCodecU16 c(8, sep);
  CheckRoundTrip(MakeSequential(256), c);
  CheckRoundTrip(MakeSequential(1024), c);
}

TEST_P(FORRoundTripTest, Flat_Random) {
  const bool sep = GetParam();
  FORCodecU16 c(8, sep);
  CheckRoundTrip(MakeRandom(256, 42), c);
  CheckRoundTrip(MakeRandom(1024, 99), c);
}

TEST_P(FORRoundTripTest, Hier_Zeros) {
  const bool sep = GetParam();
  for (auto [gw, lw] : std::initializer_list<std::pair<size_t,size_t>>{{256,8},{256,16},{64,8}}) {
    FORHierarchicalCodecU16 c(gw, lw, sep);
    CheckRoundTrip(MakeZeros(256), c);
    CheckRoundTrip(MakeZeros(1024), c);
  }
}

TEST_P(FORRoundTripTest, Hier_Constant) {
  const bool sep = GetParam();
  FORHierarchicalCodecU16 c(256, 8, sep);
  CheckRoundTrip(MakeConstant(256, 1), c);
  CheckRoundTrip(MakeConstant(256, 65535), c);
  CheckRoundTrip(MakeConstant(1024, 30000), c);
}

TEST_P(FORRoundTripTest, Hier_Sequential) {
  const bool sep = GetParam();
  FORHierarchicalCodecU16 c(256, 8, sep);
  CheckRoundTrip(MakeSequential(256), c);
  CheckRoundTrip(MakeSequential(1024), c);
}

TEST_P(FORRoundTripTest, Hier_Random) {
  const bool sep = GetParam();
  FORHierarchicalCodecU16 c(256, 8, sep);
  CheckRoundTrip(MakeRandom(256, 42), c);
  CheckRoundTrip(MakeRandom(1024, 99), c);
}

TEST_P(FORRoundTripTest, Hier_HighFloor) {
  const bool sep = GetParam();
  std::vector<uint16_t> v(1024);
  std::mt19937 gen(7);
  std::uniform_int_distribution<uint16_t> dist(0, 15);
  for (auto& x : v) x = static_cast<uint16_t>(30000 + dist(gen));
  FORHierarchicalCodecU16 c(256, 8, sep);
  CheckRoundTrip(v, c);
}

TEST_P(FORRoundTripTest, Hier_LocalWindow4) {
  const bool sep = GetParam();
  FORHierarchicalCodecU16 c(256, 4, sep);
  CheckRoundTrip(MakeRandom(256, 17), c);
  CheckRoundTrip(MakeRandom(1024, 18), c);
}

TEST_P(FORRoundTripTest, Hier_AllBitWidths) {
  const bool sep = GetParam();
  FORHierarchicalCodecU16 c(256, 8, sep);
  for (uint16_t max_local_delta : {0, 1, 3, 7, 15, 255, 1023, 65535}) {
    std::vector<uint16_t> v(256, 0);
    v[0] = max_local_delta;
    CheckRoundTrip(v, c);
  }
}

INSTANTIATE_TEST_SUITE_P(SepMetadata, FORRoundTripTest,
                         ::testing::Bool(),
                         [](const ::testing::TestParamInfo<bool>& i) {
                           return i.param ? "sep" : "mixed";
                         });

// ── FOR compression-ratio tests (flat vs hierarchical, sep=false and sep=true) ─
//
// FORHierarchicalCodecU16(gw, lw) beats FORCodecU16(lw) on smooth/structured
// data because the local anchor deltas are small (low b_local) and pwords
// << N/lw. Both are tested with sep=false (metadata mixed into the stream the
// physical codec sees) and sep=true (metadata held aside; physical codec sees
// only clean residuals). CR is measured on the logical codec output alone —
// sep=true gives a lower bound on what the residuals compress to before the
// physical codec's own bit-packing.

static std::vector<uint16_t> MakeSineWave(size_t n, uint16_t base = 1000,
                                           uint16_t amplitude = 500) {
  std::vector<uint16_t> v(n);
  for (size_t i = 0; i < n; ++i) {
    double s = std::sin(2.0 * 3.14159265 * i / 256.0);
    v[i] = static_cast<uint16_t>(base + static_cast<uint16_t>(amplitude * (s + 1.0) / 2.0));
  }
  return v;
}

static std::vector<uint16_t> MakeDemLike(size_t n, uint32_t seed = 3) {
  std::mt19937 gen(seed);
  std::uniform_int_distribution<int> noise(-2, 2);
  std::vector<uint16_t> v(n);
  int val = 500;
  for (size_t i = 0; i < n; ++i) {
    val = std::max(0, std::min(65535, val + noise(gen)));
    v[i] = static_cast<uint16_t>(val);
  }
  return v;
}

class FORCRTest : public ::testing::TestWithParam<bool> {
 protected:
  static constexpr size_t kN = 1024;
};

// Constant: b_local=0, zero packed-delta words. hier overhead = 1+num_gw=5
// words vs flat overhead = N/lw=128 words.
TEST_P(FORCRTest, BetterThanFlat_Constant) {
  const bool sep = GetParam();
  FORCodecU16 flat(8, sep);
  FORHierarchicalCodecU16 hier(256, 8, sep);
  CheckCompressionRatio(MakeConstant(kN, 30000), flat, hier);
}

// Linear ramp: max delta within gw=256 is 248 → b_local=8.
// pwords=64 < flat overhead=128.
TEST_P(FORCRTest, BetterThanFlat_LinearRamp) {
  const bool sep = GetParam();
  FORCodecU16 flat(8, sep);
  FORHierarchicalCodecU16 hier(256, 8, sep);
  CheckCompressionRatio(MakeSequential(kN), flat, hier);
}

// Slow sine: one full cycle per gw=256. b_local ≈ 5-6 → pwords << N/lw.
TEST_P(FORCRTest, BetterThanFlat_SineWave) {
  const bool sep = GetParam();
  FORCodecU16 flat(8, sep);
  FORHierarchicalCodecU16 hier(256, 8, sep);
  CheckCompressionRatio(MakeSineWave(kN), flat, hier);
}

// High floor + noise ≤15: b_local=4 → pwords=32 vs flat overhead=128.
TEST_P(FORCRTest, BetterThanFlat_HighFloorNoise) {
  const bool sep = GetParam();
  std::vector<uint16_t> v(kN);
  std::mt19937 gen(7);
  std::uniform_int_distribution<int> noise(0, 15);
  for (auto& x : v) x = static_cast<uint16_t>(30000 + noise(gen));
  FORCodecU16 flat(8, sep);
  FORHierarchicalCodecU16 hier(256, 8, sep);
  CheckCompressionRatio(v, flat, hier);
}

// DEM-like smooth random walk ±2: range within gw ≈ O(sqrt(256)*2) → b_local ≈ 3-4.
TEST_P(FORCRTest, BetterThanFlat_DemLike) {
  const bool sep = GetParam();
  FORCodecU16 flat(8, sep);
  FORHierarchicalCodecU16 hier(256, 8, sep);
  CheckCompressionRatio(MakeDemLike(kN), flat, hier);
}

// Sawtooth period=256: max delta=248 → b_local=8, pwords=64 < flat=128.
TEST_P(FORCRTest, BetterThanFlat_Sawtooth256) {
  const bool sep = GetParam();
  std::vector<uint16_t> v(kN);
  for (size_t i = 0; i < kN; ++i) v[i] = static_cast<uint16_t>(i % 256);
  FORCodecU16 flat(8, sep);
  FORHierarchicalCodecU16 hier(256, 8, sep);
  CheckCompressionRatio(v, flat, hier);
}

INSTANTIATE_TEST_SUITE_P(SepMetadata, FORCRTest,
                         ::testing::Bool(),
                         [](const ::testing::TestParamInfo<bool>& i) {
                           return i.param ? "sep" : "mixed";
                         });
