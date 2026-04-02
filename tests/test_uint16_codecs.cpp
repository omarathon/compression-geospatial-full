#include <memory>
#include <random>
#include <vector>

#include <gtest/gtest.h>

#include "codec_collection_uint16.h"
#include "codec_collection_uint8.h"

// ── Helpers ──────────────────────────────────────────────────────────────────

static bool TestCodecU16(std::vector<uint16_t>& data,
                         StatefulIntegerCodec<uint16_t>& codec) {
  codec.clear();
  try {
    codec.AllocEncoded(data.data(), data.size());
    codec.EncodeArray(data.data(), data.size());
  } catch (const std::exception& e) {
    ADD_FAILURE() << "Encode error in " << codec.name() << ": " << e.what();
    return false;
  }

  std::vector<uint16_t> data_back(data.size() +
                                   codec.GetOverflowSize(data.size()));
  try {
    codec.DecodeArray(data_back.data(), data.size());
  } catch (const std::exception& e) {
    ADD_FAILURE() << "Decode error in " << codec.name() << ": " << e.what();
    return false;
  }

  for (size_t i = 0; i < data.size(); ++i) {
    if (data[i] != data_back[i]) {
      ADD_FAILURE() << "Round-trip mismatch in " << codec.name() << " at i="
                    << i << " expected=" << data[i] << " got=" << data_back[i];
      codec.clear();
      return false;
    }
  }
  codec.clear();
  return true;
}

static bool TestCodecU8(std::vector<uint8_t>& data,
                        StatefulIntegerCodec<uint8_t>& codec) {
  codec.clear();
  try {
    codec.AllocEncoded(data.data(), data.size());
    codec.EncodeArray(data.data(), data.size());
  } catch (const std::exception& e) {
    ADD_FAILURE() << "Encode error in " << codec.name() << ": " << e.what();
    return false;
  }

  std::vector<uint8_t> data_back(data.size() +
                                  codec.GetOverflowSize(data.size()));
  try {
    codec.DecodeArray(data_back.data(), data.size());
  } catch (const std::exception& e) {
    ADD_FAILURE() << "Decode error in " << codec.name() << ": " << e.what();
    return false;
  }

  for (size_t i = 0; i < data.size(); ++i) {
    if (data[i] != data_back[i]) {
      ADD_FAILURE() << "Round-trip mismatch in " << codec.name() << " at i="
                    << i << " expected=" << static_cast<int>(data[i])
                    << " got=" << static_cast<int>(data_back[i]);
      codec.clear();
      return false;
    }
  }
  codec.clear();
  return true;
}

// ── uint16 fixture ────────────────────────────────────────────────────────────

class U16CodecRoundtripTest : public ::testing::Test {
 protected:
  std::vector<uint16_t> small_data = {0, 1, 9, 3, 4, 5, 6, 7, 2, 65535};

  // 256 elements (16×16 for 2D predictors): deterministically includes 0 and
  // UINT16_MAX via the i=0 iteration (pushes 0 and kMax-0=65535) regardless
  // of the random fourth element.
  std::vector<uint16_t> large_data;

  void SetUp() override {
    const uint16_t kMax = 65535;
    const int kN = 64;

    std::mt19937 gen(42);
    std::uniform_int_distribution<uint16_t> distr(0, kMax);

    for (int i = 0; i < kN; ++i) {
      large_data.push_back(static_cast<uint16_t>(i));
      large_data.push_back(static_cast<uint16_t>(kMax - i));
      large_data.push_back(distr(gen));
      large_data.push_back(distr(gen));
    }
  }
};

// Helper: one fresh instance of each new logical codec.
static std::vector<std::unique_ptr<StatefulIntegerCodec<uint16_t>>>
MakeNewLogicalCodcsU16() {
  std::vector<std::unique_ptr<StatefulIntegerCodec<uint16_t>>> v;
  // Scalar
  v.push_back(std::make_unique<DeltaCodecU16>());
  v.push_back(std::make_unique<DoubleDeltaCodecU16>());
  v.push_back(std::make_unique<FORCodecU16>());
  v.push_back(std::make_unique<RLECodecU16>());
  // JPEG predictors
  v.push_back(std::make_unique<JpegPred1CodecU16>());
  v.push_back(std::make_unique<JpegPred2CodecU16>());
  v.push_back(std::make_unique<JpegPred3CodecU16>());
  v.push_back(std::make_unique<JpegPred4CodecU16>());
  v.push_back(std::make_unique<JpegPred5CodecU16>());
  v.push_back(std::make_unique<JpegPred6CodecU16>());
  v.push_back(std::make_unique<JpegPred7CodecU16>());
  v.push_back(std::make_unique<JpegLSMedCodecU16>());
  v.push_back(std::make_unique<PaethCodecU16>());
  return v;
}

