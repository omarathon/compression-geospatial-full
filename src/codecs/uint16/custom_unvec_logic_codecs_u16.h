#pragma once

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <cstring>
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
// needed.
// Supported range: [0, UINT16_MAX].
//
// separate_metadata=false (default): anchors interleaved with residuals in one
//   stream. Layout per window: [min, res_0, …, res_{w-1}]. Total = N + N/w.
//
// separate_metadata=true: GetEncoded() returns residuals only (length values).
//   Anchors stored in metadata_ (N/w values) and held in the object — NOT fed
//   to the downstream physical codec. Use this to measure how much the anchor
//   contamination hurts the physical codec's CR. EncodedNumValues() counts
//   both so standalone CR measurements remain honest.
class FORCodecU16 : public StatefulIntegerCodec<uint16_t> {
  std::vector<uint16_t> compressed_data;  // residuals (always)
  std::vector<uint16_t> metadata_;         // anchors (separate_metadata_ only)
  size_t window_;
  bool separate_metadata_;
 public:
  explicit FORCodecU16(size_t window = 0, bool separate_metadata = false)
      : window_(window), separate_metadata_(separate_metadata) {}

  void EncodeArray(const uint16_t* in, const size_t length) override {
    if (length == 0) return;
    const size_t w = (window_ == 0 || window_ >= length) ? length : window_;
    if (separate_metadata_) {
      size_t rpos = 0, mpos = 0;
      for (size_t start = 0; start < length; start += w) {
        const size_t end = std::min(start + w, length);
        const uint16_t ref = *std::min_element(in + start, in + end);
        metadata_[mpos++] = ref;
        for (size_t i = start; i < end; ++i)
          compressed_data[rpos++] = static_cast<uint16_t>(in[i] - ref);
      }
    } else {
      size_t pos = 0;
      for (size_t start = 0; start < length; start += w) {
        const size_t end = std::min(start + w, length);
        const uint16_t ref = *std::min_element(in + start, in + end);
        compressed_data[pos++] = ref;
        for (size_t i = start; i < end; ++i)
          compressed_data[pos++] = static_cast<uint16_t>(in[i] - ref);
      }
    }
  }
  void DecodeArray(uint16_t* out, const size_t length) override {
    if (length == 0) return;
    const size_t w = (window_ == 0 || window_ >= length) ? length : window_;
    if (separate_metadata_) {
      size_t rpos = 0, mpos = 0;
      for (size_t start = 0; start < length; start += w) {
        const size_t end = std::min(start + w, length);
        const uint16_t ref = metadata_[mpos++];
        for (size_t i = start; i < end; ++i)
          out[i] = static_cast<uint16_t>(compressed_data[rpos++] + ref);
      }
    } else {
      size_t pos = 0;
      for (size_t start = 0; start < length; start += w) {
        const size_t end = std::min(start + w, length);
        const uint16_t ref = compressed_data[pos++];
        for (size_t i = start; i < end; ++i)
          out[i] = static_cast<uint16_t>(compressed_data[pos++] + ref);
      }
    }
  }
  std::size_t EncodedNumValues() override {
    return compressed_data.size() + metadata_.size();
  }
  std::size_t EncodedSizeValue() override { return sizeof(uint16_t); }
  std::string name() const override {
    std::string base = "custom_for_unvec_u16_w" +
                       (window_ == 0 ? "full" : std::to_string(window_));
    return separate_metadata_ ? base + "_sep" : base;
  }
  std::size_t GetOverflowSize(size_t) const override { return 0; }
  StatefulIntegerCodec<uint16_t>* CloneFresh() const override {
    return new FORCodecU16(window_, separate_metadata_);
  }
  void AllocEncoded(const uint16_t*, size_t length) override {
    const size_t w = (window_ == 0 || window_ >= length) ? length : window_;
    const size_t num_windows = (length + w - 1) / w;
    if (separate_metadata_) {
      compressed_data.resize(length);
      metadata_.resize(num_windows);
    } else {
      compressed_data.resize(length + num_windows);
    }
  }
  void clear() override {
    compressed_data.clear(); compressed_data.shrink_to_fit();
    metadata_.clear(); metadata_.shrink_to_fit();
  }
  std::vector<uint16_t>& GetEncoded() override { return compressed_data; }
};

