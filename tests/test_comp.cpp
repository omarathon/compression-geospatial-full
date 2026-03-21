#include <cmath>
#include <limits>
#include <memory>
#include <random>
#include <vector>

#include <gtest/gtest.h>

#include "codecs/composite_codec.h"
#include "codecs/custom_unvec_logic_codecs.h"
#include "codecs/custom_vec_logic_codecs.h"
#include "codecs/deflate_codecs.h"
#include "codecs/fastpfor_codecs.h"
#include "codecs/frameofreference_codecs.h"
#include "codecs/generic_codecs.h"
#include "codecs/lz4_codecs.h"
#include "codecs/lzma_codecs.h"
#include "codecs/maskedvbyte_codecs.h"
#include "codecs/simdcomp_codecs.h"
#include "codecs/streamvbyte_codecs.h"
#include "codecs/turbopfor_codecs.h"
#include "codecs/zstd_codecs.h"

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

  for (size_t i = 0; i < data.size(); i++) {
    if (data[i] != data_back[i]) {
      ADD_FAILURE() << "Round-trip mismatch in " << codec.name() << " at i="
                    << i << " expected=" << data[i]
                    << " got=" << data_back[i];
      codec.clear();
      return false;
    }
  }
  codec.clear();
  return true;
}

// ── Test fixture ──────────────────────────────────────────────────────────────

class CodecRoundtripTest : public ::testing::Test {
 protected:
  // Small deterministic sequence used for most codecs.
  std::vector<int32_t> small_data = {10, 1, 9, 3, 4, 5, 6, 7, 2, 8};

  // Larger pseudo-random dataset. Range [0, 2^28) satisfies FastPFor_Simple16.
  // Also used for codecs that require >=32 inputs (e.g. FastPFor BP32).
  std::vector<int32_t> large_data;

  void SetUp() override {
    const int kMin = 0;
    const int kMax = (1 << 28) - 1;
    // 64×4=256 elements: divisible by 8,16,32,128,256 for block-based FastPFor.
    const int kN = 64;

    std::mt19937 gen(42);  // fixed seed for reproducibility
    std::uniform_int_distribution<> distr(kMin, kMax);

    for (int i = 0; i < kN; i++) {
      large_data.push_back(i);
      large_data.push_back(kMin + i);
      large_data.push_back(kMax - i);
      large_data.push_back(distr(gen));
    }
  }
};

// ── Individual codec tests ────────────────────────────────────────────────────

TEST_F(CodecRoundtripTest, DeltaCodec) {
  DeltaCodec c;
  EXPECT_TRUE(TestCodec(small_data, c));
  EXPECT_TRUE(TestCodec(large_data, c));
}

TEST_F(CodecRoundtripTest, DeltaCodecSSE42) {
  DeltaCodecSSE42 c;
  EXPECT_TRUE(TestCodec(small_data, c));
  EXPECT_TRUE(TestCodec(large_data, c));
}

TEST_F(CodecRoundtripTest, DeltaCodecAVX2) {
  DeltaCodecAVX2 c;
  EXPECT_TRUE(TestCodec(small_data, c));
  EXPECT_TRUE(TestCodec(large_data, c));
}

TEST_F(CodecRoundtripTest, DeltaCodecAVX512) {
  DeltaCodecAVX512 c;
  EXPECT_TRUE(TestCodec(small_data, c));
  EXPECT_TRUE(TestCodec(large_data, c));
}

TEST_F(CodecRoundtripTest, FORCodec) {
  FORCodec c;
  EXPECT_TRUE(TestCodec(small_data, c));
  EXPECT_TRUE(TestCodec(large_data, c));
}

TEST_F(CodecRoundtripTest, FORCodecSSE42) {
  FORCodecSSE42 c;
  EXPECT_TRUE(TestCodec(small_data, c));
  EXPECT_TRUE(TestCodec(large_data, c));
}

TEST_F(CodecRoundtripTest, FORCodecAVX2) {
  FORCodecAVX2 c;
  EXPECT_TRUE(TestCodec(small_data, c));
  EXPECT_TRUE(TestCodec(large_data, c));
}

TEST_F(CodecRoundtripTest, FORCodecAVX512) {
  FORCodecAVX512 c;
  EXPECT_TRUE(TestCodec(small_data, c));
  EXPECT_TRUE(TestCodec(large_data, c));
}

TEST_F(CodecRoundtripTest, RLECodec) {
  RLECodec c;
  EXPECT_TRUE(TestCodec(small_data, c));
  EXPECT_TRUE(TestCodec(large_data, c));
}

TEST_F(CodecRoundtripTest, RLECodecSSE42) {
  RLECodecSSE42 c;
  EXPECT_TRUE(TestCodec(small_data, c));
  EXPECT_TRUE(TestCodec(large_data, c));
}

