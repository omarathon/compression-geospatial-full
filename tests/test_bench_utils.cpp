#include <algorithm>
#include <numeric>
#include <vector>

#include <gtest/gtest.h>

#include "bench_utils.h"
#include "codec_collection.h"  // includes zstd_codecs.h and all other codecs

// ─── ParseOrdering ────────────────────────────────────────────────────────────

TEST(ParseOrdering, RecognisesAllVariants) {
  EXPECT_EQ(ParseOrdering("zigzag"), Ordering::Zigzag);
  EXPECT_EQ(ParseOrdering("morton"), Ordering::Morton);
  EXPECT_EQ(ParseOrdering("default"), Ordering::RowMajor);
  EXPECT_EQ(ParseOrdering(""), Ordering::RowMajor);
}

TEST(ParseOrdering, RoundTrips) {
  EXPECT_EQ(ToString(ParseOrdering("zigzag")), "zigzag");
  EXPECT_EQ(ToString(ParseOrdering("morton")), "morton");
  EXPECT_EQ(ToString(ParseOrdering("default")), "default");
}

TEST(ParseOrdering, ThrowsOnUnknown) {
  EXPECT_THROW(ParseOrdering("invalid_xyz"), std::invalid_argument);
}

// ─── ParseTransformation ──────────────────────────────────────────────────────

TEST(ParseTransformation, RecognisesAllVariants) {
  EXPECT_EQ(ParseTransformation("none"), Transformation::None);
  EXPECT_EQ(ParseTransformation(""), Transformation::None);
  EXPECT_EQ(ParseTransformation("Threshold"), Transformation::Threshold);
  EXPECT_EQ(ParseTransformation("SmoothAndShift"),
            Transformation::SmoothAndShift);
  EXPECT_EQ(ParseTransformation("IndexBasedClassification"),
            Transformation::IndexBasedClassification);
  EXPECT_EQ(ParseTransformation("ValueBasedClassification"),
            Transformation::ValueBasedClassification);
  EXPECT_EQ(ParseTransformation("ValueShift"), Transformation::ValueShift);
}

TEST(ParseTransformation, RoundTrips) {
  EXPECT_EQ(ToString(ParseTransformation("none")), "none");
  EXPECT_EQ(ToString(ParseTransformation("Threshold")), "Threshold");
  EXPECT_EQ(ToString(ParseTransformation("SmoothAndShift")), "SmoothAndShift");
  EXPECT_EQ(ToString(ParseTransformation("IndexBasedClassification")),
            "IndexBasedClassification");
  EXPECT_EQ(ToString(ParseTransformation("ValueBasedClassification")),
            "ValueBasedClassification");
  EXPECT_EQ(ToString(ParseTransformation("ValueShift")), "ValueShift");
}

TEST(ParseTransformation, ThrowsOnUnknown) {
  EXPECT_THROW(ParseTransformation("invalid_xyz"), std::invalid_argument);
}

// ─── ParseAccessPattern ───────────────────────────────────────────────────────

TEST(ParseAccessPattern, RecognisesAllVariants) {
  EXPECT_EQ(ParseAccessPattern("linear"), AccessPattern::Linear);
  EXPECT_EQ(ParseAccessPattern("random"), AccessPattern::Random);
}

TEST(ParseAccessPattern, RoundTrips) {
  EXPECT_EQ(ToString(ParseAccessPattern("linear")), "linear");
  EXPECT_EQ(ToString(ParseAccessPattern("random")), "random");
}

TEST(ParseAccessPattern, ThrowsOnUnknown) {
  EXPECT_THROW(ParseAccessPattern("invalid_xyz"), std::invalid_argument);
}

// ─── ParseAccessTransformation ───────────────────────────────────────────────

