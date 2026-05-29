#pragma once

#include <cassert>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "generic_codecs.h"
#include "predictive_codecs_u16.h"  // ZigzagEnc16 / ZigzagDec16
#include "FastPFor/headers/compositecodec_u16.h"
#include "FastPFor/headers/simdpfor_u16.h"  // SIMDPForU16::BlockSize
#include "delta_scratch_u16.h"

class FastPForFusedCodecU16 : public StatefulIntegerCodec<uint16_t> {
 private:
  FastPForLib::CompositeCodecU16 codec;

 public:
  std::vector<uint32_t> compressed;

  void EncodeArray(const uint16_t* in, const size_t length) override {
    auto& scratch = GetFastPForScratch();
    size_t nvalue = scratch.size();
    codec.encodeArray(in, length, scratch.data(), nvalue);
    compressed.assign(scratch.data(), scratch.data() + nvalue);
  }

  void DecodeArray(uint16_t* out, const std::size_t length) override {
    size_t recovered_size = length;
    codec.decodeArray(compressed.data(), compressed.size(), out, recovered_size);
    assert(recovered_size == length);
    // Sum is already stored in out[length] and out[length+1] by the codec
  }

  std::size_t EncodedNumValues() override { return compressed.size(); }

  std::size_t EncodedSizeValue() override { return sizeof(uint32_t); }

  virtual ~FastPForFusedCodecU16() {}

  std::string name() const override {
    return "FastPFor_fused_" + codec.name();
  }

  std::size_t GetOverflowSize(size_t) const override {
    return 64;  // 32 uint32 slots = 64 uint16 slots
  }

  StatefulIntegerCodec<uint16_t>* CloneFresh() const override {
    return new FastPForFusedCodecU16();
  }
  double MeanExceptionsPerInnerBlock() const override {
    return codec.codec1.MeanExceptionsPerBlock();
  }

  void AllocEncoded(const uint16_t*, size_t) override {
    // No-op: EncodeArray writes through shared scratch and sizes `compressed`
    // exactly via assign() — no worst-case allocation on `compressed`.
  };

  void clear() override {
    compressed.clear();
    compressed.shrink_to_fit();
  }

  std::vector<uint16_t>& GetEncoded() override {
    throw std::runtime_error(
        "Encoded format does not match input. Cannot forward.");
  };
};

// ── Fused delta variants ─────────────────────────────────────────────────────
// Both variants pre-transform the input scalar-side into zigzag-encoded deltas
// before bitpacking, then the SIMD decode path runs a fused
//   correction → zigzag_dec → prefix_sum [→ +carry → update_carry] → aggregate
// pipeline per OutReg. Same encoded format on disk as
// FastPForFusedCorrectedCodecU16 — only the encode-side transform and the
// decode-side pipeline differ.

// LOCAL: prev resets to 0 every 16 elements. Each OutReg is an independent
// prefix-sum window; no inter-OutReg / inter-block carry.
class FastPForFusedCorrectedDeltaLocalCodecU16
    : public StatefulIntegerCodec<uint16_t> {
 private:
  FastPForLib::CompositeCodecU16 codec;

 public:
  std::vector<uint32_t> compressed;

  void EncodeArray(const uint16_t* in, const size_t length) override {
    // Scalar pre-pass into shared thread-local scratch (no heap alloc).
    uint16_t prev = 0;
    for (size_t i = 0; i < length; ++i) {
      if ((i & 15) == 0) prev = 0;
      const uint16_t delta = static_cast<uint16_t>(in[i] - prev);
      s_delta_scratch[i] = ZigzagEnc16(delta);
      prev = in[i];
    }
    size_t compressed_size = compressed.size();
    codec.encodeArray(s_delta_scratch, length, compressed.data(),
                      compressed_size);
    compressed.resize(compressed_size);
    compressed.shrink_to_fit();
  }

  void DecodeArray(uint16_t* out, const std::size_t length) override {
    size_t recovered_size = length;
    codec.decodeArrayCorrectedDeltaLocal(compressed.data(), compressed.size(),
                                          out, recovered_size);
    assert(recovered_size == length);
  }

  std::size_t EncodedNumValues() override { return compressed.size(); }
  std::size_t EncodedSizeValue() override { return sizeof(uint32_t); }
  virtual ~FastPForFusedCorrectedDeltaLocalCodecU16() {}

  std::string name() const override {
    return "FastPFor_fused_corrected_delta_local_" + codec.name();
  }

  std::size_t GetOverflowSize(size_t) const override { return 64; }

  StatefulIntegerCodec<uint16_t>* CloneFresh() const override {
    return new FastPForFusedCorrectedDeltaLocalCodecU16();
  }
  double MeanExceptionsPerInnerBlock() const override {
    return codec.codec1.MeanExceptionsPerBlock();
  }

  void AllocEncoded(const uint16_t* in, size_t length) override {
    compressed.resize(length * 2);
  };

  void clear() override {
    compressed.clear();
    compressed.shrink_to_fit();
  }

  std::vector<uint16_t>& GetEncoded() override {
    throw std::runtime_error(
        "Encoded format does not match input. Cannot forward.");
  };
};

