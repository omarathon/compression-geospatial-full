#pragma once

#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>

#include "generic_codecs.h"

// ── Run-Length Encoding (RLE) for uint8 ───────────────────────────────────────
// Stores pairs (value, encoded_run) as consecutive uint8 values, where
// encoded_run = run - 1.  A uint8 encoded_run therefore represents runs of
// 1–256, so every 256×256 = 65536-element block needs at most 256 entries for
// a fully-constant input (256 × 256 elements per entry = 65536).
//
// Supported range: [0, UINT8_MAX].  No zigzag needed (no signed residuals).
// Encoded size is variable; worst case is 2×length (all singletons).
class RLECodecU8 : public StatefulIntegerCodec<uint8_t> {
  std::vector<uint8_t> compressed_data;

 public:
  void EncodeArray(const uint8_t* in, const size_t length) override {
    if (length == 0) return;
    size_t i = 0;
    while (i < length) {
      uint8_t val = in[i];
      size_t run = 1;
      while (i + run < length && in[i + run] == val)
        ++run;
      // Split into chunks of at most 256; store each as (val, chunk-1)
      size_t remaining = run;
      while (remaining > 0) {
        size_t chunk = remaining > 256 ? 256 : remaining;
        compressed_data.push_back(val);
        compressed_data.push_back(static_cast<uint8_t>(chunk - 1));
        remaining -= chunk;
      }
      i += run;
    }
    compressed_data.shrink_to_fit();
  }

  void DecodeArray(uint8_t* out, const size_t length) override {
    size_t out_idx = 0;
    for (size_t i = 0; i + 1 < compressed_data.size(); i += 2) {
      uint8_t val = compressed_data[i];
      size_t  run = static_cast<size_t>(compressed_data[i + 1]) + 1;
      std::fill_n(out + out_idx, run, val);
      out_idx += run;
    }
  }

  std::size_t EncodedNumValues() override { return compressed_data.size(); }
  std::size_t EncodedSizeValue() override { return sizeof(uint8_t); }
  std::string name() const override { return "custom_rle_unvec_u8"; }
  std::size_t GetOverflowSize(size_t) const override { return 0; }

  StatefulIntegerCodec<uint8_t>* CloneFresh() const override {
    return new RLECodecU8();
  }

  void AllocEncoded(const uint8_t*, size_t length) override {
    // Variable output size; reserve worst-case (all singletons = 2×length).
    // EncodeArray uses push_back so no resize needed.
    compressed_data.clear();
    compressed_data.reserve(2 * length);
  }

  void clear() override {
    compressed_data.clear();
    compressed_data.shrink_to_fit();
  }

  std::vector<uint8_t>& GetEncoded() override { return compressed_data; }
};
