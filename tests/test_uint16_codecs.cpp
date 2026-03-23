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
