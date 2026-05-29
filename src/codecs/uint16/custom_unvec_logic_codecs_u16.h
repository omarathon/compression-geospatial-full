#pragma once

#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>

// ZigzagEnc16/ZigzagDec16 are defined in predictive_codecs_u16.h,
// which is always included alongside this file via codec_collection_uint16.h.
#include "predictive_codecs_u16.h"

// ── Delta ─────────────────────────────────────────────────────────────────────
// delta[0] = in[0] (first value stored raw, no zigzag — it is non-negative).
// delta[i] = ZigzagEnc16(in[i] - in[i-1]) for i > 0.
// Supported range: [0, UINT16_MAX]. Differences use modular uint16 arithmetic;
// ZigzagEnc16 maps the residual bijectively so ZigzagDec16 + prefix-sum
// recovers every original value exactly regardless of input magnitude.
class DeltaCodecU16 : public StatefulIntegerCodec<uint16_t> {
  std::vector<uint16_t> compressed_data;
 public:
  void EncodeArray(const uint16_t* in, const size_t length) override {
    if (length == 0) return;
    compressed_data[0] = in[0];
    for (size_t i = 1; i < length; ++i)
      compressed_data[i] = ZigzagEnc16(static_cast<uint16_t>(in[i] - in[i - 1]));
  }
  void DecodeArray(uint16_t* out, const size_t length) override {
    if (length == 0) return;
    out[0] = compressed_data[0];
    for (size_t i = 1; i < length; ++i)
      out[i] = out[i - 1] + ZigzagDec16(compressed_data[i]);
  }
  std::size_t EncodedNumValues() override { return compressed_data.size(); }
  std::size_t EncodedSizeValue() override { return sizeof(uint16_t); }
  std::string name() const override { return "custom_delta_unvec_u16"; }
  std::size_t GetOverflowSize(size_t) const override { return 0; }
  StatefulIntegerCodec<uint16_t>* CloneFresh() const override { return new DeltaCodecU16(); }
  void AllocEncoded(const uint16_t*, size_t length) override { compressed_data.resize(length); }
  void clear() override { compressed_data.clear(); compressed_data.shrink_to_fit(); }
  std::vector<uint16_t>& GetEncoded() override { return compressed_data; }
};

// ── Double Delta ──────────────────────────────────────────────────────────────
// First pass: raw uint16 differences d1[i] = in[i] - in[i-1] (no zigzag).
// Second pass: ZigzagEnc16(d1[i] - d1[i-1]), i.e. delta-of-delta then zigzag.
// Supported range: [0, UINT16_MAX]. Both passes use modular uint16 arithmetic;
// zigzag is applied only after the second delta so negatives are mapped to
// non-negative values for the downstream physical compressor.
class DoubleDeltaCodecU16 : public StatefulIntegerCodec<uint16_t> {
  std::vector<uint16_t> compressed_data;
 public:
  void EncodeArray(const uint16_t* in, const size_t length) override {
    if (length == 0) return;
    // First delta (raw, no zigzag)
    std::vector<uint16_t> d1(length);
    d1[0] = in[0];
    for (size_t i = 1; i < length; ++i)
      d1[i] = static_cast<uint16_t>(in[i] - in[i - 1]);
    // Second delta then zigzag
    compressed_data[0] = d1[0];  // first value stored as first delta (raw)
    for (size_t i = 1; i < length; ++i)
      compressed_data[i] = ZigzagEnc16(static_cast<uint16_t>(d1[i] - d1[i - 1]));
  }
  void DecodeArray(uint16_t* out, const size_t length) override {
    if (length == 0) return;
    // Undo second delta (prefix sum)
    std::vector<uint16_t> d1(length);
    d1[0] = compressed_data[0];
    for (size_t i = 1; i < length; ++i)
      d1[i] = static_cast<uint16_t>(d1[i - 1] + ZigzagDec16(compressed_data[i]));
    // Undo first delta (prefix sum)
    out[0] = d1[0];
    for (size_t i = 1; i < length; ++i)
      out[i] = static_cast<uint16_t>(out[i - 1] + d1[i]);
  }
  std::size_t EncodedNumValues() override { return compressed_data.size(); }
  std::size_t EncodedSizeValue() override { return sizeof(uint16_t); }
  std::string name() const override { return "custom_doubledelta_unvec_u16"; }
  std::size_t GetOverflowSize(size_t) const override { return 0; }
  StatefulIntegerCodec<uint16_t>* CloneFresh() const override { return new DoubleDeltaCodecU16(); }
  void AllocEncoded(const uint16_t*, size_t length) override { compressed_data.resize(length); }
  void clear() override { compressed_data.clear(); compressed_data.shrink_to_fit(); }
  std::vector<uint16_t>& GetEncoded() override { return compressed_data; }
};

