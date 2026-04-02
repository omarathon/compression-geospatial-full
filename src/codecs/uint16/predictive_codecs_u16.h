#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

#include "generic_codecs.h"

// ── Helpers ──────────────────────────────────────────────────────────────────

// ZigzagEnc16/ZigzagDec16: bijectively map uint16 residuals (interpreted as
// signed) to non-negative uint16 values.  All encode/decode arithmetic uses
// modular uint16, so the full [0, UINT16_MAX] input range is supported without
// loss: (pred + ZigzagDec16(ZigzagEnc16(x - pred))) == x for all uint16 x.
static inline uint16_t ZigzagEnc16(uint16_t d) {
  return static_cast<uint16_t>((d << 1) ^ static_cast<uint16_t>(-(d >> 15)));
}
static inline uint16_t ZigzagDec16(uint16_t z) {
  return static_cast<uint16_t>((z >> 1) ^ static_cast<uint16_t>(-(z & 1)));
}

// BlockStride: 2D raster stride = floor(sqrt(length)).
static inline int BlockStride(size_t length) {
  return static_cast<int>(std::sqrt(static_cast<double>(length)));
}

// ── 2D neighbor helper ───────────────────────────────────────────────────────
// A = left, B = above, C = upper-left; boundary neighbors are 0.
// Predictions use int32 intermediates to avoid uint16 overflow mid-formula;
// the result is cast back to uint16 (wrapping), which is fine because the same
// formula is applied in both encode and decode.
static inline void GetNeighbors(const uint16_t* buf, size_t i, int stride,
                                 uint16_t& A, uint16_t& B, uint16_t& C) {
  int row = static_cast<int>(i) / stride, col = static_cast<int>(i) % stride;
  A  = (col > 0)            ? buf[i - 1]          : 0;
  B  = (row > 0)            ? buf[i - stride]     : 0;
  C  = (row > 0 && col > 0) ? buf[i - stride - 1] : 0;
}

static inline uint16_t JpegPredict(uint16_t A, uint16_t B, uint16_t C, int id) {
  int32_t a = A, b = B, c = C, p;
  switch (id) {
    case 1: p = a;                 break;
    case 2: p = b;                 break;
    case 3: p = c;                 break;
    case 4: p = a + b - c;         break;
    case 5: p = a + (b - c) / 2;  break;
    case 6: p = b + (a - c) / 2;  break;
    case 7: p = (a + b) / 2;      break;
    default: p = 0;                break;
  }
  return static_cast<uint16_t>(p);
}

static inline uint16_t JpegLSMed(uint16_t L, uint16_t U, uint16_t UL) {
  // JPEG-LS MED: median(L, U, L+U-UL), computed entirely in int32 so the
  // ordering of L+U-UL is not distorted by uint16 wrapping before comparison.
  // Equivalent clamp form: clip L+U-UL to [min(L,U), max(L,U)].
  int32_t l = L, u = U, x = l + u - (int32_t)UL;
  int32_t lo = std::min(l, u), hi = std::max(l, u);
  if (x < lo) return static_cast<uint16_t>(lo);
  if (x > hi) return static_cast<uint16_t>(hi);
  return static_cast<uint16_t>(x);
}

static inline uint16_t PaethPredict16(uint16_t a, uint16_t b, uint16_t c) {
  // PNG Paeth predictor — uses int32 to avoid overflow (a,b,c ≤ 65535)
  int32_t p  = (int32_t)a + (int32_t)b - (int32_t)c;
  int32_t pa = std::abs(p - (int32_t)a);
  int32_t pb = std::abs(p - (int32_t)b);
  int32_t pc = std::abs(p - (int32_t)c);
  if (pa <= pb && pa <= pc) return a;
  if (pb <= pc) return b;
  return c;
}

// ── Codec boilerplate ─────────────────────────────────────────────────────────
// All predictive codecs share identical boilerplate; only EncodeArray and
// DecodeArray differ.  Macro keeps things DRY.
#define PRED_CODEC_BOILERPLATE_U16(ClassName)                                   \
  std::size_t EncodedNumValues() override { return compressed_data.size(); }   \
  std::size_t EncodedSizeValue() override { return sizeof(uint16_t); }         \
  std::size_t GetOverflowSize(size_t) const override { return 0; }             \
  StatefulIntegerCodec<uint16_t>* CloneFresh() const override {                \
    return new ClassName();                                                      \
  }                                                                              \
  void AllocEncoded(const uint16_t*, size_t length) override {                 \
    compressed_data.resize(length);                                              \
  }                                                                              \
  void clear() override {                                                        \
    compressed_data.clear();                                                     \
    compressed_data.shrink_to_fit();                                             \
  }                                                                              \
  std::vector<uint16_t>& GetEncoded() override { return compressed_data; }

