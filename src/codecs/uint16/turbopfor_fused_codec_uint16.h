#pragma once

#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

#include "generic_codecs.h"
#include "ic.h"  // p4nenc128v16 / p4nbound128v16

// Fused-sum decode (prototype). Defined in external/TurboPFor/lib/vp4d_fused.c.
extern "C" uint32_t p4ndec128v16_sum(const unsigned char *in, unsigned n);

// Encodes with stock TurboPFor (p4nenc128v16) — so CR is identical to
// TurboPFor128 — and decodes via the fused-sum path: the corrected value stream
// is materialized in SIMD registers and summed (32-bit-widened) without storing
// decoded values to memory. Sum is written to out[length] / out[length+1] like
// the FastPFor fused codecs.
class TurboPForFusedCodecU16 : public StatefulIntegerCodec<uint16_t> {
 private:
  std::vector<uint8_t> compressed;

 public:
  void EncodeArray(const uint16_t *in, const size_t length) override {
    compressed.resize(p4nbound128v16(length));
    uint16_t *in_nc = const_cast<uint16_t *>(in);  // TurboPFor is a C API
    size_t csize = p4nenc128v16(in_nc, length, compressed.data());
    compressed.resize(csize);
  }

  void DecodeArray(uint16_t *out, const std::size_t length) override {
    uint32_t s = p4ndec128v16_sum(compressed.data(), static_cast<unsigned>(length));
    out[length]     = static_cast<uint16_t>(s & 0xFFFF);
    out[length + 1] = static_cast<uint16_t>(s >> 16);
  }

  std::size_t EncodedNumValues() override { return compressed.size(); }
  std::size_t EncodedSizeValue() override { return sizeof(uint8_t); }
  virtual ~TurboPForFusedCodecU16() {}

  std::string name() const override { return "TurboPFor_fused_128v16_sum"; }

  std::size_t GetOverflowSize(size_t) const override { return 64; }

  StatefulIntegerCodec<uint16_t> *CloneFresh() const override {
    return new TurboPForFusedCodecU16();
  }

  void AllocEncoded(const uint16_t *, size_t) override {}

  void clear() override {
    compressed.clear();
    compressed.shrink_to_fit();
  }

  std::vector<uint16_t> &GetEncoded() override {
    throw std::runtime_error("Encoded format does not match input.");
  }
};
