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
    if (data.size() < 2) return; // No smoothing or shifting needed for 1-element or empty vectors

    std::vector<int32_t> smoothed(data.size(), 0);

    // Apply smoothing with a simple moving average for the internal elements
    for (size_t i = 1; i < data.size() - 1; ++i) {
        smoothed[i] = (data[i-1] + data[i] + data[i+1]) / 3;
    }

    // Handle edge cases
    smoothed[0] = (data[0] + data[1]) / 2; // First element
    smoothed[data.size() - 1] = (data[data.size() - 2] + data[data.size() - 1]) / 2; // Last element

    // Find the minimum value in the smoothed data
    int32_t minVal = *std::min_element(smoothed.begin(), smoothed.end());

    // If the minimum value is negative, shift all values up
    if (minVal < 0) {
        int32_t shiftValue = -minVal; // Calculate shift value to make the smallest value 0
        for (size_t i = 0; i < smoothed.size(); ++i) {
            // Apply the shift and update the original data
            data[i] = smoothed[i] + shiftValue;
        }
    } else {
        // If all values are already positive, just copy the smoothed values back to the original data
        std::copy(smoothed.begin(), smoothed.end(), data.begin());
    }
}

// EXPECTED BEST FOLLOWING COMPRESSION METHOD: rle
void indexBasedClassification(std::vector<int32_t>& data, int max_classes) {
    int items_per_class = data.size() / max_classes; // Number of items per class
    for (int i = 0; i < data.size(); ++i) {
        data[i] = i / items_per_class; // Assign class based on index
    }
    // Correct assignment for the last class due to integer division
    for (int i = items_per_class * max_classes; i < data.size(); ++i) {
        data[i] = max_classes - 1;
    }
}

// EXPECTED BEST FOLLOWING COMPRESSION METHOD: dict or rle
void valueBasedClassification(std::vector<int32_t>& data, int num_classes) {
    int32_t minVal = *std::min_element(data.begin(), data.end());
    int32_t maxVal = *std::max_element(data.begin(), data.end());
    int32_t range = maxVal - minVal;
    int32_t binSize = range / (num_classes - 1); // Determine the size of each bin, adjusting for inclusive range

    std::cout << range << ", " << binSize << std::endl;

    for (auto& val : data) {
        if (range == 0 || binSize == 0) // Avoid division by zero
            val = 0;
        else
            val = (val - minVal) / binSize; // Assign class based on value
        val = std::min(val, num_classes - 1); // Ensure the classification does not exceed num_classes - 1
    }
}

// EXPECTED BEST FOLLOWING COMPRESSION METHOD: for
void valueShift(std::vector<int32_t>& data, int32_t delta /* idea: 2^22 */) {
    for (int i = 0; i < data.size(); i++) {
        data[i] += delta;
    }
}