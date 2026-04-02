#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

#include "generic_codecs.h"

// ── Helpers ───────────────────────────────────────────────────────────────────

// Zigzag for int32: maps signed residuals to non-negative int32 values
// (stored as int32_t but semantically uint32).
// Max residual magnitude for inputs in [0, 2^26]: ~2^27 (predictors 4–7 can
// combine two neighbors minus a third, giving range [-2^26, 2^27]).
// ZigzagEnc32(±2^27) = 2^28, which fits in int32_t (max ~2.1×10^9).
static inline int32_t ZigzagEnc32Pred(int32_t d) {
  uint32_t u = static_cast<uint32_t>(d);
  return static_cast<int32_t>((u << 1) ^ (0u - (u >> 31)));
}
static inline int32_t ZigzagDec32Pred(int32_t z) {
  uint32_t u = static_cast<uint32_t>(z);
  return static_cast<int32_t>((u >> 1) ^ -(u & 1));
}

// BlockStride32: 2D stride = floor(sqrt(length)).
static inline int BlockStride32(size_t length) {
  return static_cast<int>(std::sqrt(static_cast<double>(length)));
}

// 2D neighbors: A = left, B = above, C = upper-left; 0 at boundaries.
static inline void GetNeighbors32(const int32_t* buf, size_t i, int stride,
                                   int32_t& A, int32_t& B, int32_t& C) {
  int row = static_cast<int>(i) / stride, col = static_cast<int>(i) % stride;
  A = (col > 0)            ? buf[i - 1]          : 0;
  B = (row > 0)            ? buf[i - stride]     : 0;
  C = (row > 0 && col > 0) ? buf[i - stride - 1] : 0;
}

static inline int32_t JpegPredict32(int32_t A, int32_t B, int32_t C, int id) {
  switch (id) {
    case 1: return A;
    case 2: return B;
    case 3: return C;
    case 4: return A + B - C;
    case 5: return A + (B - C) / 2;
    case 6: return B + (A - C) / 2;
    case 7: return (A + B) / 2;
    default: return 0;
  }
}

static inline int32_t JpegLSMed32(int32_t L, int32_t U, int32_t UL) {
  int32_t lor = L + U - UL;
  int32_t v[3] = {L, U, lor};
  if (v[0] > v[1]) std::swap(v[0], v[1]);
  if (v[1] > v[2]) std::swap(v[1], v[2]);
  if (v[0] > v[1]) std::swap(v[0], v[1]);
  return v[1];
}

static inline int32_t PaethPredict32(int32_t a, int32_t b, int32_t c) {
  // For inputs in [0, 2^26]: p = a+b-c ∈ [-2^26, 2^27]; all abs() values
  // fit in int32_t.
  int32_t p  = a + b - c;
  int32_t pa = std::abs(p - a);
  int32_t pb = std::abs(p - b);
  int32_t pc = std::abs(p - c);
  if (pa <= pb && pa <= pc) return a;
  if (pb <= pc) return b;
  return c;
}

// ── Codec boilerplate macro ────────────────────────────────────────────────────
#define PRED_CODEC_BOILERPLATE_I32(ClassName)                                   \
  std::size_t EncodedNumValues() override { return compressed_data.size(); }   \
  std::size_t EncodedSizeValue() override { return sizeof(int32_t); }          \
  std::size_t GetOverflowSize(size_t) const override { return 0; }             \
  StatefulIntegerCodec<int32_t>* CloneFresh() const override {                 \
    return new ClassName();                                                      \
  }                                                                              \
  void AllocEncoded(const int32_t*, size_t length) override {                  \
    compressed_data.resize(length);                                              \
  }                                                                              \
  void clear() override {                                                        \
    compressed_data.clear();                                                     \
    compressed_data.shrink_to_fit();                                             \
  }                                                                              \
  std::vector<int32_t>& GetEncoded() override { return compressed_data; }

