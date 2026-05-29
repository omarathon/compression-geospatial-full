#include <memory>
#include <random>
#include <vector>

#include <gtest/gtest.h>

#include "codec_collection_uint16.h"

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
  return InitLogicalCodecsU16();
}

static std::vector<std::unique_ptr<StatefulIntegerCodec<uint16_t>>>
MakeAllCodecs() {
  return BuildAllCodecsU16();
}

// ── round-trip tests ───────────────────────────────────────────────

TEST_F(U16CodecRoundtripTest, AllCodecs) {
  for (auto& codec : MakeAllCodecs()) {
    // custom_direct_access intentionally has a no-op DecodeArray — callers read
    // GetEncoded() directly. Round-trip via DecodeArray is not its contract.
    if (codec->name() == "custom_direct_access") continue;
    SCOPED_TRACE(codec->name());
    std::cout << codec->name() << std::endl;
    EXPECT_TRUE(TestCodecU16(small_data, *codec));
    EXPECT_TRUE(TestCodecU16(large_data, *codec));
  }
}

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