// CARRY: prev persists across the whole stream. The SIMD decode threads a
// broadcast-carry __m256i across OutRegs and blocks; the VB tail seeds its
// scalar prev from the last decoded SIMD value.
class FastPForFusedCorrectedDeltaCarryCodecU16
    : public StatefulIntegerCodec<uint16_t> {
 private:
  FastPForLib::CompositeCodecU16 codec;

 public:
  std::vector<uint32_t> compressed;

  void EncodeArray(const uint16_t* in, const size_t length) override {
    uint16_t prev = 0;
    for (size_t i = 0; i < length; ++i) {
      const uint16_t delta = static_cast<uint16_t>(in[i] - prev);
      s_delta_scratch[i] = ZigzagEnc16(delta);
      prev = in[i];
    }
    size_t compressed_size = compressed.size();
    codec.encodeArray(s_delta_scratch, length, compressed.data(),
                      compressed_size);
    compressed.resize(compressed_size);
    compressed.shrink_to_fit();
  }

  void DecodeArray(uint16_t* out, const std::size_t length) override {
    size_t recovered_size = length;
    codec.decodeArrayCorrectedDeltaCarry(compressed.data(), compressed.size(),
                                          out, recovered_size);
    assert(recovered_size == length);
  }

  std::size_t EncodedNumValues() override { return compressed.size(); }
  std::size_t EncodedSizeValue() override { return sizeof(uint32_t); }
  virtual ~FastPForFusedCorrectedDeltaCarryCodecU16() {}

  std::string name() const override {
    return "FastPFor_fused_corrected_delta_carry_" + codec.name();
  }

  std::size_t GetOverflowSize(size_t) const override { return 64; }

  StatefulIntegerCodec<uint16_t>* CloneFresh() const override {
    return new FastPForFusedCorrectedDeltaCarryCodecU16();
  }
  double MeanExceptionsPerInnerBlock() const override {
    return codec.codec1.MeanExceptionsPerBlock();
  }

  void AllocEncoded(const uint16_t* in, size_t length) override {
    compressed.resize(length * 2);
  };

  void clear() override {
    compressed.clear();
    compressed.shrink_to_fit();
  }

  std::vector<uint16_t>& GetEncoded() override {
    throw std::runtime_error(
        "Encoded format does not match input. Cannot forward.");
  };
};

// ── FastPFor FoR-global ──────────────────────────────────────────────────────
//
// Frame-of-Reference codec using FastPFor bitpacking. Per-block anchor = min;
// encoder subtracts anchor → unsigned residuals → FastPFor.
// Decoder reads the stored anchor array from the front of `compressed`, then
// routes each block through the FoR-corrected SIMD unpack path.
//
// Encoded layout: [n_anchor_words uint32 words | FastPFor(residuals)]
//   n_anchor_words = (n_blocks + 1) / 2   (2 uint16 anchors packed per uint32)
//
// `forWindowSize_` (32/64/128/256) applies to the adaptive_b path only and
// controls both the FoR anchor scope and the bitpacking window size (they are
// always equal). The global_b path always uses kBlockSize = 256.
//
// REQUIREMENT: length % effectiveBlockSize() == 0.
class FastPForFusedCorrectedForGlobalCodecU16 : public StatefulIntegerCodec<uint16_t> {
  static constexpr size_t kBlockSize = FastPForLib::SIMDPForU16::BlockSize;

  static size_t n_anchor_words(size_t n_blocks) {
    return (n_blocks + 1) / 2;  // two uint16 anchors packed per uint32 word
  }

  static size_t chunkSizeFor(bool useGlobalB) {
    return useGlobalB
        ? (1U << (32 - FastPForLib::SIMDPForU16::blocksizeinbits - 1))
        : FastPForLib::SIMDPForU16::BlockSize;
  }

  size_t effectiveBlockSize() const {
    return useGlobalB_ ? kBlockSize : forWindowSize_;
  }

  FastPForLib::CompositeCodecU16 codec;
  bool   useGlobalB_;
  double exceptionPenalty_;
  size_t forWindowSize_;

 public:
  std::vector<uint32_t> compressed;