// ── Predictor 1: A (left neighbor) ────────────────────────────────────────────
// Residual = x - A. Inputs in [0, 2^26]; residuals in [-2^26, 2^26].
// ZigzagEnc32Pred maps residuals to [0, 2^27], stored as non-negative int32_t.
// Boundary pixels (A=0) give residuals equal to x ∈ [0, 2^26]; zigzag maps
// these to even values in [0, 2^27].  Full round-trip: pred + ZigzagDec32(z).
class JpegPred1Codec : public StatefulIntegerCodec<int32_t> {
  std::vector<int32_t> compressed_data;
 public:
  void EncodeArray(const int32_t* in, const size_t length) override {
    if (length == 0) return;
    int stride = BlockStride32(length);
    for (size_t i = 0; i < length; ++i) {
      int32_t A, B, C; GetNeighbors32(in, i, stride, A, B, C);
      compressed_data[i] = ZigzagEnc32Pred(in[i] - JpegPredict32(A, B, C, 1));
    }
  }
  void DecodeArray(int32_t* out, const size_t length) override {
    if (length == 0) return;
    int stride = BlockStride32(length);
    for (size_t i = 0; i < length; ++i) {
      int32_t A, B, C; GetNeighbors32(out, i, stride, A, B, C);
      out[i] = JpegPredict32(A, B, C, 1) + ZigzagDec32Pred(compressed_data[i]);
    }
  }
  std::string name() const override { return "jpeg_pred1_A_i32"; }
  PRED_CODEC_BOILERPLATE_I32(JpegPred1Codec)
};

// ── Predictor 2: B (above neighbor) ───────────────────────────────────────────
// Residual = x - B. Inputs in [0, 2^26]; residuals in [-2^26, 2^26].
// Same range analysis as predictor 1.
class JpegPred2Codec : public StatefulIntegerCodec<int32_t> {
  std::vector<int32_t> compressed_data;
 public:
  void EncodeArray(const int32_t* in, const size_t length) override {
    if (length == 0) return;
    int stride = BlockStride32(length);
    for (size_t i = 0; i < length; ++i) {
      int32_t A, B, C; GetNeighbors32(in, i, stride, A, B, C);
      compressed_data[i] = ZigzagEnc32Pred(in[i] - JpegPredict32(A, B, C, 2));
    }
  }
  void DecodeArray(int32_t* out, const size_t length) override {
    if (length == 0) return;
    int stride = BlockStride32(length);
    for (size_t i = 0; i < length; ++i) {
      int32_t A, B, C; GetNeighbors32(out, i, stride, A, B, C);
      out[i] = JpegPredict32(A, B, C, 2) + ZigzagDec32Pred(compressed_data[i]);
    }
  }
  std::string name() const override { return "jpeg_pred2_B_i32"; }
  PRED_CODEC_BOILERPLATE_I32(JpegPred2Codec)
};

// ── Predictor 3: C (upper-left neighbor) ──────────────────────────────────────
// Residual = x - C. Inputs in [0, 2^26]; residuals in [-2^26, 2^26].
class JpegPred3Codec : public StatefulIntegerCodec<int32_t> {
  std::vector<int32_t> compressed_data;
 public:
  void EncodeArray(const int32_t* in, const size_t length) override {
    if (length == 0) return;
    int stride = BlockStride32(length);
    for (size_t i = 0; i < length; ++i) {
      int32_t A, B, C; GetNeighbors32(in, i, stride, A, B, C);
      compressed_data[i] = ZigzagEnc32Pred(in[i] - JpegPredict32(A, B, C, 3));
    }
  }
  void DecodeArray(int32_t* out, const size_t length) override {
    if (length == 0) return;
    int stride = BlockStride32(length);
    for (size_t i = 0; i < length; ++i) {
      int32_t A, B, C; GetNeighbors32(out, i, stride, A, B, C);
      out[i] = JpegPredict32(A, B, C, 3) + ZigzagDec32Pred(compressed_data[i]);
    }
  }
  std::string name() const override { return "jpeg_pred3_C_i32"; }
  PRED_CODEC_BOILERPLATE_I32(JpegPred3Codec)
};

