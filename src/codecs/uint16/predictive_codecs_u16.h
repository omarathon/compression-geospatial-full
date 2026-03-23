#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

#include "generic_codecs.h"

// ── Helpers ─────────────────────────────────────────────────────────────────

static inline uint16_t ZigzagEnc16(uint16_t d) {
  return (d << 1) ^ static_cast<uint16_t>(static_cast<int16_t>(d) >> 15);
}

static inline uint16_t ZigzagDec16(uint16_t z) {
  return (z >> 1) ^ static_cast<uint16_t>(-static_cast<uint16_t>(z & 1));
}

// ── Double Delta ────────────────────────────────────────────────────────────
// Applies delta twice (without intermediate zigzag), then zigzag encodes.
// For linearly varying data, second delta → 0.

class DoubleDeltaCodecU16 : public StatefulIntegerCodec<uint16_t> {
  std::vector<uint16_t> compressed_data;

 public:
  void EncodeArray(const uint16_t* in, const size_t length) override {
    if (length == 0) return;

    // First pass: raw deltas (no zigzag)
    std::vector<uint16_t> d1(length);
    d1[0] = in[0];
    for (size_t i = 1; i < length; ++i)
      d1[i] = in[i] - in[i - 1];

    // Second pass: delta of deltas, then zigzag
    compressed_data[0] = ZigzagEnc16(d1[0]);
    for (size_t i = 1; i < length; ++i)
      compressed_data[i] = ZigzagEnc16(d1[i] - d1[i - 1]);
  }

  void DecodeArray(uint16_t* out, const size_t length) override {
    if (length == 0) return;

    // Undo second delta (prefix sum → d1)
    std::vector<uint16_t> d1(length);
    d1[0] = ZigzagDec16(compressed_data[0]);
    for (size_t i = 1; i < length; ++i)
      d1[i] = d1[i - 1] + ZigzagDec16(compressed_data[i]);

    // Undo first delta (prefix sum → original)
    out[0] = d1[0];
    for (size_t i = 1; i < length; ++i)
      out[i] = out[i - 1] + d1[i];
  }

  std::size_t EncodedNumValues() override { return compressed_data.size(); }
  std::size_t EncodedSizeValue() override { return sizeof(uint16_t); }
  std::string name() const override { return "custom_doubledelta"; }
  std::size_t GetOverflowSize(size_t) const override { return 0; }

  StatefulIntegerCodec<uint16_t>* CloneFresh() const override {
    return new DoubleDeltaCodecU16();
  }

  void AllocEncoded(const uint16_t*, size_t length) override {
    compressed_data.resize(length);
  }

  void clear() override {
    compressed_data.clear();
    compressed_data.shrink_to_fit();
  }

  std::vector<uint16_t>& GetEncoded() override { return compressed_data; }
};

// ── XOR Delta ───────────────────────────────────────────────────────────────
// xor[i] = x[i] ^ x[i-1]. No zigzag needed — XOR of similar values has
// leading zeros which bit-packing exploits directly.

class XorDeltaCodecU16 : public StatefulIntegerCodec<uint16_t> {
  std::vector<uint16_t> compressed_data;

 public:
  void EncodeArray(const uint16_t* in, const size_t length) override {
    if (length == 0) return;
    compressed_data[0] = in[0];
    for (size_t i = 1; i < length; ++i)
      compressed_data[i] = in[i] ^ in[i - 1];
  }

  void DecodeArray(uint16_t* out, const size_t length) override {
    if (length == 0) return;
    out[0] = compressed_data[0];
    for (size_t i = 1; i < length; ++i)
      out[i] = out[i - 1] ^ compressed_data[i];
  }

  std::size_t EncodedNumValues() override { return compressed_data.size(); }
  std::size_t EncodedSizeValue() override { return sizeof(uint16_t); }
  std::string name() const override { return "custom_xordelta"; }
  std::size_t GetOverflowSize(size_t) const override { return 0; }

  StatefulIntegerCodec<uint16_t>* CloneFresh() const override {
    return new XorDeltaCodecU16();
  }

  void AllocEncoded(const uint16_t*, size_t length) override {
    compressed_data.resize(length);
  }

  void clear() override {
    compressed_data.clear();
    compressed_data.shrink_to_fit();
  }

  std::vector<uint16_t>& GetEncoded() override { return compressed_data; }
};

// ── Byte Shuffle ────────────────────────────────────────────────────────────
// Deinterleaves low and high bytes: [v0,v1,...] → [lo0,lo1,...,hi0,hi1,...]
// Same encoded size; grouping similar bytes improves downstream bit-packing.

class ByteShuffleCodecU16 : public StatefulIntegerCodec<uint16_t> {
  std::vector<uint16_t> compressed_data;

 public:
  void EncodeArray(const uint16_t* in, const size_t length) override {
    if (length == 0) return;
    const uint8_t* bin = reinterpret_cast<const uint8_t*>(in);
    uint8_t* bout = reinterpret_cast<uint8_t*>(compressed_data.data());
    for (size_t i = 0; i < length; ++i) {
      bout[i] = bin[2 * i];              // low bytes first
      bout[length + i] = bin[2 * i + 1]; // high bytes second
    }
  }

