#include <memory>
#include <random>
#include <vector>

#include <gtest/gtest.h>

#include <codec_collection.h>

// Returns true if codec correctly round-trips `data`.
static bool TestCodec(std::vector<int32_t>& data,
                      StatefulIntegerCodec<int32_t>& codec) {
  codec.clear();
  try {
    codec.AllocEncoded(data.data(), data.size());
    codec.EncodeArray(data.data(), data.size());
  } catch (const std::exception& e) {
    ADD_FAILURE() << "Encode error in " << codec.name() << ": " << e.what();
    return false;
  }

  std::vector<int32_t> data_back(data.size() +
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

class CodecRoundtripTest : public ::testing::Test {
 protected:
  // Small deterministic sequence used for most codecs.
  std::vector<int32_t> small_data = {10, 1, 9, 3, 4, 5, 6, 7, 2, 8};

  // Larger pseudo-random dataset. Range [0, 2^28) satisfies FastPFor_Simple16.
  // 64×4=256 elements: divisible by 8,16,32,128,256 for block-based codecs.
  // i=0 deterministically pushes 0 and kMax=(2^28-1), so boundary values are
  // always present regardless of the random fourth element.
  std::vector<int32_t> large_data;

  void SetUp() override {
    const int kMin = 0;
    const int kMax = (1 << 28) - 1;
    const int kN = 64;

    std::mt19937 gen(42);
    std::uniform_int_distribution<> distr(kMin, kMax);

    for (int i = 0; i < kN; ++i) {
      large_data.push_back(i);
      large_data.push_back(kMin + i);
      large_data.push_back(kMax - i);
      large_data.push_back(distr(gen));
    }
  }
};

static std::vector<std::unique_ptr<StatefulIntegerCodec<int32_t>>>
MakeAllCodecs() {
  return BuildAllCodecs();
}

TEST_F(CodecRoundtripTest, AllCodecs) {
  for (auto& codec : MakeAllCodecs()) {
    SCOPED_TRACE(codec->name());
    std::cout << codec->name() << std::endl;
    EXPECT_TRUE(TestCodec(small_data, *codec));
    EXPECT_TRUE(TestCodec(large_data, *codec));
  }
}