// ── Frame of Reference (FOR) — Hierarchical ──────────────────────────────────
// Two-level FoR: one global anchor per global_window_ elements, plus per-
// local_window_ anchor deltas (local_min − global_min, always ≥ 0) bitpacked
// at b_local bits (ceil(log2(max_delta + 1))). Element residuals are stored as
// (value − local_min).
//
// Constraint: local_window_ must divide global_window_.
//
// Layout (flat vector<uint16_t>):
//   [b_local : 1]
//   [global_mins : num_gw]      num_gw = ceil(length / global_window_)
//   [packed_local_deltas : pwords]  pwords = ceil(total_local * b_local / 16)
//   [residuals : length]
//
// With local_window_=8 and Landsat data, b_local is typically 2–4 bits,
// reducing anchor overhead from 1/8 (12.5%) to ~1/64–1/32 (1.5–3%).
class FORHierarchicalCodecU16 : public StatefulIntegerCodec<uint16_t> {
  std::vector<uint16_t> compressed_data;  // residuals (always)
  std::vector<uint16_t> metadata_;         // [b_local, global_mins, packed_deltas] (separate_metadata_ only)
  size_t global_window_;
  size_t local_window_;
  bool separate_metadata_;

  // Scalar bitpack helpers (no SIMD dependency).
  static size_t packed_u16_words(size_t n, uint32_t b) {
    // ceil(n * b / 8) bytes, rounded up to uint16 boundary
    return ((n * static_cast<size_t>(b) + 7) / 8 + 1) / 2;
  }
  static void pack_u16(const uint16_t* in, size_t n, uint32_t b, uint8_t* out) {
    if (b == 0) return;
    const size_t nbytes = (n * static_cast<size_t>(b) + 7) / 8;
    std::memset(out, 0, nbytes);
    for (size_t i = 0; i < n; ++i) {
      const uint32_t v = static_cast<uint32_t>(in[i]) & ((1u << b) - 1u);
      const size_t bit_pos = i * static_cast<size_t>(b);
      const uint64_t shifted = static_cast<uint64_t>(v) << (bit_pos % 8);
      uint8_t* p = out + bit_pos / 8;
      const size_t nb = (bit_pos % 8 + b + 7) / 8;
      uint64_t tmp = shifted;
      for (size_t j = 0; j < nb; ++j) { p[j] |= static_cast<uint8_t>(tmp); tmp >>= 8; }
    }
  }
  static void unpack_u16(const uint8_t* in, size_t n, uint32_t b, uint16_t* out) {
    if (b == 0) { std::memset(out, 0, n * sizeof(uint16_t)); return; }
    const uint32_t mask = (b >= 32) ? 0xFFFFFFFFu : ((1u << b) - 1u);
    for (size_t i = 0; i < n; ++i) {
      const size_t bit_pos = i * static_cast<size_t>(b);
      const size_t byte_pos = bit_pos / 8;
      const uint32_t shift = static_cast<uint32_t>(bit_pos % 8);
      uint32_t raw = 0;
      const size_t nb = (shift + b + 7) / 8;
      for (size_t j = 0; j < nb; ++j)
        raw |= static_cast<uint32_t>(in[byte_pos + j]) << (j * 8);
      out[i] = static_cast<uint16_t>((raw >> shift) & mask);
    }
  }

  // Effective window sizes (clamp to array length).
  size_t eff_gw(size_t length) const {
    return (global_window_ == 0 || global_window_ >= length) ? length : global_window_;
  }
  size_t eff_lw(size_t gw) const {
    return (local_window_ == 0 || local_window_ >= gw) ? gw : local_window_;
  }

 public:
  FORHierarchicalCodecU16(size_t global_window, size_t local_window,
                          bool separate_metadata = false)
      : global_window_(global_window), local_window_(local_window),
        separate_metadata_(separate_metadata) {}

