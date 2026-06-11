#pragma once

#include <cassert>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

#include "generic_codecs.h"
#include "delta_scratch_u16.h"  // GetPackScratch()

// 256-width 16-bit PFor with fused-sum decode. Defined in
// external/TurboPFor/lib/vp4d256v16_fused.c. Unlike the 128v16 codec this is a
// NEW bitstream (not stock p4n*-compatible): a 256-element PFor block built on
// simdcomp's AVX2 16-lane kernels. Decode materializes the corrected stream in
// SIMD registers (low bits + in-register exception merge) and sums it without
// storing decoded values. Sum is written to out[length] / out[length+1] like
// the other fused codecs.
extern "C" size_t   p4nenc256v16(uint16_t *in, size_t n, unsigned char *out);
extern "C" uint32_t p4ndec256v16_sum(const unsigned char *in, unsigned n);
extern "C" uint32_t p4ndec256v16_sum_madd(const unsigned char *in, unsigned n);
extern "C" uint32_t p4ndec256v16_sum_fast(const unsigned char *in, unsigned n);
extern "C" uint32_t p4ndec256v16_sum_fast_unpack(const unsigned char *in, unsigned n);
extern "C" size_t   p4nbound256v16_fused(size_t n);

class TurboPForFused256CodecU16 : public StatefulIntegerCodec<uint16_t> {
 private:
  std::vector<uint8_t> compressed;

 public:
  void EncodeArray(const uint16_t *in, const size_t length) override {
    // Encode into shared thread-local scratch, then assign exact bytes into
    // `compressed`. Avoids the worst-case resize + shrink on `compressed` that
    // leaves glibc's freelist holding ~128 KB per codec instance. The 256 KB
    // scratch covers the max supported block (p4nbound256v16_fused(65536)=144 KB).
    auto& scratch = GetPackScratch();
    assert(p4nbound256v16_fused(length) <= scratch.size());
    uint16_t *in_nc = const_cast<uint16_t *>(in);  // C API
    size_t csize = p4nenc256v16(in_nc, length, scratch.data());
    compressed.assign(scratch.data(), scratch.data() + csize);
  }

  void DecodeArray(uint16_t *out, const std::size_t length) override {
    uint32_t s = p4ndec256v16_sum(compressed.data(), static_cast<unsigned>(length));
    out[length]     = static_cast<uint16_t>(s & 0xFFFF);
    out[length + 1] = static_cast<uint16_t>(s >> 16);
  }

  std::size_t EncodedNumValues() override { return compressed.size(); }
  std::size_t EncodedSizeValue() override { return sizeof(uint8_t); }
  virtual ~TurboPForFused256CodecU16() {}

  std::string name() const override { return "TurboPFor_fused_256v16_sum"; }

  std::size_t GetOverflowSize(size_t) const override { return 64; }

  StatefulIntegerCodec<uint16_t> *CloneFresh() const override {
    return new TurboPForFused256CodecU16();
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

// Non-FoR PFor with selectable aggregate (madd/unpack) and exception handling
// (factored sum-fast vs positional merge-into-OutReg). On a PFOR_BYTE_EXC build
// the excess is byte-aligned; otherwise bit-packed (same encoder, build-time).
// madd assumes madd-safe data (full value < 2^15 for merge, low bits for factored)
// — true for ETOPO1-normalized; not gated (benchmark variant).
class TurboPForFused256VariantCodecU16 : public StatefulIntegerCodec<uint16_t> {
 private:
  std::vector<uint8_t> compressed;
  bool madd_, merge_;

 public:
  TurboPForFused256VariantCodecU16(FusedAggImpl agg, bool merge)
      : madd_(agg == FusedAggImpl::kMadd), merge_(merge) {}

  void EncodeArray(const uint16_t *in, const size_t length) override {
    auto &scratch = GetPackScratch();
    assert(p4nbound256v16_fused(length) <= scratch.size());
    size_t csize = p4nenc256v16(const_cast<uint16_t *>(in), length, scratch.data());
    compressed.assign(scratch.data(), scratch.data() + csize);
  }

  void DecodeArray(uint16_t *out, const std::size_t length) override {
    const unsigned n = static_cast<unsigned>(length);
    const unsigned char *c = compressed.data();
    uint32_t s = merge_ ? (madd_ ? p4ndec256v16_sum_madd(c, n) : p4ndec256v16_sum(c, n))
                        : (madd_ ? p4ndec256v16_sum_fast(c, n) : p4ndec256v16_sum_fast_unpack(c, n));
    out[length]     = static_cast<uint16_t>(s & 0xFFFF);
    out[length + 1] = static_cast<uint16_t>(s >> 16);
  }

  std::size_t EncodedNumValues() override { return compressed.size(); }
  std::size_t EncodedSizeValue() override { return sizeof(uint8_t); }
  virtual ~TurboPForFused256VariantCodecU16() {}

  std::string name() const override {
    std::string n = "TurboPFor_fused_256v16_";
    n += merge_ ? "merge_" : "byte_";
    n += madd_ ? "madd" : "unpack";
    return n;
  }

  std::size_t GetOverflowSize(size_t) const override { return 64; }

  StatefulIntegerCodec<uint16_t> *CloneFresh() const override {
    return new TurboPForFused256VariantCodecU16(
        madd_ ? FusedAggImpl::kMadd : FusedAggImpl::kUnpack, merge_);
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
