#pragma once

#include <algorithm>
#include <cstdint>
#include <vector>

// EXPECTED BEST FOLLOWING COMPRESSION METHOD: bitpack
inline void Threshold(std::vector<int32_t>& data, int32_t threshold_value) {
  for (auto& v : data) v = v >= threshold_value ? 1 : 0;
}

// EXPECTED BEST FOLLOWING COMPRESSION METHOD: delta
inline void SmoothAndShift(std::vector<int32_t>& data) {
  if (data.size() < 2) return;

  std::vector<int32_t> smoothed(data.size(), 0);

  for (std::size_t i = 1; i < data.size() - 1; ++i) {
    smoothed[i] = (data[i - 1] + data[i] + data[i + 1]) / 3;
  }
  smoothed[0] = (data[0] + data[1]) / 2;
  smoothed.back() = (data[data.size() - 2] + data.back()) / 2;

  int32_t minVal = *std::ranges::min_element(smoothed);
  if (minVal < 0) {
    int32_t shift = -minVal;
    for (std::size_t i = 0; i < smoothed.size(); ++i)
      data[i] = smoothed[i] + shift;
  } else {
    std::ranges::copy(smoothed, data.begin());
  }
}

// EXPECTED BEST FOLLOWING COMPRESSION METHOD: rle
inline void IndexBasedClassification(std::vector<int32_t>& data,
                                     int max_classes) {
  std::size_t n = data.size();
  std::size_t items_per_class = n / static_cast<std::size_t>(max_classes);
  for (std::size_t i = 0; i < n; ++i)
    data[i] = static_cast<int32_t>(i / items_per_class);
  for (std::size_t i = items_per_class * static_cast<std::size_t>(max_classes);
       i < n; ++i)
    data[i] = max_classes - 1;
}

// EXPECTED BEST FOLLOWING COMPRESSION METHOD: dict or rle
inline void ValueBasedClassification(std::vector<int32_t>& data,
                                     int num_classes) {
  int32_t minVal = *std::ranges::min_element(data);
  int32_t maxVal = *std::ranges::max_element(data);
  int32_t range = maxVal - minVal;
  int32_t binSize = range / (num_classes - 1);

  for (auto& val : data) {
    val = (range == 0 || binSize == 0) ? 0 : (val - minVal) / binSize;
    val = std::min(val, num_classes - 1);
  }
}

// EXPECTED BEST FOLLOWING COMPRESSION METHOD: for
inline void ValueShift(std::vector<int32_t>& data, int32_t delta) {
  for (auto& v : data) v += delta;
}