  // Shared encode pass: computes global_mins, local_deltas, b_local.
  // Writes residuals into `compressed_data` (always starting at offset 0).
  // When separate_metadata_=false, also writes [b_local, global_mins,
  // packed_deltas] before the residuals (current mixed layout).
  // When separate_metadata_=true, writes those into `metadata_` instead,
  // keeping `compressed_data` as pure residuals for the physical codec.
  void EncodeArray(const uint16_t* in, const size_t length) override {
    if (length == 0) return;
    const size_t gw = eff_gw(length);
    const size_t lw = eff_lw(gw);
    assert(gw % lw == 0);
    const size_t num_gw = (length + gw - 1) / gw;
    const size_t total_local = (length + lw - 1) / lw;

    // Pass 1: compute global_mins and local_deltas; find max_delta for b_local.
    std::vector<uint16_t> global_mins(num_gw);
    std::vector<uint16_t> local_deltas(total_local);
    uint16_t max_delta = 0;
    for (size_t g = 0; g < num_gw; ++g) {
      const size_t gstart = g * gw, gend = std::min(gstart + gw, length);
      const uint16_t gmin = *std::min_element(in + gstart, in + gend);
      global_mins[g] = gmin;
      for (size_t lstart = gstart; lstart < gend; lstart += lw) {
        const size_t lend = std::min(lstart + lw, gend);
        const uint16_t lmin = *std::min_element(in + lstart, in + lend);
        const uint16_t delta = static_cast<uint16_t>(lmin - gmin);
        local_deltas[lstart / lw] = delta;
        if (delta > max_delta) max_delta = delta;
      }
    }

    uint32_t b_local = 0;
    for (uint16_t tmp = max_delta; tmp > 0; tmp >>= 1) ++b_local;
    const size_t pwords = packed_u16_words(total_local, b_local);

    // Helper: write header + packed deltas into a destination buffer.
    auto write_meta = [&](std::vector<uint16_t>& dest, size_t hdr_off) {
      dest[hdr_off] = static_cast<uint16_t>(b_local);
      for (size_t g = 0; g < num_gw; ++g) dest[hdr_off + 1 + g] = global_mins[g];
      if (pwords > 0) {
        uint8_t* pbuf = reinterpret_cast<uint8_t*>(dest.data() + hdr_off + 1 + num_gw);
        pack_u16(local_deltas.data(), total_local, b_local, pbuf);
      }
    };

    size_t res_off;
    if (separate_metadata_) {
      // metadata_ = [b_local, global_mins..., packed_deltas...]
      write_meta(metadata_, 0);
      metadata_.resize(1 + num_gw + pwords);  // shrink from worst-case alloc
      res_off = 0;
    } else {
      write_meta(compressed_data, 0);
      res_off = 1 + num_gw + pwords;
    }

    // Pass 2: write residuals into compressed_data starting at res_off.
    size_t pos = res_off;
    for (size_t g = 0; g < num_gw; ++g) {
      const size_t gstart = g * gw, gend = std::min(gstart + gw, length);
      const uint16_t gmin = global_mins[g];
      for (size_t lstart = gstart; lstart < gend; lstart += lw) {
        const size_t lend = std::min(lstart + lw, gend);
        const uint16_t lmin = static_cast<uint16_t>(gmin + local_deltas[lstart / lw]);
        for (size_t i = lstart; i < lend; ++i)
          compressed_data[pos++] = static_cast<uint16_t>(in[i] - lmin);
      }
    }
    compressed_data.resize(res_off + length);
  }

  void DecodeArray(uint16_t* out, const size_t length) override {
    if (length == 0) return;
    const size_t gw = eff_gw(length);
    const size_t lw = eff_lw(gw);
    const size_t num_gw = (length + gw - 1) / gw;
    const size_t total_local = (length + lw - 1) / lw;

    // Read header from whichever buffer holds the metadata.
    const std::vector<uint16_t>& meta = separate_metadata_ ? metadata_ : compressed_data;
    const uint32_t b_local = meta[0];
    const uint16_t* global_mins_ptr = meta.data() + 1;
    const size_t pwords = packed_u16_words(total_local, b_local);

    std::vector<uint16_t> local_deltas(total_local);
    if (pwords > 0) {
      const uint8_t* pbuf =
          reinterpret_cast<const uint8_t*>(meta.data() + 1 + num_gw);
      unpack_u16(pbuf, total_local, b_local, local_deltas.data());
    }

    const size_t res_off = separate_metadata_ ? 0 : (1 + num_gw + pwords);
    size_t pos = res_off;
    for (size_t g = 0; g < num_gw; ++g) {
      const size_t gstart = g * gw, gend = std::min(gstart + gw, length);
      const uint16_t gmin = global_mins_ptr[g];
      for (size_t lstart = gstart; lstart < gend; lstart += lw) {
        const size_t lend = std::min(lstart + lw, gend);
        const uint16_t lmin = static_cast<uint16_t>(gmin + local_deltas[lstart / lw]);
        for (size_t i = lstart; i < lend; ++i)
          out[i] = static_cast<uint16_t>(compressed_data[pos++] + lmin);
      }
    }
  }

  std::size_t EncodedNumValues() override {
    return compressed_data.size() + metadata_.size();
  }
  std::size_t EncodedSizeValue() override { return sizeof(uint16_t); }
  std::string name() const override {
    std::string base = "custom_for_hier_unvec_u16_g" + std::to_string(global_window_) +
                       "_l" + std::to_string(local_window_);
    return separate_metadata_ ? base + "_sep" : base;
  }
  std::size_t GetOverflowSize(size_t) const override { return 0; }
  StatefulIntegerCodec<uint16_t>* CloneFresh() const override {
    return new FORHierarchicalCodecU16(global_window_, local_window_, separate_metadata_);
  }
  void AllocEncoded(const uint16_t*, size_t length) override {
    const size_t gw = eff_gw(length);
    const size_t lw = eff_lw(gw);
    const size_t num_gw = (length + gw - 1) / gw;
    const size_t total_local = (length + lw - 1) / lw;
    if (separate_metadata_) {
      compressed_data.resize(length);
      // Worst case metadata: b_local=16 → pwords=total_local
      metadata_.resize(1 + num_gw + total_local);
    } else {
      compressed_data.resize(1 + num_gw + total_local + length);
    }
  }
  void clear() override {
    compressed_data.clear(); compressed_data.shrink_to_fit();
    metadata_.clear(); metadata_.shrink_to_fit();
  }
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
