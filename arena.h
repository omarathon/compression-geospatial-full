#pragma once
#include <vector>
#include <cstddef>
#include <cstdint>
#include <algorithm>

struct Arena {
    std::vector<uint8_t> buffer;
    size_t used = 0;

    Arena(size_t sz) {
        buffer.reserve(sz);
    }

    // void reserve(size_t n) {
    //     buffer.reserve(used + n);
    // }

    uint8_t* alloc(size_t n, size_t& out_offset) {
        if (used + n > buffer.size()) {
            buffer.resize(std::max(buffer.size() * 2, used + n));
        }
        out_offset = used;
        used += n;
        return buffer.data() + out_offset;
    }

    void shrink_to_fit() {
        buffer.resize(used);
        buffer.shrink_to_fit();
    }
};