#include <cstdint>
#include <memory>
#include <numeric>
#include <random>
#include <vector>

#include <gtest/gtest.h>

#include "simdcomp_fused_codec_uint16.h"
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

// ── FastPFor fused (corrected decode path) ────────────────────────────────────

TEST_F(FusedSumTest, FastPForFusedCorrected_Zeros) {
  FastPForFusedCorrectedCodecU16 c;
  CheckFusedSum(MakeZeros(kSmall), c);
  CheckFusedSum(MakeZeros(kMedium), c);
}

TEST_F(FusedSumTest, FastPForFusedCorrected_Constant) {
  FastPForFusedCorrectedCodecU16 c;
  CheckFusedSum(MakeConstant(kSmall,  1), c);
  CheckFusedSum(MakeConstant(kSmall,  65535), c);
  CheckFusedSum(MakeConstant(kMedium, 100), c);
}

TEST_F(FusedSumTest, FastPForFusedCorrected_Sequential) {
  FastPForFusedCorrectedCodecU16 c;
  CheckFusedSum(MakeSequential(kSmall), c);
  CheckFusedSum(MakeSequential(kMedium), c);
}

TEST_F(FusedSumTest, FastPForFusedCorrected_Random) {
  FastPForFusedCorrectedCodecU16 c;
  CheckFusedSum(MakeRandom(kSmall,  42), c);
  CheckFusedSum(MakeRandom(kMedium, 99), c);
}

TEST_F(FusedSumTest, FastPForFusedCorrected_FixedBlock) {
  FastPForFusedCorrectedCodecU16 c;
  CheckFusedSum(MakeLargeFixed(), c);
}
