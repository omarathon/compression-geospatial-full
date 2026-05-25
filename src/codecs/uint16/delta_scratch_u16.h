#pragma once
#include <cstdint>

// Shared scratch buffer for the delta+zigzag encode pre-pass used by all
// fused-delta uint16 codecs. 256×256 covers the maximum supported block size
// (131072 bytes = 128 KB per thread). thread_local so parallel encodes from
// different threads don't race; zero heap allocation, zero fragmentation.
static thread_local uint16_t s_delta_scratch[256 * 256];
