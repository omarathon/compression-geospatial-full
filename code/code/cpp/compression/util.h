#include <vector>
#include <string>
#include <sstream>
#include <numeric>

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

int32_t avg(const std::vector<int32_t>& data) {
    if (data.empty()) return 0; // Avoid division by zero

    int64_t sum = std::accumulate(data.begin(), data.end(), int64_t(0));
    return static_cast<int32_t>(sum / data.size());
}