TEST(ParseAccessTransformation, RecognisesAllVariants) {
  EXPECT_EQ(ParseAccessTransformation("linearXOR"),
            AccessTransformation::LinearXOR);
  EXPECT_EQ(ParseAccessTransformation("linearSum"),
            AccessTransformation::LinearSum);
  EXPECT_EQ(ParseAccessTransformation("linearSumSimd"),
            AccessTransformation::LinearSumSimd);
  EXPECT_EQ(ParseAccessTransformation("linearSumFused"),
            AccessTransformation::LinearSumFused);
  EXPECT_EQ(ParseAccessTransformation("randomXOR"),
            AccessTransformation::RandomXOR);
  EXPECT_EQ(ParseAccessTransformation("randomSum"),
            AccessTransformation::RandomSum);
  EXPECT_EQ(ParseAccessTransformation("Threshold"),
            AccessTransformation::Threshold);
  EXPECT_EQ(ParseAccessTransformation("SmoothAndShift"),
            AccessTransformation::SmoothAndShift);
  EXPECT_EQ(ParseAccessTransformation("IndexBasedClassification"),
            AccessTransformation::IndexBasedClassification);
  EXPECT_EQ(ParseAccessTransformation("ValueBasedClassification"),
            AccessTransformation::ValueBasedClassification);
  EXPECT_EQ(ParseAccessTransformation("ValueShift"),
            AccessTransformation::ValueShift);
}

TEST(ParseAccessTransformation, RoundTrips) {
  EXPECT_EQ(ToString(ParseAccessTransformation("linearXOR")), "linearXOR");
  EXPECT_EQ(ToString(ParseAccessTransformation("linearSumSimd")), "linearSumSimd");
  EXPECT_EQ(ToString(ParseAccessTransformation("linearSumFused")), "linearSumFused");
  EXPECT_EQ(ToString(ParseAccessTransformation("randomSum")), "randomSum");
  EXPECT_EQ(ToString(ParseAccessTransformation("Threshold")), "Threshold");
  EXPECT_EQ(ToString(ParseAccessTransformation("ValueShift")), "ValueShift");
}

TEST(ParseAccessTransformation, ThrowsOnUnknown) {
  EXPECT_THROW(ParseAccessTransformation("invalid_xyz"), std::invalid_argument);
}

// ─── RemapAndTransform ────────────────────────────────────────────────────────

TEST(RemapAndTransform, RowMajorNoneIsIdentity) {
  std::vector<int32_t> data(16);
  std::iota(data.begin(), data.end(), 0);
  auto original = data;
  RemapAndTransform(data, Ordering::RowMajor, Transformation::None, 4);
  EXPECT_EQ(data, original);
}

TEST(RemapAndTransform, ZigzagPermutesElements) {
  std::vector<int32_t> data(16);
  std::iota(data.begin(), data.end(), 1);
  auto original = data;
  RemapAndTransform(data, Ordering::Zigzag, Transformation::None, 4);
  // The ordering changes but all elements are still present.
  EXPECT_NE(data, original);
  auto sorted_result = data;
  auto sorted_orig = original;
  std::ranges::sort(sorted_result);
  std::ranges::sort(sorted_orig);
  EXPECT_EQ(sorted_result, sorted_orig);
}

// ─── AccessTransformationMutatesData ─────────────────────────────────────────

TEST(AccessTransformationMutatesData, MutatingVariants) {
  EXPECT_TRUE(
      AccessTransformationMutatesData(AccessTransformation::Threshold));
  EXPECT_TRUE(
      AccessTransformationMutatesData(AccessTransformation::SmoothAndShift));
  EXPECT_TRUE(AccessTransformationMutatesData(
      AccessTransformation::IndexBasedClassification));
  EXPECT_TRUE(AccessTransformationMutatesData(
      AccessTransformation::ValueBasedClassification));
  EXPECT_TRUE(
      AccessTransformationMutatesData(AccessTransformation::ValueShift));
}