// ── Lossless JPEG predictor 1: A (left neighbor) ──────────────────────────────
// Residual = x - A, zigzag encoded. Supported range: [0, UINT16_MAX].
// uint16 subtraction wraps mod 65536; ZigzagEnc16 bijectively maps the result;
// ZigzagDec16 + uint16 addition reconstructs x exactly.
class JpegPred1CodecU16 : public StatefulIntegerCodec<uint16_t> {
  std::vector<uint16_t> compressed_data;
 public:
  void EncodeArray(const uint16_t* in, const size_t length) override {
    if (length == 0) return;
    int stride = BlockStride(length);
    for (size_t i = 0; i < length; ++i) {
      uint16_t A, B, C; GetNeighbors(in, i, stride, A, B, C);
      compressed_data[i] = ZigzagEnc16(in[i] - JpegPredict(A, B, C, 1));
    }
  }
  void DecodeArray(uint16_t* out, const size_t length) override {
    if (length == 0) return;
    int stride = BlockStride(length);
    for (size_t i = 0; i < length; ++i) {
      uint16_t A, B, C; GetNeighbors(out, i, stride, A, B, C);
      out[i] = JpegPredict(A, B, C, 1) + ZigzagDec16(compressed_data[i]);
    }
  }
  std::string name() const override { return "jpeg_pred1_A"; }
  PRED_CODEC_BOILERPLATE_U16(JpegPred1CodecU16)
};

// ── Lossless JPEG predictor 2: B (above neighbor) ────────────────────────────
// Residual = x - B, zigzag encoded. Supported range: [0, UINT16_MAX].
// Same modular arithmetic argument as predictor 1.
class JpegPred2CodecU16 : public StatefulIntegerCodec<uint16_t> {
  std::vector<uint16_t> compressed_data;
 public:
  void EncodeArray(const uint16_t* in, const size_t length) override {
    if (length == 0) return;
    int stride = BlockStride(length);
    for (size_t i = 0; i < length; ++i) {
      uint16_t A, B, C; GetNeighbors(in, i, stride, A, B, C);
      compressed_data[i] = ZigzagEnc16(in[i] - JpegPredict(A, B, C, 2));
    }
  }
  void DecodeArray(uint16_t* out, const size_t length) override {
    if (length == 0) return;
    int stride = BlockStride(length);
    for (size_t i = 0; i < length; ++i) {
      uint16_t A, B, C; GetNeighbors(out, i, stride, A, B, C);
      out[i] = JpegPredict(A, B, C, 2) + ZigzagDec16(compressed_data[i]);
    }
  }
  std::string name() const override { return "jpeg_pred2_B"; }
  PRED_CODEC_BOILERPLATE_U16(JpegPred2CodecU16)
};

// ── Lossless JPEG predictor 3: C (upper-left neighbor) ───────────────────────
// Residual = x - C, zigzag encoded. Supported range: [0, UINT16_MAX].
class JpegPred3CodecU16 : public StatefulIntegerCodec<uint16_t> {
  std::vector<uint16_t> compressed_data;
 public:
  void EncodeArray(const uint16_t* in, const size_t length) override {
    if (length == 0) return;
    int stride = BlockStride(length);
    for (size_t i = 0; i < length; ++i) {
      uint16_t A, B, C; GetNeighbors(in, i, stride, A, B, C);
      compressed_data[i] = ZigzagEnc16(in[i] - JpegPredict(A, B, C, 3));
    }
  }
  void DecodeArray(uint16_t* out, const size_t length) override {
    if (length == 0) return;
    int stride = BlockStride(length);
    for (size_t i = 0; i < length; ++i) {
      uint16_t A, B, C; GetNeighbors(out, i, stride, A, B, C);
      out[i] = JpegPredict(A, B, C, 3) + ZigzagDec16(compressed_data[i]);
    }
  }
  std::string name() const override { return "jpeg_pred3_C"; }
  PRED_CODEC_BOILERPLATE_U16(JpegPred3CodecU16)
};

