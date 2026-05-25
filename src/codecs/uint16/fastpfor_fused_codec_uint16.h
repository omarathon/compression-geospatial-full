#pragma once

#include <cassert>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "generic_codecs.h"
#include "predictive_codecs_u16.h"  // ZigzagEnc16 / ZigzagDec16
#include "FastPFor/headers/compositecodec_u16.h"

class FastPForFusedCodecU16 : public StatefulIntegerCodec<uint16_t> {
 private:
  FastPForLib::CompositeCodecU16 codec;

 public:
  std::vector<uint32_t> compressed;

  void EncodeArray(const uint16_t* in, const size_t length) override {
    size_t compressed_size = compressed.size();
    codec.encodeArray(in, length, compressed.data(), compressed_size);
    compressed.resize(compressed_size);
    compressed.shrink_to_fit();
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
  std::vector<uint16_t> scratch;  // delta+zigzag pre-pass buffer

 public:
  std::vector<uint32_t> compressed;

  void EncodeArray(const uint16_t* in, const size_t length) override {
    // Scalar pre-pass: prev resets every 16 elements; anchor (offset%16 == 0)
    // is encoded as zigzag(in[i] - 0) = zigzag(in[i]).
    scratch.resize(length);
    uint16_t prev = 0;
    for (size_t i = 0; i < length; ++i) {
      if ((i & 15) == 0) prev = 0;
      const uint16_t delta = static_cast<uint16_t>(in[i] - prev);
      scratch[i] = ZigzagEnc16(delta);
      prev = in[i];
    }
    size_t compressed_size = compressed.size();
    codec.encodeArray(scratch.data(), length, compressed.data(),
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

  void AllocEncoded(const uint16_t* in, size_t length) override {
    compressed.resize(length * 2);
  };

  void clear() override {
    compressed.clear();
    compressed.shrink_to_fit();
    scratch.clear();
    scratch.shrink_to_fit();
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
  std::vector<uint16_t> scratch;

 public:
  std::vector<uint32_t> compressed;

  void EncodeArray(const uint16_t* in, const size_t length) override {
    scratch.resize(length);
    uint16_t prev = 0;
    for (size_t i = 0; i < length; ++i) {
      const uint16_t delta = static_cast<uint16_t>(in[i] - prev);
      scratch[i] = ZigzagEnc16(delta);
      prev = in[i];
    }
    size_t compressed_size = compressed.size();
    codec.encodeArray(scratch.data(), length, compressed.data(),
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

  void AllocEncoded(const uint16_t* in, size_t length) override {
    compressed.resize(length * 2);
  };

  void clear() override {
    compressed.clear();
    compressed.shrink_to_fit();
    scratch.clear();
    scratch.shrink_to_fit();
  }

  std::vector<uint16_t>& GetEncoded() override {
    throw std::runtime_error(
        "Encoded format does not match input. Cannot forward.");
  };
};

// Variant of FastPForFusedCodecU16 that uses the "corrected" decode path:
// exception correction is folded into the SIMD aggregation via per-OutReg
// correction masks (clean reimplementation of the precomputed-mask idea, with
// stack-local state and SIMD-friendly mask writes). Same encoded format as
// FastPForFusedCodecU16 — only the decode path differs.
class FastPForFusedCorrectedCodecU16 : public StatefulIntegerCodec<uint16_t> {
 private:
  FastPForLib::CompositeCodecU16 codec;

 public:
  std::vector<uint32_t> compressed;

  void EncodeArray(const uint16_t* in, const size_t length) override {
    size_t compressed_size = compressed.size();
    codec.encodeArray(in, length, compressed.data(), compressed_size);
    compressed.resize(compressed_size);
    compressed.shrink_to_fit();
  }

  void DecodeArray(uint16_t* out, const std::size_t length) override {
    size_t recovered_size = length;
    codec.decodeArrayCorrected(compressed.data(), compressed.size(), out,
                                recovered_size);
    assert(recovered_size == length);
    // Sum is already stored in out[length] and out[length+1] by the codec
  }

  std::size_t EncodedNumValues() override { return compressed.size(); }

  std::size_t EncodedSizeValue() override { return sizeof(uint32_t); }

  virtual ~FastPForFusedCorrectedCodecU16() {}

  std::string name() const override {
    return "FastPFor_fused_corrected_" + codec.name();
  }

  std::size_t GetOverflowSize(size_t) const override {
    return 64;  // 32 uint32 slots = 64 uint16 slots
  }

  StatefulIntegerCodec<uint16_t>* CloneFresh() const override {
    return new FastPForFusedCorrectedCodecU16();
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