  void DecodeArray(uint16_t* out, const size_t length) override {
    if (length == 0) return;
    const uint8_t* bin =
        reinterpret_cast<const uint8_t*>(compressed_data.data());
    uint8_t* bout = reinterpret_cast<uint8_t*>(out);
    for (size_t i = 0; i < length; ++i) {
      bout[2 * i] = bin[i];
      bout[2 * i + 1] = bin[length + i];
    }
  }

  std::size_t EncodedNumValues() override { return compressed_data.size(); }
  std::size_t EncodedSizeValue() override { return sizeof(uint16_t); }
  std::string name() const override { return "custom_byteshuffle"; }
  std::size_t GetOverflowSize(size_t) const override { return 0; }

  StatefulIntegerCodec<uint16_t>* CloneFresh() const override {
    return new ByteShuffleCodecU16();
  }

  void AllocEncoded(const uint16_t*, size_t length) override {
    compressed_data.resize(length);
  }

  void clear() override {
    compressed_data.clear();
    compressed_data.shrink_to_fit();
  }

  std::vector<uint16_t>& GetEncoded() override { return compressed_data; }
};

// ── 2D Predictors ───────────────────────────────────────────────────────────
// All operate on a square block (stride = sqrt(length)).
// Boundary convention: missing neighbors (L, U, UL) treated as 0.
// Residuals are zigzag-encoded.

// Helper to get stride from length, asserting square block.
static inline int BlockStride(size_t length) {
  int s = static_cast<int>(std::sqrt(static_cast<double>(length)));
  return s;
}

// r = x - U  (vertical delta)
class PredUpCodecU16 : public StatefulIntegerCodec<uint16_t> {
  std::vector<uint16_t> compressed_data;

 public:
  void EncodeArray(const uint16_t* in, const size_t length) override {
    if (length == 0) return;
    int stride = BlockStride(length);
    // First row: predict from 0
    for (int i = 0; i < stride; ++i)
      compressed_data[i] = ZigzagEnc16(in[i]);
    // Remaining rows: predict from above
    for (size_t i = stride; i < length; ++i)
      compressed_data[i] = ZigzagEnc16(in[i] - in[i - stride]);
  }

  void DecodeArray(uint16_t* out, const size_t length) override {
    if (length == 0) return;
    int stride = BlockStride(length);
    for (int i = 0; i < stride; ++i)
      out[i] = ZigzagDec16(compressed_data[i]);
    for (size_t i = stride; i < length; ++i)
      out[i] = out[i - stride] + ZigzagDec16(compressed_data[i]);
  }

  std::size_t EncodedNumValues() override { return compressed_data.size(); }
  std::size_t EncodedSizeValue() override { return sizeof(uint16_t); }
  std::string name() const override { return "custom_pred_up"; }
  std::size_t GetOverflowSize(size_t) const override { return 0; }

  StatefulIntegerCodec<uint16_t>* CloneFresh() const override {
    return new PredUpCodecU16();
  }

  void AllocEncoded(const uint16_t*, size_t length) override {
    compressed_data.resize(length);
  }

  void clear() override {
    compressed_data.clear();
    compressed_data.shrink_to_fit();
  }

  std::vector<uint16_t>& GetEncoded() override { return compressed_data; }
};

// r = x - ((L + U) >> 1)  (average predictor)
class PredAvgCodecU16 : public StatefulIntegerCodec<uint16_t> {
  std::vector<uint16_t> compressed_data;

 public:
  void EncodeArray(const uint16_t* in, const size_t length) override {
    if (length == 0) return;
    int stride = BlockStride(length);
    for (size_t i = 0; i < length; ++i) {
      int row = i / stride, col = i % stride;
      uint16_t L = (col > 0) ? in[i - 1] : 0;
      uint16_t U = (row > 0) ? in[i - stride] : 0;
      uint16_t pred = static_cast<uint16_t>((L + U) >> 1);
      compressed_data[i] = ZigzagEnc16(in[i] - pred);
    }
  }

  void DecodeArray(uint16_t* out, const size_t length) override {
    if (length == 0) return;
    int stride = BlockStride(length);
    for (size_t i = 0; i < length; ++i) {
      int row = i / stride, col = i % stride;
      uint16_t L = (col > 0) ? out[i - 1] : 0;
      uint16_t U = (row > 0) ? out[i - stride] : 0;
      uint16_t pred = static_cast<uint16_t>((L + U) >> 1);
      out[i] = pred + ZigzagDec16(compressed_data[i]);
    }
  }

  std::size_t EncodedNumValues() override { return compressed_data.size(); }
  std::size_t EncodedSizeValue() override { return sizeof(uint16_t); }
  std::string name() const override { return "custom_pred_avg"; }
  std::size_t GetOverflowSize(size_t) const override { return 0; }