  explicit FastPForFusedCorrectedForGlobalCodecU16(bool useGlobalB = true,
                                                    double exceptionPenalty = 16.0,
                                                    size_t forWindowSize = 256)
      : codec(chunkSizeFor(useGlobalB), exceptionPenalty),
        useGlobalB_(useGlobalB), exceptionPenalty_(exceptionPenalty),
        forWindowSize_(forWindowSize) {
    assert(forWindowSize == 32 || forWindowSize == 64 ||
           forWindowSize == 128 || forWindowSize == 256);
  }

  void EncodeArray(const uint16_t* in, const size_t length) override {
    const size_t blk = effectiveBlockSize();
    const size_t n_blocks = length / blk;
    assert(n_blocks <= 2048 && "too many blocks for s_anchor_scratch");

    // 1. Compute per-block anchors (= min) and write residuals into
    //    s_delta_scratch. Tail elements (if any) are copied as-is.
    for (size_t k = 0; k < n_blocks; ++k) {
      const uint16_t* blk_in = in + k * blk;
      uint16_t anchor = blk_in[0];
      for (size_t i = 1; i < blk; ++i)
        if (blk_in[i] < anchor) anchor = blk_in[i];
      s_anchor_scratch[k] = anchor;
      for (size_t i = 0; i < blk; ++i)
        s_delta_scratch[k * blk + i] =
            static_cast<uint16_t>(blk_in[i] - anchor);
    }
    const size_t rounded = n_blocks * blk;
    for (size_t i = rounded; i < length; ++i)
      s_delta_scratch[i] = in[i];  // tail: no anchor subtraction

    // 2. Encode residuals into shared scratch, leaving space for anchor words
    //    at the front. scratch-then-assign: no worst-case alloc on `compressed`.
    //    adaptive_b uses flat format to break the per-block b pointer chain.
    auto& scratch = GetFastPForScratch();
    const size_t naw = n_anchor_words(n_blocks);
    size_t data_capacity = scratch.size() - naw;
    if (useGlobalB_) {
      codec.encodeArray(s_delta_scratch, length,
                        scratch.data() + naw, data_capacity);
    } else {
      codec.encodeArrayFlat(s_delta_scratch, length,
                            scratch.data() + naw, data_capacity, forWindowSize_);
    }

    // 3. Write anchors packed 2 per uint32 (low 16 = even block, high 16 = odd).
    for (size_t k = 0; k < n_blocks; ++k) {
      if (k & 1)
        scratch[k / 2] |= static_cast<uint32_t>(s_anchor_scratch[k]) << 16;
      else
        scratch[k / 2] = static_cast<uint32_t>(s_anchor_scratch[k]);
    }

    const size_t actual = naw + data_capacity;
    assert(actual <= scratch.size());
    compressed.assign(scratch.data(), scratch.data() + actual);
  }

  void DecodeArray(uint16_t* out, const std::size_t length) override {
    const size_t blk = effectiveBlockSize();
    const size_t n_blocks = length / blk;
    assert(n_blocks <= 2048 && "too many blocks for s_anchor_scratch");
    const size_t naw = n_anchor_words(n_blocks);

    // 1. Unpack anchors from front (2 per uint32: low 16 = even, high 16 = odd).
    for (size_t k = 0; k < n_blocks; ++k)
      s_anchor_scratch[k] =
          static_cast<uint16_t>(compressed[k / 2] >> (16 * (k & 1)));

    // 2. Decode residuals via FoR-corrected path (adds anchor per block).
    //    adaptive_b uses flat format (no data-dep pointer chain on decode).
    size_t recovered_size = length;
    if (useGlobalB_) {
      codec.decodeArrayCorrectedFor(compressed.data() + naw,
                                     compressed.size() - naw, out,
                                     recovered_size, s_anchor_scratch);
    } else {
      codec.decodeArrayFlatCorrectedFor(compressed.data() + naw,
                                         compressed.size() - naw, out,
                                         recovered_size, s_anchor_scratch,
                                         forWindowSize_);
    }
    assert(recovered_size == length);
    // Sum stored in out[length] and out[length+1] by the codec.
  }

  std::size_t EncodedNumValues() override { return compressed.size(); }
  std::size_t EncodedSizeValue() override { return sizeof(uint32_t); }
  virtual ~FastPForFusedCorrectedForGlobalCodecU16() {}

  std::string name() const override {
    std::string n = "FastPFor_fused_corrected_for_global_" +
                    std::string(useGlobalB_ ? "global_b" : "adaptive_b");
    if (!useGlobalB_ && forWindowSize_ != 256)
      n += "_w" + std::to_string(forWindowSize_);
    if (exceptionPenalty_ != 16.0)
      n += "_p" + std::to_string(static_cast<int>(exceptionPenalty_));
    return n;
  }

