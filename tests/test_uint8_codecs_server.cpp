#include <vector>

#include <gtest/gtest.h>

#include "codec_collection_uint8.h"

// ── Helpers ──────────────────────────────────────────────────────────────────

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
// With run-1 encoding this fits in 256 chunks of 256 → 512 encoded bytes.
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
