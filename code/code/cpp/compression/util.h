#include <vector>
#include <string>
#include <sstream>
#include <numeric>
#include <cmath>

std::vector<std::string> parseCommaDelimited(std::string str) {
    std::stringstream ss(str);
    std::vector<std::string> result;
    while(ss.good())
    {
        std::string substr;
        std::getline(ss, substr, ',');
        result.push_back(substr);
    }
    return result;
}

std::vector<std::string> parseBarDelimited(std::string str) {
    std::stringstream ss(str);
    std::vector<std::string> result;
    while(ss.good())
    {
        std::string substr;
        std::getline(ss, substr, '|');
        result.push_back(substr);
    }
    return result;
}

int32_t avg(const std::vector<int32_t>& data) {
    if (data.empty()) return 0; // Avoid division by zero

    int64_t sum = std::accumulate(data.begin(), data.end(), int64_t(0));
    return static_cast<int32_t>(sum / data.size());
}

float mean(const std::vector<float>& values) {
    if (values.empty()) return 0;
    return std::accumulate(values.begin(), values.end(), 0.0) / values.size();
}

float variance(const std::vector<float>& values, float meanValue) {
    if (values.empty()) return 0;
    float sq_sum = std::accumulate(values.begin(), values.end(), 0.0,
        [meanValue](double a, double b) { return a + (b - meanValue) * (b - meanValue); });
    return sq_sum / values.size();
}

std::size_t sum(const std::vector<std::size_t>& values) {
    if (values.empty()) return 0;
    std::size_t result = 0;
    for (auto& value : values) {
        result += value;
    }
    return result;
}

float mean(const std::vector<std::size_t>& values) {
    if (values.empty()) return 0;
    return ((float)sum(values)) / ((float)values.size()); 
}

float variance(const std::vector<std::size_t>& values, float meanValue) {
    if (values.empty()) return 0;
    float sq_sum = std::accumulate(values.begin(), values.end(), 0.0,
        [meanValue](std::size_t a, std::size_t b) { return (float)a + ((float)b - meanValue) * ((float)b - meanValue); });
    return sq_sum / values.size();
}