TEST_F(CodecRoundtripTest, RLECodecAVX2) {
  RLECodecAVX2 c;
  EXPECT_TRUE(TestCodec(small_data, c));
  EXPECT_TRUE(TestCodec(large_data, c));
}

TEST_F(CodecRoundtripTest, RLECodecAVX512) {
  RLECodecAVX512 c;
  EXPECT_TRUE(TestCodec(small_data, c));
  EXPECT_TRUE(TestCodec(large_data, c));
}

TEST_F(CodecRoundtripTest, DeflateCodec) {
  DeflateCodec c;
  EXPECT_TRUE(TestCodec(small_data, c));
  EXPECT_TRUE(TestCodec(large_data, c));
}

TEST_F(CodecRoundtripTest, MaskedVByteCodec) {
  MaskedVByteCodec c;
  EXPECT_TRUE(TestCodec(small_data, c));
  EXPECT_TRUE(TestCodec(large_data, c));
}

TEST_F(CodecRoundtripTest, MaskedVByteDeltaCodec) {
  MaskedVByteDeltaCodec c;
  EXPECT_TRUE(TestCodec(small_data, c));
  EXPECT_TRUE(TestCodec(large_data, c));
}

TEST_F(CodecRoundtripTest, StreamVByteCodec) {
  StreamVByteCodec c;
  EXPECT_TRUE(TestCodec(small_data, c));
  EXPECT_TRUE(TestCodec(large_data, c));
}

TEST_F(CodecRoundtripTest, FrameOfReferenceCodec) {
  FrameOfReferenceCodec c;
  EXPECT_TRUE(TestCodec(small_data, c));
  EXPECT_TRUE(TestCodec(large_data, c));
}

TEST_F(CodecRoundtripTest, FrameOfReferenceTurboCodec) {
  FrameOfReferenceTurboCodec c;
  EXPECT_TRUE(TestCodec(small_data, c));
  EXPECT_TRUE(TestCodec(large_data, c));
}

TEST_F(CodecRoundtripTest, SimdCompCodec) {
  SimdCompCodec c;
  EXPECT_TRUE(TestCodec(small_data, c));
  EXPECT_TRUE(TestCodec(large_data, c));
}

TEST_F(CodecRoundtripTest, LZ4Codec) {
  LZ4Codec c;
  EXPECT_TRUE(TestCodec(small_data, c));
  EXPECT_TRUE(TestCodec(large_data, c));
}

TEST_F(CodecRoundtripTest, ZstdCodec3) {
  ZstdCodec c(3);
  EXPECT_TRUE(TestCodec(small_data, c));
  EXPECT_TRUE(TestCodec(large_data, c));
}

TEST_F(CodecRoundtripTest, TurboPForAllMethods) {
  for (size_t method = 1; method <= 20; method++) {
    if (method == 11) continue;
    SCOPED_TRACE("TurboPFor method=" + std::to_string(method));
    TurboPForCodec c(method);
    EXPECT_TRUE(TestCodec(small_data, c));
    EXPECT_TRUE(TestCodec(large_data, c));
  }
}

TEST_F(CodecRoundtripTest, FastPForAllSchemes) {
  CODECFactory factory;
  const std::vector<std::string> kBroken = {
      "Simple8b_RLE", "Simple9_RLE", "SimplePFor+VariableByte", "VSEncoding"};
  for (auto& fpf : factory.allSchemes()) {
    if (std::find(kBroken.begin(), kBroken.end(), fpf->name()) !=
        kBroken.end())
      continue;
    SCOPED_TRACE("FastPFor codec=" + fpf->name());
    FastPForCodec c(fpf);
    // Some schemes (e.g. BP32) require >=32 inputs; skip small_data.
    EXPECT_TRUE(TestCodec(large_data, c));
  }
}

TEST_F(CodecRoundtripTest, CompositeDeltaAVX512PlusTurboPFor) {
  for (size_t method = 1; method <= 20; method++) {
    if (method == 11) continue;
    SCOPED_TRACE("method=" + std::to_string(method));
    auto delta = std::make_unique<DeltaCodecAVX512>();
    auto tpf = std::make_unique<TurboPForCodec>(method);
    CompositeStatefulIntegerCodec<int32_t> c(std::move(delta), std::move(tpf));
    EXPECT_TRUE(TestCodec(small_data, c));
    EXPECT_TRUE(TestCodec(large_data, c));
  }
}

TEST_F(CodecRoundtripTest, CompositeRLEAVX512PlusTurboPFor) {
  for (size_t method = 1; method <= 20; method++) {
    if (method == 11) continue;
    SCOPED_TRACE("method=" + std::to_string(method));
    auto rle = std::make_unique<RLECodecAVX512>();
    auto tpf = std::make_unique<TurboPForCodec>(method);
    CompositeStatefulIntegerCodec<int32_t> c(std::move(rle), std::move(tpf));
    EXPECT_TRUE(TestCodec(small_data, c));
    EXPECT_TRUE(TestCodec(large_data, c));
  }
}