// ── Predictor 4: A + B − C ─────────────────────────────────────────────────────
// Residual = x - (A+B-C). Inputs in [0, 2^26].
// pred ∈ [-2^26, 2^27]; residual ∈ [-(2^27), 2^27].
// ZigzagEnc32Pred(±2^27) = 2^28 < INT32_MAX, so encoded values fit in int32_t.
class JpegPred4Codec : public StatefulIntegerCodec<int32_t> {
  std::vector<int32_t> compressed_data;
 public:
  void EncodeArray(const int32_t* in, const size_t length) override {
    if (length == 0) return;
    int stride = BlockStride32(length);
    for (size_t i = 0; i < length; ++i) {
      int32_t A, B, C; GetNeighbors32(in, i, stride, A, B, C);
      compressed_data[i] = ZigzagEnc32Pred(in[i] - JpegPredict32(A, B, C, 4));
    }
  }
  void DecodeArray(int32_t* out, const size_t length) override {
    if (length == 0) return;
    int stride = BlockStride32(length);
    for (size_t i = 0; i < length; ++i) {
      int32_t A, B, C; GetNeighbors32(out, i, stride, A, B, C);
      out[i] = JpegPredict32(A, B, C, 4) + ZigzagDec32Pred(compressed_data[i]);
    }
  }
  std::string name() const override { return "jpeg_pred4_ApBmC_i32"; }
  PRED_CODEC_BOILERPLATE_I32(JpegPred4Codec)
};

// ── Predictor 5: A + (B − C)/2 ─────────────────────────────────────────────────
// Residual = x - (A+(B-C)/2). Inputs in [0, 2^26].
// (B-C)/2 ∈ [-2^25, 2^25]; pred ∈ [-2^25, 2^26+2^25] ≈ [-2^25, 1.5·2^26].
// Residual magnitude ≤ 2^26 + 2^25 < 2^27; zigzag fits in int32_t.
class JpegPred5Codec : public StatefulIntegerCodec<int32_t> {
  std::vector<int32_t> compressed_data;
 public:
  void EncodeArray(const int32_t* in, const size_t length) override {
    if (length == 0) return;
    int stride = BlockStride32(length);
    for (size_t i = 0; i < length; ++i) {
      int32_t A, B, C; GetNeighbors32(in, i, stride, A, B, C);
      compressed_data[i] = ZigzagEnc32Pred(in[i] - JpegPredict32(A, B, C, 5));
    }
  }
  void DecodeArray(int32_t* out, const size_t length) override {
    if (length == 0) return;
    int stride = BlockStride32(length);
    for (size_t i = 0; i < length; ++i) {
      int32_t A, B, C; GetNeighbors32(out, i, stride, A, B, C);
      out[i] = JpegPredict32(A, B, C, 5) + ZigzagDec32Pred(compressed_data[i]);
    }
  }
  std::string name() const override { return "jpeg_pred5_Ap(BmC)d2_i32"; }
  PRED_CODEC_BOILERPLATE_I32(JpegPred5Codec)
};

// ── Predictor 6: B + (A − C)/2 ─────────────────────────────────────────────────
// Residual = x - (B+(A-C)/2). Same range analysis as predictor 5.
class JpegPred6Codec : public StatefulIntegerCodec<int32_t> {
  std::vector<int32_t> compressed_data;
 public:
  void EncodeArray(const int32_t* in, const size_t length) override {
    if (length == 0) return;
    int stride = BlockStride32(length);
    for (size_t i = 0; i < length; ++i) {
      int32_t A, B, C; GetNeighbors32(in, i, stride, A, B, C);
      compressed_data[i] = ZigzagEnc32Pred(in[i] - JpegPredict32(A, B, C, 6));
    }
  }
  void DecodeArray(int32_t* out, const size_t length) override {
    if (length == 0) return;
    int stride = BlockStride32(length);
    for (size_t i = 0; i < length; ++i) {
      int32_t A, B, C; GetNeighbors32(out, i, stride, A, B, C);
      out[i] = JpegPredict32(A, B, C, 6) + ZigzagDec32Pred(compressed_data[i]);
    }
  }
  std::string name() const override { return "jpeg_pred6_Bp(AmC)d2_i32"; }
  PRED_CODEC_BOILERPLATE_I32(JpegPred6Codec)
};