  StatefulIntegerCodec<uint16_t>* CloneFresh() const override {
    return new PredAvgCodecU16();
  }

  void AllocEncoded(const uint16_t*, size_t length) override {
    compressed_data.resize(length);
  }

  void clear() override {
    compressed_data.clear();
    compressed_data.shrink_to_fit();
  }

  std::vector<uint16_t>& GetEncoded() override { return compressed_data; }
};

// r = x - (L + U - UL)  (Lorenzo / Paeth-linear predictor)
class PredLorenzoCodecU16 : public StatefulIntegerCodec<uint16_t> {
  std::vector<uint16_t> compressed_data;

 public:
  void EncodeArray(const uint16_t* in, const size_t length) override {
    if (length == 0) return;
    int stride = BlockStride(length);
    for (size_t i = 0; i < length; ++i) {
      int row = i / stride, col = i % stride;
      uint16_t L = (col > 0) ? in[i - 1] : 0;
      uint16_t U = (row > 0) ? in[i - stride] : 0;
      uint16_t UL = (row > 0 && col > 0) ? in[i - stride - 1] : 0;
      uint16_t pred = L + U - UL;
      compressed_data[i] = ZigzagEnc16(in[i] - pred);
    }
  }

  void DecodeArray(uint16_t* out, const size_t length) override {
    if (length == 0) return;
    int stride = BlockStride(length);
    for (size_t i = 0; i < length; ++i) {
      int row = i / stride, col = i % stride;
      uint16_t L = (col > 0) ? out[i - 1] : 0;
      uint16_t U = (row > 0) ? out[i - stride] : 0;
      uint16_t UL = (row > 0 && col > 0) ? out[i - stride - 1] : 0;
      uint16_t pred = L + U - UL;
      out[i] = pred + ZigzagDec16(compressed_data[i]);
    }
  }

  std::size_t EncodedNumValues() override { return compressed_data.size(); }
  std::size_t EncodedSizeValue() override { return sizeof(uint16_t); }
  std::string name() const override { return "custom_pred_lorenzo"; }
  std::size_t GetOverflowSize(size_t) const override { return 0; }

  StatefulIntegerCodec<uint16_t>* CloneFresh() const override {
    return new PredLorenzoCodecU16();
  }

  void AllocEncoded(const uint16_t*, size_t length) override {
    compressed_data.resize(length);
  }

  void clear() override {
    compressed_data.clear();
    compressed_data.shrink_to_fit();
  }

  std::vector<uint16_t>& GetEncoded() override { return compressed_data; }
};

// JPEG-LS style median-edge predictor:
// p = median(L, U, L + U - UL)
// r = x - p
class PredJPEGLSCodecU16 : public StatefulIntegerCodec<uint16_t> {
  std::vector<uint16_t> compressed_data;

  static uint16_t Median3(uint16_t a, uint16_t b, uint16_t c) {
    if (a > b) std::swap(a, b);
    if (b > c) std::swap(b, c);
    if (a > b) std::swap(a, b);
    return b;
  }

 public:
  void EncodeArray(const uint16_t* in, const size_t length) override {
    if (length == 0) return;
    int stride = BlockStride(length);
    for (size_t i = 0; i < length; ++i) {
      int row = i / stride, col = i % stride;
      uint16_t L = (col > 0) ? in[i - 1] : 0;
      uint16_t U = (row > 0) ? in[i - stride] : 0;
      uint16_t UL = (row > 0 && col > 0) ? in[i - stride - 1] : 0;
      uint16_t pred = Median3(L, U, L + U - UL);
      compressed_data[i] = ZigzagEnc16(in[i] - pred);
    }
  }

  void DecodeArray(uint16_t* out, const size_t length) override {
    if (length == 0) return;
    int stride = BlockStride(length);
    for (size_t i = 0; i < length; ++i) {
      int row = i / stride, col = i % stride;
      uint16_t L = (col > 0) ? out[i - 1] : 0;
      uint16_t U = (row > 0) ? out[i - stride] : 0;
      uint16_t UL = (row > 0 && col > 0) ? out[i - stride - 1] : 0;
      uint16_t pred = Median3(L, U, L + U - UL);
      out[i] = pred + ZigzagDec16(compressed_data[i]);
    }
  }

  std::size_t EncodedNumValues() override { return compressed_data.size(); }
  std::size_t EncodedSizeValue() override { return sizeof(uint16_t); }
  std::string name() const override { return "custom_pred_jpegls"; }
  std::size_t GetOverflowSize(size_t) const override { return 0; }

  StatefulIntegerCodec<uint16_t>* CloneFresh() const override {
    return new PredJPEGLSCodecU16();
  }

  void AllocEncoded(const uint16_t*, size_t length) override {
    compressed_data.resize(length);
  }

  void clear() override {
    compressed_data.clear();
    compressed_data.shrink_to_fit();
  }

  std::vector<uint16_t>& GetEncoded() override { return compressed_data; }
};
