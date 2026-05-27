#pragma once
#include <cstdint>
#include <vector>

// Shared scratch buffer for the delta+zigzag encode pre-pass used by all
// fused-delta uint16 codecs. 256×256 covers the maximum supported block size
// (131072 bytes = 128 KB per thread). thread_local so parallel encodes from
// different threads don't race; zero heap allocation, zero fragmentation.
static thread_local uint16_t s_delta_scratch[256 * 256];

// Shared scratch buffer for the bit-packed encoder output of SimdComp /
// SimdComp-FoR codecs. Codecs encode into this buffer first, then
// `compressed.assign(...)` exactly `actual_size` bytes into the per-codec
// `compressed` vector. This avoids the worst-case AllocEncoded → shrink_to_fit
// cycle that leaves glibc's freelist holding ~128 KB per codec instance.
//
// Function-local thread_local: one allocation per thread, lazily constructed
// on first call, deallocated at thread exit. Sized at 256 KB ≈ 2× the worst-
// case bit-pack of a 65,536-element block at b=16 (~132 KB).
inline std::vector<uint8_t>& GetPackScratch() {
  thread_local std::vector<uint8_t> scratch(256 * 1024);
  return scratch;
}
