#include <vector>
#include <cstdint>
#include <algorithm>

// EXPECTED BEST FOLLOWING COMPRESSION METHOD: bitpack 
void threshold(std::vector<int32_t>& data, const int32_t threshold_value) {
    for (int i = 0; i < data.size(); i++) {
        data[i] = data[i] >= threshold_value ? 1 : 0;
    }
}

// EXPECTED BEST FOLLOWING COMPRESSION METHOD: delta
void smoothAndShift(std::vector<int32_t>& data) {
    if (data.size() < 2) return;

    std::vector<int32_t> smoothed(data.size(), 0);

    for (size_t i = 1; i < data.size() - 1; ++i) {
        smoothed[i] = (data[i-1] + data[i] + data[i+1]) / 3;
    }

    smoothed[0] = (data[0] + data[1]) / 2;
    smoothed[data.size() - 1] = (data[data.size() - 2] + data[data.size() - 1]) / 2;

    int32_t minVal = *std::min_element(smoothed.begin(), smoothed.end());

    if (minVal < 0) {
        int32_t shiftValue = -minVal;
        for (size_t i = 0; i < smoothed.size(); ++i) {
            data[i] = smoothed[i] + shiftValue;
        }
    } else {
        std::copy(smoothed.begin(), smoothed.end(), data.begin());
    }
}

// EXPECTED BEST FOLLOWING COMPRESSION METHOD: rle
void indexBasedClassification(std::vector<int32_t>& data, int max_classes) {
    int items_per_class = data.size() / max_classes;
    for (int i = 0; i < data.size(); ++i) {
        data[i] = i / items_per_class;
    }
    for (int i = items_per_class * max_classes; i < data.size(); ++i) {
        data[i] = max_classes - 1;
    }
}

// EXPECTED BEST FOLLOWING COMPRESSION METHOD: dict or rle
void valueBasedClassification(std::vector<int32_t>& data, int num_classes) {
    int32_t minVal = *std::min_element(data.begin(), data.end());
    int32_t maxVal = *std::max_element(data.begin(), data.end());
    int32_t range = maxVal - minVal;
    int32_t binSize = range / (num_classes - 1);

    for (auto& val : data) {
        if (range == 0 || binSize == 0)
            val = 0;
        else
            val = (val - minVal) / binSize;
        val = std::min(val, num_classes - 1);
    }
}

// EXPECTED BEST FOLLOWING COMPRESSION METHOD: for
void valueShift(std::vector<int32_t>& data, int32_t delta /* idea: 2^22 */) {
    for (int i = 0; i < data.size(); i++) {
        data[i] += delta;
    }
}