// ── Frame of Reference (FOR) ──────────────────────────────────────────────────
// Stores the minimum value as a reference, then encodes each element as
// (value - reference).  Offsets are always non-negative, so no zigzag is
// needed.  Layout: [reference, offset[0], offset[1], ..., offset[n-1]].
// Supported range: [0, UINT16_MAX]. The reference is the minimum, so every
// offset fits in [0, 65535] and is stored as uint16 without overflow.
// Layout per window: [min, res_0, res_1, ..., res_{w-1}]
// Last window may be shorter than w. Total stored values = num_windows + length.
class FORCodecU16 : public StatefulIntegerCodec<uint16_t> {
  std::vector<uint16_t> compressed_data;
  size_t window_;
 public:
  explicit FORCodecU16(size_t window = 0)
      : window_(window) {}  // 0 = whole array (original behaviour)

  void EncodeArray(const uint16_t* in, const size_t length) override {
    if (length == 0) return;
    size_t w = (window_ == 0 || window_ >= length) ? length : window_;
    size_t pos = 0;
    for (size_t start = 0; start < length; start += w) {
      size_t end = std::min(start + w, length);
      uint16_t ref = *std::min_element(in + start, in + end);
      compressed_data[pos++] = ref;
      for (size_t i = start; i < end; ++i)
        compressed_data[pos++] = static_cast<uint16_t>(in[i] - ref);
    }
  }
  void DecodeArray(uint16_t* out, const size_t length) override {
    if (length == 0) return;
    size_t w = (window_ == 0 || window_ >= length) ? length : window_;
    size_t pos = 0;
    for (size_t start = 0; start < length; start += w) {
      size_t end = std::min(start + w, length);
      uint16_t ref = compressed_data[pos++];
      for (size_t i = start; i < end; ++i)
        out[i] = static_cast<uint16_t>(compressed_data[pos++] + ref);
    }
  }
  std::size_t EncodedNumValues() override { return compressed_data.size(); }
  std::size_t EncodedSizeValue() override { return sizeof(uint16_t); }
  std::string name() const override {
    return "custom_for_unvec_u16_w" + (window_ == 0 ? "full" : std::to_string(window_));
  }
  std::size_t GetOverflowSize(size_t) const override { return 0; }
  StatefulIntegerCodec<uint16_t>* CloneFresh() const override { return new FORCodecU16(window_); }
  void AllocEncoded(const uint16_t*, size_t length) override {
    size_t w = (window_ == 0 || window_ >= length) ? length : window_;
    size_t num_windows = (length + w - 1) / w;
    compressed_data.resize(length + num_windows);
  }
  void clear() override { compressed_data.clear(); compressed_data.shrink_to_fit(); }
  std::vector<uint16_t>& GetEncoded() override { return compressed_data; }
};

// ── Run-Length Encoding (RLE) ─────────────────────────────────────────────────
// Stores pairs (value, encoded_run) where encoded_run = run - 1.
// This lets a uint16 encoded_run represent runs of 1–65536 (stored as 0–65535),
// so a full 256×256 = 65536-element constant block encodes as a single pair
// without splitting.  Runs exceeding 65536 are split into multiple pairs.
// Supported range: [0, UINT16_MAX]. No zigzag needed.
// Note: encoded size is variable (≤ 2*length in worst case).
class RLECodecU16 : public StatefulIntegerCodec<uint16_t> {
  std::vector<uint16_t> compressed_data;
 public:
  void EncodeArray(const uint16_t* in, const size_t length) override {
    if (length == 0) return;
    size_t i = 0;
    while (i < length) {
      uint16_t val = in[i];
      size_t run = 1;
      while (i + run < length && in[i + run] == val)
        ++run;
      // Split runs exceeding 65536; store each chunk as (chunk - 1)
      size_t remaining = run;
      while (remaining > 0) {
        size_t chunk = remaining > 65536 ? 65536 : remaining;
        compressed_data.push_back(val);
        compressed_data.push_back(static_cast<uint16_t>(chunk - 1));
        remaining -= chunk;
      }
      i += run;
    }
    compressed_data.shrink_to_fit();
  }
  void DecodeArray(uint16_t* out, const size_t length) override {
    size_t out_idx = 0;
    for (size_t i = 0; i + 1 < compressed_data.size(); i += 2) {
      uint16_t val = compressed_data[i];
      size_t   run = static_cast<size_t>(compressed_data[i + 1]) + 1;
      std::fill_n(out + out_idx, run, val);
      out_idx += run;
    }
  }
  std::size_t EncodedNumValues() override { return compressed_data.size(); }
  std::size_t EncodedSizeValue() override { return sizeof(uint16_t); }
  std::string name() const override { return "custom_rle_unvec_u16"; }
  std::size_t GetOverflowSize(size_t) const override { return 0; }
  StatefulIntegerCodec<uint16_t>* CloneFresh() const override { return new RLECodecU16(); }
  void AllocEncoded(const uint16_t*, size_t length) override {
    // Worst case: 2 values per element (all singletons); use reserve since
    // EncodeArray uses push_back (variable output size).
    compressed_data.clear();
    compressed_data.reserve(2 * length);
  }
  void clear() override { compressed_data.clear(); compressed_data.shrink_to_fit(); }
  std::vector<uint16_t>& GetEncoded() override { return compressed_data; }
};