// ── Lossless JPEG predictor 4: A + B − C ─────────────────────────────────────
// Residual = x - (A+B-C), zigzag encoded. Supported range: [0, UINT16_MAX].
// Prediction computed in int32 then truncated to uint16 (wrapping); encode and
// decode apply the identical formula so round-trip is exact.
class JpegPred4CodecU16 : public StatefulIntegerCodec<uint16_t> {
  std::vector<uint16_t> compressed_data;
 public:
  void EncodeArray(const uint16_t* in, const size_t length) override {
    if (length == 0) return;
    int stride = BlockStride(length);
    for (size_t i = 0; i < length; ++i) {
      uint16_t A, B, C; GetNeighbors(in, i, stride, A, B, C);
      compressed_data[i] = ZigzagEnc16(in[i] - JpegPredict(A, B, C, 4));
    }
  }
  void DecodeArray(uint16_t* out, const size_t length) override {
    if (length == 0) return;
    int stride = BlockStride(length);
    for (size_t i = 0; i < length; ++i) {
      uint16_t A, B, C; GetNeighbors(out, i, stride, A, B, C);
      out[i] = JpegPredict(A, B, C, 4) + ZigzagDec16(compressed_data[i]);
    }
  }
  std::string name() const override { return "jpeg_pred4_ApBmC"; }
  PRED_CODEC_BOILERPLATE_U16(JpegPred4CodecU16)
};

// ── Lossless JPEG predictor 5: A + (B − C)/2 ─────────────────────────────────
// Residual = x - (A+(B-C)/2), zigzag encoded. Supported range: [0, UINT16_MAX].
// (B-C) computed as signed int32 so division rounds toward zero (signed).
class JpegPred5CodecU16 : public StatefulIntegerCodec<uint16_t> {
  std::vector<uint16_t> compressed_data;
 public:
  void EncodeArray(const uint16_t* in, const size_t length) override {
    if (length == 0) return;
    int stride = BlockStride(length);
    for (size_t i = 0; i < length; ++i) {
      uint16_t A, B, C; GetNeighbors(in, i, stride, A, B, C);
      compressed_data[i] = ZigzagEnc16(in[i] - JpegPredict(A, B, C, 5));
    }
  }
  void DecodeArray(uint16_t* out, const size_t length) override {
    if (length == 0) return;
    int stride = BlockStride(length);
    for (size_t i = 0; i < length; ++i) {
      uint16_t A, B, C; GetNeighbors(out, i, stride, A, B, C);
      out[i] = JpegPredict(A, B, C, 5) + ZigzagDec16(compressed_data[i]);
    }
  }
  std::string name() const override { return "jpeg_pred5_Ap(BmC)d2"; }
  PRED_CODEC_BOILERPLATE_U16(JpegPred5CodecU16)
};

// ── Lossless JPEG predictor 6: B + (A − C)/2 ─────────────────────────────────
// Residual = x - (B+(A-C)/2), zigzag encoded. Supported range: [0, UINT16_MAX].
class JpegPred6CodecU16 : public StatefulIntegerCodec<uint16_t> {
  std::vector<uint16_t> compressed_data;
 public:
  void EncodeArray(const uint16_t* in, const size_t length) override {
    if (length == 0) return;
    int stride = BlockStride(length);
    for (size_t i = 0; i < length; ++i) {
      uint16_t A, B, C; GetNeighbors(in, i, stride, A, B, C);
      compressed_data[i] = ZigzagEnc16(in[i] - JpegPredict(A, B, C, 6));
    }
  }
  void DecodeArray(uint16_t* out, const size_t length) override {
    if (length == 0) return;
    int stride = BlockStride(length);
    for (size_t i = 0; i < length; ++i) {
      uint16_t A, B, C; GetNeighbors(out, i, stride, A, B, C);
      out[i] = JpegPredict(A, B, C, 6) + ZigzagDec16(compressed_data[i]);
    }
  }
  std::string name() const override { return "jpeg_pred6_Bp(AmC)d2"; }
  PRED_CODEC_BOILERPLATE_U16(JpegPred6CodecU16)
};

