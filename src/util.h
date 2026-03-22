#pragma once

#include <cmath>
#include <numeric>
#include <string>
#include <vector>

inline int32_t Avg(const std::vector<int32_t>& data) {
  if (data.empty()) return 0;
  int64_t total = std::reduce(data.begin(), data.end(), int64_t{0});
  return static_cast<int32_t>(total / static_cast<int64_t>(data.size()));
}

inline float Mean(const std::vector<float>& values) {
  if (values.empty()) return 0;
  return std::reduce(values.begin(), values.end(), 0.0f) /
         static_cast<float>(values.size());
}

inline float Variance(const std::vector<float>& values, float meanValue) {
  if (values.empty()) return 0;
  float sq_sum = std::accumulate(
      values.begin(), values.end(), 0.0f,
      [meanValue](float a, float b) { return a + (b - meanValue) * (b - meanValue); });
  return sq_sum / static_cast<float>(values.size());
}

inline std::size_t Sum(const std::vector<std::size_t>& values) {
  return std::reduce(values.begin(), values.end(), std::size_t{0});
}

inline float Mean(const std::vector<std::size_t>& values) {
  if (values.empty()) return 0;
  return static_cast<float>(Sum(values)) / static_cast<float>(values.size());
}

inline float Variance(const std::vector<std::size_t>& values, float meanValue) {
  if (values.empty()) return 0;
  float sq_sum = std::accumulate(
      values.begin(), values.end(), 0.0f,
      [meanValue](float acc, std::size_t b) {
        float diff = static_cast<float>(b) - meanValue;
        return acc + diff * diff;
      });
  return sq_sum / static_cast<float>(values.size());
}