// ── Predictor 7: (A + B)/2 ─────────────────────────────────────────────────────
// Residual = x - (A+B)/2. Inputs in [0, 2^26].
// pred ∈ [0, 2^26]; residual ∈ [-2^26, 2^26]; zigzag ∈ [0, 2^27] ⊂ int32_t.
class JpegPred7Codec : public StatefulIntegerCodec<int32_t> {
  std::vector<int32_t> compressed_data;
 public:
  void EncodeArray(const int32_t* in, const size_t length) override {
    if (length == 0) return;
    int stride = BlockStride32(length);
    for (size_t i = 0; i < length; ++i) {
      int32_t A, B, C; GetNeighbors32(in, i, stride, A, B, C);
      compressed_data[i] = ZigzagEnc32Pred(in[i] - JpegPredict32(A, B, C, 7));
    }
  }
  void DecodeArray(int32_t* out, const size_t length) override {
    if (length == 0) return;
    int stride = BlockStride32(length);
    for (size_t i = 0; i < length; ++i) {
      int32_t A, B, C; GetNeighbors32(out, i, stride, A, B, C);
      out[i] = JpegPredict32(A, B, C, 7) + ZigzagDec32Pred(compressed_data[i]);
    }
  }
  std::string name() const override { return "jpeg_pred7_ApBd2_i32"; }
  PRED_CODEC_BOILERPLATE_I32(JpegPred7Codec)
};

// ── JPEG-LS predictor 0: MED (Median Edge Detector) ───────────────────────────
// pred = median(A, B, A+B-C). Residual = x - pred, zigzag encoded.
// Inputs in [0, 2^26]. A+B-C ∈ [-2^26, 2^27]; median is always one of
// {A, B, A+B-C} so pred ∈ [-2^26, 2^27]; residual magnitude ≤ 2^27.
// ZigzagEnc32Pred(±2^27) = 2^28 < INT32_MAX. Safe.
class JpegLSMedCodec : public StatefulIntegerCodec<int32_t> {
  std::vector<int32_t> compressed_data;
 public:
  void EncodeArray(const int32_t* in, const size_t length) override {
    if (length == 0) return;
    int stride = BlockStride32(length);
    for (size_t i = 0; i < length; ++i) {
      int32_t A, B, C; GetNeighbors32(in, i, stride, A, B, C);
      compressed_data[i] = ZigzagEnc32Pred(in[i] - JpegLSMed32(A, B, C));
    }
  }
  void DecodeArray(int32_t* out, const size_t length) override {
    if (length == 0) return;
    int stride = BlockStride32(length);
    for (size_t i = 0; i < length; ++i) {
      int32_t A, B, C; GetNeighbors32(out, i, stride, A, B, C);
      out[i] = JpegLSMed32(A, B, C) + ZigzagDec32Pred(compressed_data[i]);
    }
  }
  std::string name() const override { return "jpegls_pred0_med_i32"; }
  PRED_CODEC_BOILERPLATE_I32(JpegLSMedCodec)
};

// ── JPEG-LS predictor 1: Paeth (PNG) ───────────────────────────────────────────
// pred = Paeth(A, B, C).  Residual = x - pred, zigzag encoded.
// Inputs in [0, 2^26]. Paeth selects one of {A, B, C} ∈ [0, 2^26].
// Residual ∈ [-2^26, 2^26]; zigzag maps to [0, 2^27] ⊂ int32_t.
// Paeth internals: p = A+B-C ∈ [-2^26, 2^27]; abs()-values fit in int32_t.
class PaethCodec : public StatefulIntegerCodec<int32_t> {
  std::vector<int32_t> compressed_data;
 public:
  void EncodeArray(const int32_t* in, const size_t length) override {
    if (length == 0) return;
    int stride = BlockStride32(length);
    for (size_t i = 0; i < length; ++i) {
      int32_t A, B, C; GetNeighbors32(in, i, stride, A, B, C);
      compressed_data[i] = ZigzagEnc32Pred(in[i] - PaethPredict32(A, B, C));
    }
  }
  void DecodeArray(int32_t* out, const size_t length) override {
    if (length == 0) return;
    int stride = BlockStride32(length);
    for (size_t i = 0; i < length; ++i) {
      int32_t A, B, C; GetNeighbors32(out, i, stride, A, B, C);
      out[i] = PaethPredict32(A, B, C) + ZigzagDec32Pred(compressed_data[i]);
    }
  }
  std::string name() const override { return "jpegls_pred1_paeth_i32"; }
  PRED_CODEC_BOILERPLATE_I32(PaethCodec)
};

#undef PRED_CODEC_BOILERPLATE_I32