TEST(AccessTransformationMutatesData, ReadOnlyVariants) {
  EXPECT_FALSE(
      AccessTransformationMutatesData(AccessTransformation::LinearXOR));
  EXPECT_FALSE(
      AccessTransformationMutatesData(AccessTransformation::LinearSum));
  EXPECT_FALSE(
      AccessTransformationMutatesData(AccessTransformation::LinearSumSimd));
  EXPECT_FALSE(
      AccessTransformationMutatesData(AccessTransformation::LinearSumFused));
  EXPECT_FALSE(
      AccessTransformationMutatesData(AccessTransformation::RandomXOR));
  EXPECT_FALSE(
      AccessTransformationMutatesData(AccessTransformation::RandomSum));
}

// ─── SelectCodecsByName ───────────────────────────────────────────────────────

TEST(SelectCodecsByName, AllKeyword) {
  auto pool = InitCodecs(true, nullptr);
  ASSERT_GT(pool.size(), 0u);
  auto selected = SelectCodecsByName(pool, {"all"});
  EXPECT_EQ(selected.size(), pool.size());
}

TEST(SelectCodecsByName, StarKeyword) {
  auto pool = InitCodecs(true, nullptr);
  auto selected = SelectCodecsByName(pool, {"*"});
  EXPECT_EQ(selected.size(), pool.size());
}

TEST(SelectCodecsByName, ByName) {
  auto pool = InitCodecs(true, nullptr);
  ASSERT_GT(pool.size(), 0u);
  std::string firstName = pool[0]->name();
  auto selected = SelectCodecsByName(pool, {firstName});
  ASSERT_EQ(selected.size(), 1u);
  EXPECT_EQ(selected[0]->name(), firstName);
}

TEST(SelectCodecsByName, UnknownNameReturnsEmpty) {
  auto pool = InitCodecs(true, nullptr);
  auto selected = SelectCodecsByName(pool, {"no_such_codec_xyz_999"});
  EXPECT_TRUE(selected.empty());
}

// ─── SampleBlockOffsets ───────────────────────────────────────────────────────

TEST(SampleBlockOffsets, CorrectCount) {
  auto offsets = SampleBlockOffsets(10, 10, 64, 5);
  EXPECT_EQ(offsets.size(), 5u);
}

TEST(SampleBlockOffsets, FirstOffsetIsOrigin) {
  auto offsets = SampleBlockOffsets(10, 10, 64, 5);
  EXPECT_EQ(offsets[0].x, 0);
  EXPECT_EQ(offsets[0].y, 0);
}

TEST(SampleBlockOffsets, OffsetsAlignedToBlockSize) {
  int blockSize = 64;
  auto offsets = SampleBlockOffsets(10, 10, blockSize, 5);
  for (auto& o : offsets) {
    EXPECT_EQ(o.x % blockSize, 0);
    EXPECT_EQ(o.y % blockSize, 0);
  }
}

TEST(SampleBlockOffsets, SingleBlock) {
  auto offsets = SampleBlockOffsets(5, 5, 32, 1);
  ASSERT_EQ(offsets.size(), 1u);
  EXPECT_EQ(offsets[0].x, 0);
  EXPECT_EQ(offsets[0].y, 0);
}

// ─── BenchmarkOneCodec ────────────────────────────────────────────────────────

TEST(BenchmarkOneCodec, RoundTripAndStats) {
  std::vector<int32_t> data(512);
  std::iota(data.begin(), data.end(), 0);
  auto dataOrig = data;

  // Use explicit base type so BenchmarkOneCodec<int32_t> deduction works.
  std::unique_ptr<StatefulIntegerCodec<int32_t>> codec =
      std::make_unique<ZstdCodec>(1);
  auto stats = BenchmarkOneCodec(data, codec);

  // Data must be unchanged (BenchmarkOneCodec only reads it for encoding).
  EXPECT_EQ(data, dataOrig);

  // Stats must be non-trivial after a successful round-trip.
  EXPECT_GT(stats.cf, 0.0f);
  EXPECT_GT(stats.bpi, 0.0f);
  EXPECT_GE(stats.tenc, 0.0f);
  EXPECT_GE(stats.tdec, 0.0f);
}