// ── Lossless JPEG predictor 7: (A + B)/2 ─────────────────────────────────────
// Residual = x - (A+B)/2, zigzag encoded. Supported range: [0, UINT16_MAX].
// (A+B) computed in int32; division truncates toward zero.
class JpegPred7CodecU16 : public StatefulIntegerCodec<uint16_t> {
  std::vector<uint16_t> compressed_data;
 public:
  void EncodeArray(const uint16_t* in, const size_t length) override {
    if (length == 0) return;
    int stride = BlockStride(length);
    for (size_t i = 0; i < length; ++i) {
      uint16_t A, B, C; GetNeighbors(in, i, stride, A, B, C);
      compressed_data[i] = ZigzagEnc16(in[i] - JpegPredict(A, B, C, 7));
    }
  }
  void DecodeArray(uint16_t* out, const size_t length) override {
    if (length == 0) return;
    int stride = BlockStride(length);
    for (size_t i = 0; i < length; ++i) {
      uint16_t A, B, C; GetNeighbors(out, i, stride, A, B, C);
      out[i] = JpegPredict(A, B, C, 7) + ZigzagDec16(compressed_data[i]);
    }
  }
  std::string name() const override { return "jpeg_pred7_ApBd2"; }
  PRED_CODEC_BOILERPLATE_U16(JpegPred7CodecU16)
};

// ── JPEG-LS predictor 0: MED (Median Edge Detector) ──────────────────────────
// pred = median(A, B, A+B-C). Residual = x - pred, zigzag encoded.
// Supported range: [0, UINT16_MAX]. Prediction computed in int32 for the
// Lorenzo term (A+B-C) then truncated to uint16; median selection is unsigned.
class JpegLSMedCodecU16 : public StatefulIntegerCodec<uint16_t> {
  std::vector<uint16_t> compressed_data;
 public:
  void EncodeArray(const uint16_t* in, const size_t length) override {
    if (length == 0) return;
    int stride = BlockStride(length);
    for (size_t i = 0; i < length; ++i) {
      uint16_t A, B, C; GetNeighbors(in, i, stride, A, B, C);
      compressed_data[i] = ZigzagEnc16(in[i] - JpegLSMed(A, B, C));
    }
  }
  void DecodeArray(uint16_t* out, const size_t length) override {
    if (length == 0) return;
    int stride = BlockStride(length);
    for (size_t i = 0; i < length; ++i) {
      uint16_t A, B, C; GetNeighbors(out, i, stride, A, B, C);
      out[i] = JpegLSMed(A, B, C) + ZigzagDec16(compressed_data[i]);
    }
  }
  std::string name() const override { return "jpegls_pred0_med"; }
  PRED_CODEC_BOILERPLATE_U16(JpegLSMedCodecU16)
};

// ── JPEG-LS predictor 1: Paeth (PNG) ─────────────────────────────────────────
// pred = Paeth(A, B, C) = argmin(|A|, |B|, |C|) of (p-a, p-b, p-c) where
// p = A+B-C.  Residual = x - pred, zigzag encoded.
// Supported range: [0, UINT16_MAX]. Paeth internals use int32 (p can exceed
// 65535); the chosen predictor value is always one of A, B, C ∈ uint16.
class PaethCodecU16 : public StatefulIntegerCodec<uint16_t> {
  std::vector<uint16_t> compressed_data;
 public:
  void EncodeArray(const uint16_t* in, const size_t length) override {
    if (length == 0) return;
    int stride = BlockStride(length);
    for (size_t i = 0; i < length; ++i) {
      uint16_t A, B, C; GetNeighbors(in, i, stride, A, B, C);
      compressed_data[i] = ZigzagEnc16(in[i] - PaethPredict16(A, B, C));
    }
  }
  void DecodeArray(uint16_t* out, const size_t length) override {
    if (length == 0) return;
    int stride = BlockStride(length);
    for (size_t i = 0; i < length; ++i) {
      uint16_t A, B, C; GetNeighbors(out, i, stride, A, B, C);
      out[i] = PaethPredict16(A, B, C) + ZigzagDec16(compressed_data[i]);
    }
  }
  std::string name() const override { return "jpegls_pred1_paeth"; }
  PRED_CODEC_BOILERPLATE_U16(PaethCodecU16)
};

#undef PRED_CODEC_BOILERPLATE_U16
