#include "remappings.h"
#include <iostream>

void test(const int N, bool output) {
    std::vector<int32_t> block;
    for (int i = 1; i <= N*N; i++) {
        block.push_back(i);
    }

    auto mortonOrderedBlock = remapToMortonOrder(block, N);
    auto zigzagOrderedBlock = remapToZigzagOrder(block, N);

    auto print = [&](std::vector<int32_t> &data) {
        for (int i = 0; i < N * N; ++i) {
            if (i % N == 0 && i != 0) std::cout << '\n';
            std::cout << data[i] << ' ';
        }
        std::cout << std::endl;
    };

    if (!output) return;

    std::cout << "in:" << std::endl;
    print(block);
    std::cout << "morton:" << std::endl;
    print(mortonOrderedBlock);
    std::cout << "zigzag:" << std::endl;
    print(zigzagOrderedBlock);
}

int main() {
    test(2, true);
    test(4, true);
    test(8, true);
    test(256, false); // test perf
}