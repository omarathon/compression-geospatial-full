#pragma once
#include <cstdint>
#include <vector>

// Aggregate-sum implementation selector for the fused FoR codecs (128 & 256).
// The two implementations differ in port pressure, not in result (when valid):
//   kUnpack: 2× unpacklo/hi widen vs zero + add (port 5). UNSIGNED → always
//            correct for the full uint16 range [0, 65535].
//   kMadd:   1× madd_epi16(OutReg, set1(1)) (port 0/1). ~1.5× faster kernel.
//
// CONSTRAINT — when kMadd does NOT work:
//   madd_epi16 is a SIGNED 16×16→32 multiply-add. It reads each lane as int16,
//   so it is correct ONLY when every DECODED VALUE is < 2^15, i.e. in
//   [0, 32767]. Any value >= 32768 (0x8000) has bit 15 set, is read as
//   negative, and CORRUPTS the fused sum. The bound is on the decoded value
//   (residual + anchor = the original datum), NOT on the residual or the
//   per-block bit width b — a low-b block can still be unsafe if its anchor
//   lifts values to >= 32768.
//
//   Enforcement: the encoder ORs all input values and stores a 1-byte
//   `madd_safe = (orall < 0x8000)` flag. At decode, kMadd is used only when
//   `madd_safe` holds; otherwise it transparently falls back to kUnpack. So
//   selecting kMadd is never WRONG, but on data with any value >= 32768 it
//   silently runs the (slower) kUnpack kernel — you get correctness, not speed.
enum class FusedAggImpl { kUnpack = 0, kMadd = 1 };

// Shared scratch buffer for the delta+zigzag encode pre-pass used by all
// fused-delta uint16 codecs. 256×256 covers the maximum supported block size
// (131072 bytes = 128 KB per thread). thread_local so parallel encodes from
// different threads don't race; zero heap allocation, zero fragmentation.
static thread_local uint16_t s_delta_scratch[256 * 256];

// Per-block anchor scratch for FoR codecs. Covers up to 2048 blocks (= max for
// a 65,536-element inner block at forWindowSize=32). thread_local so
// parallel encodes from different threads don't race.
static thread_local uint16_t s_anchor_scratch[2048];

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

// Shared scratch buffer for FastPFor codec output (uint32_t words).
// FastPFor's worst-case output for 65,536 uint16 elements at b=16 is
// ~33,027 uint32 words (~132 KB), well within 64 K uint32 = 256 KB.
// Same scratch-then-assign discipline as GetPackScratch().
inline std::vector<uint32_t>& GetFastPForScratch() {
  thread_local std::vector<uint32_t> scratch(64 * 1024);
  return scratch;
}