// ── Standalone round-trip tests ───────────────────────────────────────────────

TEST_F(U16CodecRoundtripTest, AllLogicalCodecsStandalone) {
  for (auto& codec : BuildAllCodecsU16()) {
    SCOPED_TRACE(codec->name());
    EXPECT_TRUE(TestCodecU16(small_data, *codec));
    EXPECT_TRUE(TestCodecU16(large_data, *codec));
  }
}

// ── Composite: logical + SimdCompFused ───────────────────────────────────────
// Disabled: SimdCompFusedCodecU16 is commented out in codec_collection_uint16.h
// TEST_F(U16CodecRoundtripTest, AllLogicalCodecsPlusSimdComp) { ... }

// ── Composite: logical + FastPForFused ───────────────────────────────────────
// Disabled: FastPForFusedCodecU16 is commented out in codec_collection_uint16.h
// TEST_F(U16CodecRoundtripTest, AllLogicalCodecsPlusFastPFor) { ... }

// ── 2D predictor: linear gradient produces zero interior residuals ────────────

TEST(U16Predictor2D, LinearGradientZeroResiduals) {
  const int N = 16;  // 16×16 = 256 elements
  std::vector<uint16_t> data(N * N);
  for (int y = 0; y < N; ++y)
    for (int x = 0; x < N; ++x)
      data[y * N + x] = static_cast<uint16_t>(100 + 3 * x + 7 * y);

  // JpegPred4 (A+B-C) is exact for a linear gradient: residual = 0 everywhere
  // for interior pixels (row>0, col>0).
  JpegPred4CodecU16 pred4;
  pred4.AllocEncoded(data.data(), data.size());
  pred4.EncodeArray(data.data(), data.size());
  auto& enc = pred4.GetEncoded();
  int nonzero = 0;
  for (int y = 1; y < N; ++y)
    for (int x = 1; x < N; ++x)
      if (enc[y * N + x] != 0) ++nonzero;
  EXPECT_EQ(nonzero, 0)
      << "JpegPred4 on linear gradient must give 0 residuals for interior pixels";
  pred4.clear();

  // All predictors should round-trip correctly on this data.
  for (auto& codec : MakeNewLogicalCodcsU16()) {
    SCOPED_TRACE(codec->name());
    EXPECT_TRUE(TestCodecU16(data, *codec));
  }
}

// ── uint8 RLE tests ───────────────────────────────────────────────────────────

class U8CodecRoundtripTest : public ::testing::Test {
 protected:
  // small_data: short sequence with runs, boundary values (0 and 255), and
  // some variation.
  std::vector<uint8_t> small_data = {0, 0, 1, 2, 2, 2, 255, 255, 3, 0};

  // large_data: 256 elements explicitly covering all byte values 0–255.
  // Boundary values 0 and 255 are guaranteed (first and last elements).
  std::vector<uint8_t> large_data;

  void SetUp() override {
    // Each value 0–255 appears exactly once, so the full uint8 range is covered
    // and both boundary values are always present.
    for (int i = 0; i < 256; ++i)
      large_data.push_back(static_cast<uint8_t>(i));
  }
};

TEST_F(U8CodecRoundtripTest, RLECodecU8Standalone) {
  RLECodecU8 c;
  EXPECT_TRUE(TestCodecU8(small_data, c));
  EXPECT_TRUE(TestCodecU8(large_data, c));
}

// Edge case: entire block is a single constant value (max run = 256×256 = 65536).
// With run-1 encoding this fits in a single uint8 entry per 256-element chunk,
// so 256 entries total — no splitting artefacts.
TEST(U8RLEEdgeCases, FullBlockConstant) {
  std::vector<uint8_t> all_same(256 * 256, 42);
  RLECodecU8 c;
  EXPECT_TRUE(TestCodecU8(all_same, c));
  // Encoded size: ceil(65536/256) = 256 pairs × 2 bytes = 512 bytes.
  c.AllocEncoded(all_same.data(), all_same.size());
  c.EncodeArray(all_same.data(), all_same.size());
  EXPECT_EQ(c.EncodedNumValues(), 512u);
  c.clear();
}

TEST(U8RLEEdgeCases, AllDistinctValues) {
  // Worst case for RLE: no runs → one pair per element.
  std::vector<uint8_t> distinct(256);
  for (int i = 0; i < 256; ++i) distinct[i] = static_cast<uint8_t>(i);
  RLECodecU8 c;
  EXPECT_TRUE(TestCodecU8(distinct, c));
}

TEST(U8RLEEdgeCases, BoundaryValues) {
  std::vector<uint8_t> data = {0, 255, 0, 255, 128, 127, 1, 254};
  RLECodecU8 c;
  EXPECT_TRUE(TestCodecU8(data, c));
}
