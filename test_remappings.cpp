#include <algorithm>
#include <iostream>
#include <vector>

#include <gtest/gtest.h>

#include "remappings.h"

// ── Morton order ──────────────────────────────────────────────────────────────

TEST(RemappingTest, MortonOrder2x2) {
  // libmorton encode(x,y) interleaves x in even bits, y in odd bits:
  //   encode(0,0)=0, encode(1,0)=1, encode(0,1)=2, encode(1,1)=3
  // Input in row-major: input[y*N+x]: (0,0)=1,(1,0)=2,(0,1)=3,(1,1)=4
  // output[encode(x,y)] = input[y*N+x] → output = {1,2,3,4}
  std::vector<int32_t> input = {1, 2, 3, 4};
  auto result = RemapToMortonOrder(input, 2);
  ASSERT_EQ(result.size(), 4u);
  EXPECT_EQ(result[0], 1);  // encode(0,0)=0 ← input[0]
  EXPECT_EQ(result[1], 2);  // encode(1,0)=1 ← input[1]
  EXPECT_EQ(result[2], 3);  // encode(0,1)=2 ← input[2]
  EXPECT_EQ(result[3], 4);  // encode(1,1)=3 ← input[3]
}

TEST(RemappingTest, MortonOrderPreservesAllValues) {
  const int N = 8;
  std::vector<int32_t> input(N * N);
  for (int i = 0; i < N * N; i++) input[i] = i + 1;

  auto result = RemapToMortonOrder(input, N);

  ASSERT_EQ(result.size(), static_cast<size_t>(N * N));
  std::vector<int32_t> sorted_input = input;
  std::vector<int32_t> sorted_result = result;
  std::sort(sorted_input.begin(), sorted_input.end());
  std::sort(sorted_result.begin(), sorted_result.end());
  EXPECT_EQ(sorted_input, sorted_result);
}

TEST(RemappingTest, MortonOrderInvalidSizeThrows) {
  std::vector<int32_t> input = {1, 2, 3};  // not N*N
  EXPECT_THROW(RemapToMortonOrder(input, 2), std::invalid_argument);
}

// ── Zigzag order ──────────────────────────────────────────────────────────────

TEST(RemappingTest, ZigzagOrderEvenRowUnchangedOddRowReversed) {
  // Row 0 (even): unchanged {1,2}. Row 1 (odd): reversed {4,3}.
  std::vector<int32_t> input = {1, 2, 3, 4};
  auto result = RemapToZigzagOrder(input, 2);
  ASSERT_EQ(result.size(), 4u);
  EXPECT_EQ(result[0], 1);
  EXPECT_EQ(result[1], 2);
  EXPECT_EQ(result[2], 4);
  EXPECT_EQ(result[3], 3);
}

TEST(RemappingTest, ZigzagOrderPreservesAllValues) {
  const int N = 8;
  std::vector<int32_t> input(N * N);
  for (int i = 0; i < N * N; i++) input[i] = i + 1;

  auto result = RemapToZigzagOrder(input, N);

  ASSERT_EQ(result.size(), static_cast<size_t>(N * N));
  std::vector<int32_t> sorted_input = input;
  std::vector<int32_t> sorted_result = result;
  std::sort(sorted_input.begin(), sorted_input.end());
  std::sort(sorted_result.begin(), sorted_result.end());
  EXPECT_EQ(sorted_input, sorted_result);
}

TEST(RemappingTest, ZigzagLargeSmoke) {
  const int N = 256;
  std::vector<int32_t> input(N * N);
  for (int i = 0; i < N * N; i++) input[i] = i;
  auto result = RemapToZigzagOrder(input, N);
  EXPECT_EQ(result.size(), static_cast<size_t>(N * N));
}
