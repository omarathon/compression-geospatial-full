#include <memory>
#include <random>
#include <vector>

#include <gtest/gtest.h>

#include "composite_codec.h"
#include "generic_codecs.h"
#include "codec_collection_uint16.h"

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

class U16CodecRoundtripTest : public ::testing::Test {
 protected:
  std::vector<uint16_t> small_data = {10, 1, 9, 3, 4, 5, 6, 7, 2, 8};

  // Larger pseudo-random dataset. 256 elements: divisible by 8, 128 for
  // block-based codecs (simdcomp, FastPFor).
  std::vector<uint16_t> large_data;

  void SetUp() override {
    const uint16_t kMax = 65535;
    const int kN = 64;

    std::mt19937 gen(42);
    std::uniform_int_distribution<uint16_t> distr(0, kMax);

    for (int i = 0; i < kN; ++i) {
      large_data.push_back(static_cast<uint16_t>(i));
      large_data.push_back(static_cast<uint16_t>(i));
      large_data.push_back(static_cast<uint16_t>(kMax - i));
      large_data.push_back(distr(gen));
    }
  }
};

// ── Logical codec round-trip tests ──────────────────────────────────────────

TEST_F(U16CodecRoundtripTest, DeltaCodecSSE42U16) {
  DeltaCodecSSE42U16 c;
  EXPECT_TRUE(TestCodecU16(small_data, c));
  EXPECT_TRUE(TestCodecU16(large_data, c));
}

TEST_F(U16CodecRoundtripTest, FORCodecSSE42U16) {
  FORCodecSSE42U16 c;
  EXPECT_TRUE(TestCodecU16(small_data, c));
  EXPECT_TRUE(TestCodecU16(large_data, c));
}

TEST_F(U16CodecRoundtripTest, RLECodecSSE42U16) {
  RLECodecSSE42U16 c;
  EXPECT_TRUE(TestCodecU16(small_data, c));
  EXPECT_TRUE(TestCodecU16(large_data, c));
}

// ── New logical codecs ──────────────────────────────────────────────────────

TEST_F(U16CodecRoundtripTest, DoubleDeltaCodecU16) {
  DoubleDeltaCodecU16 c;
  EXPECT_TRUE(TestCodecU16(small_data, c));
  EXPECT_TRUE(TestCodecU16(large_data, c));
}

TEST_F(U16CodecRoundtripTest, XorDeltaCodecU16) {
  XorDeltaCodecU16 c;
  EXPECT_TRUE(TestCodecU16(small_data, c));
  EXPECT_TRUE(TestCodecU16(large_data, c));
}

TEST_F(U16CodecRoundtripTest, ByteShuffleCodecU16) {
  ByteShuffleCodecU16 c;
  EXPECT_TRUE(TestCodecU16(small_data, c));
  EXPECT_TRUE(TestCodecU16(large_data, c));
}

// 2D predictors need square data — large_data is 256 = 16x16.
TEST_F(U16CodecRoundtripTest, PredUpCodecU16) {
  PredUpCodecU16 c;
  EXPECT_TRUE(TestCodecU16(large_data, c));  // 256 = 16x16
}

TEST_F(U16CodecRoundtripTest, PredAvgCodecU16) {
  PredAvgCodecU16 c;
  EXPECT_TRUE(TestCodecU16(large_data, c));
}

TEST_F(U16CodecRoundtripTest, PredLorenzoCodecU16) {
  PredLorenzoCodecU16 c;
  EXPECT_TRUE(TestCodecU16(large_data, c));
}

TEST_F(U16CodecRoundtripTest, PredJPEGLSCodecU16) {
  PredJPEGLSCodecU16 c;
  EXPECT_TRUE(TestCodecU16(large_data, c));
}

// Test 2D predictors on a larger 64x64 block with smooth gradient data
// (simulates elevation-like data where 2D predictors should excel).
TEST(U16Predictor2D, SmoothGradient64x64) {
  const int N = 64;
  std::vector<uint16_t> data(N * N);
  for (int y = 0; y < N; ++y)
    for (int x = 0; x < N; ++x)
      data[y * N + x] = static_cast<uint16_t>(100 + 3 * x + 7 * y);

  PredUpCodecU16 up;
  EXPECT_TRUE(TestCodecU16(data, up));

  PredAvgCodecU16 avg;
  EXPECT_TRUE(TestCodecU16(data, avg));

  PredLorenzoCodecU16 lor;
  EXPECT_TRUE(TestCodecU16(data, lor));

  PredJPEGLSCodecU16 jpegls;
  EXPECT_TRUE(TestCodecU16(data, jpegls));

  // Lorenzo on a linear gradient should produce near-zero residuals.
  // Verify: encode and check most residuals are tiny.
  lor.clear();
  lor.AllocEncoded(data.data(), data.size());
  lor.EncodeArray(data.data(), data.size());
  auto& enc = lor.GetEncoded();
  int nonzero = 0;
  for (int y = 1; y < N; ++y)
    for (int x = 1; x < N; ++x)
      if (enc[y * N + x] != 0) ++nonzero;
  EXPECT_EQ(nonzero, 0) << "Lorenzo on linear gradient should give 0 residuals "
                            "for interior pixels (row>0, col>0)";
}
