#include <cassert>
#include <cstdint>
#include <iostream>
#include <string>

#include "compression.h"
#include "generic_codecs.h"
#include "turbocompression.h"

#ifdef __clang__
#pragma clang diagnostic ignored "-Wreturn-local-addr"
#endif

// NOTE: Disabled because it fails after transformations: threshold and
// valueShift.
class FrameOfReferenceCodec : public StatefulIntegerCodec<int32_t> {
 public:
  std::vector<uint32_t> compressed;

  void EncodeArray(const int32_t *in, const size_t length) override {
    int32_t *in_nconst =
        const_cast<int32_t *>(in);  // Necessary as compress input is not const
    uint32_t *out = compress(reinterpret_cast<uint32_t *>(in_nconst), length,
                             compressed.data());
    size_t compressed_size = out - compressed.data();
    compressed.resize(compressed_size);
  }

  void DecodeArray(int32_t *out, const std::size_t length) override {
    uint32_t nvalue = 0;
    uncompress(compressed.data(), reinterpret_cast<uint32_t *>(out), nvalue);
    assert(nvalue == length);
  }

  std::size_t EncodedNumValues() override { return compressed.size(); }

  std::size_t EncodedSizeValue() override { return sizeof(uint32_t); }

  virtual ~FrameOfReferenceCodec() {}

  std::string name() const override { return "FrameOfReference"; }

  std::size_t GetOverflowSize(size_t) const override { return 1024; }

  StatefulIntegerCodec<int32_t> *CloneFresh() const override {
    return new FrameOfReferenceCodec();
  }

  void AllocEncoded(const int32_t *in, size_t length) override {
    compressed.resize(length + 1024);
  };

  void clear() override {
    compressed.clear();
    compressed.shrink_to_fit();
  }

  std::vector<int32_t> &GetEncoded() override {
    return reinterpret_cast<std::vector<int32_t> &>(compressed);
  };
};

class FrameOfReferenceTurboCodec : public StatefulIntegerCodec<int32_t> {
 public:
  std::vector<uint8_t> compressed;

  void EncodeArray(const int32_t *in, const size_t length) override {
    int32_t *in_nconst =
        const_cast<int32_t *>(in);  // Necessary as compress input is not const
    uint8_t *out = turbocompress(reinterpret_cast<uint32_t *>(in_nconst),
                                 length, compressed.data());
    size_t compressed_size = out - compressed.data();
    compressed.resize(compressed_size);
  }

  void DecodeArray(int32_t *out, const std::size_t length) override {
    uint32_t nvalue = 0;
    turbouncompress(compressed.data(), reinterpret_cast<uint32_t *>(out),
                    nvalue);
    assert(nvalue == length);
  }

  std::size_t EncodedNumValues() override { return compressed.size(); }

  std::size_t EncodedSizeValue() override { return sizeof(uint8_t); }

  virtual ~FrameOfReferenceTurboCodec() {}

  std::string name() const override { return "FrameOfReference_Turbo"; }

  std::size_t GetOverflowSize(size_t) const override { return 1024; }

  StatefulIntegerCodec<int32_t> *CloneFresh() const override {
    return new FrameOfReferenceTurboCodec();
  }

  void AllocEncoded(const int32_t *in, size_t length) override {
    compressed.resize(length * sizeof(int32_t) + 1024);
  };

  void clear() override {
    compressed.clear();
    compressed.shrink_to_fit();
  }

  std::vector<int32_t> &GetEncoded() override {
    throw std::runtime_error(
        "Encoded format does not match input. Cannot forward.");
  };
};