  std::size_t GetOverflowSize(size_t) const override { return 64; }

  StatefulIntegerCodec<uint16_t>* CloneFresh() const override {
    return new FastPForFusedCorrectedForGlobalCodecU16(useGlobalB_,
                                                        exceptionPenalty_,
                                                        forWindowSize_);
  }
  double MeanExceptionsPerInnerBlock() const override {
    return codec.codec1.MeanExceptionsPerBlock();
  }

  void AllocEncoded(const uint16_t*, size_t) override {
    // No-op: EncodeArray writes through shared scratch and sizes `compressed`
    // exactly via assign() — no worst-case allocation on `compressed`.
  }

  void clear() override {
    compressed.clear();
    compressed.shrink_to_fit();
  }

  std::vector<uint16_t>& GetEncoded() override {
    throw std::runtime_error(
        "Encoded format does not match input. Cannot forward.");
  }
};

// Variant of FastPForFusedCodecU16 that uses the "corrected" decode path:
// exception correction is folded into the SIMD aggregation via per-OutReg
// correction masks (clean reimplementation of the precomputed-mask idea, with
// stack-local state and SIMD-friendly mask writes). Same encoded format as
// FastPForFusedCodecU16 — only the decode path differs.
class FastPForFusedCorrectedCodecU16 : public StatefulIntegerCodec<uint16_t> {
 private:
  FastPForLib::CompositeCodecU16 codec;
  bool useGlobalB_;
  size_t windowSize_;  // adaptive_b sub-block width (32/64/128/256); ignored for global_b

  static size_t chunkSizeFor(bool useGlobalB) {
    return useGlobalB
        ? (1U << (32 - FastPForLib::SIMDPForU16::blocksizeinbits - 1))
        : FastPForLib::SIMDPForU16::BlockSize;
  }

 public:
  std::vector<uint32_t> compressed;

  explicit FastPForFusedCorrectedCodecU16(bool useGlobalB = true,
                                          size_t windowSize = 256)
      : codec(chunkSizeFor(useGlobalB)), useGlobalB_(useGlobalB),
        windowSize_(windowSize) {
    assert(windowSize == 32 || windowSize == 64 ||
           windowSize == 128 || windowSize == 256);
  }

  void EncodeArray(const uint16_t* in, const size_t length) override {
    if (useGlobalB_) {
      size_t compressed_size = compressed.size();
      codec.encodeArray(in, length, compressed.data(), compressed_size);
      compressed.resize(compressed_size);
      compressed.shrink_to_fit();
    } else {
      // adaptive_b: flat format (no data-dep pointer chain) + scratch-assign
      auto& scratch = GetFastPForScratch();
      size_t nvalue = scratch.size();
      codec.encodeArrayFlat(in, length, scratch.data(), nvalue, windowSize_);
      compressed.assign(scratch.data(), scratch.data() + nvalue);
    }
  }

  void DecodeArray(uint16_t* out, const std::size_t length) override {
    size_t recovered_size = length;
    if (useGlobalB_) {
      codec.decodeArrayCorrected(compressed.data(), compressed.size(), out,
                                  recovered_size);
    } else {
      codec.decodeArrayFlatCorrected(compressed.data(), compressed.size(), out,
                                      recovered_size, windowSize_);
    }
    assert(recovered_size == length);
  }

  std::size_t EncodedNumValues() override { return compressed.size(); }

  std::size_t EncodedSizeValue() override { return sizeof(uint32_t); }

  virtual ~FastPForFusedCorrectedCodecU16() {}

  std::string name() const override {
    std::string n = "FastPFor_fused_corrected_" +
                    std::string(useGlobalB_ ? "global_b" : "adaptive_b");
    if (!useGlobalB_ && windowSize_ != 256)
      n += "_w" + std::to_string(windowSize_);
    return n + "_" + codec.name();
  }

  std::size_t GetOverflowSize(size_t) const override {
    return 64;  // 32 uint32 slots = 64 uint16 slots
  }

  StatefulIntegerCodec<uint16_t>* CloneFresh() const override {
    return new FastPForFusedCorrectedCodecU16(useGlobalB_, windowSize_);
  }
  double MeanExceptionsPerInnerBlock() const override {
    return codec.codec1.MeanExceptionsPerBlock();
  }

  void AllocEncoded(const uint16_t*, size_t length) override {
    if (useGlobalB_) compressed.resize(length * 2);
    // adaptive_b: no-op — EncodeArray uses scratch+assign
  };

  void clear() override {
    compressed.clear();
    compressed.shrink_to_fit();
  }

  std::vector<uint16_t>& GetEncoded() override {
    throw std::runtime_error(
        "Encoded format does not match input. Cannot forward.");
  };
};
