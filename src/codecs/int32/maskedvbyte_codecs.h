#include <cassert>
#include <cstdint>
#include <iostream>
#include <string>

#include "generic_codecs.h"
#include "varintdecode.h"
#include "varintencode.h"

#ifdef __clang__
#pragma clang diagnostic ignored "-Wreturn-local-addr"
#endif

class MaskedVByteCodec : public StatefulIntegerCodec<int32_t> {
 public:
  std::vector<uint8_t> compressed;

  void EncodeArray(const int32_t *in, const size_t length) override {
    size_t compsize = vbyte_encode(reinterpret_cast<const uint32_t *>(in),
                                   length, compressed.data());  // encoding
    compressed.resize(compsize);
  }

  void DecodeArray(int32_t *out, const std::size_t length) override {
    size_t compsize = compressed.size();
    assert(masked_vbyte_decode(compressed.data(),
                               reinterpret_cast<uint32_t *>(out),
                               length) == compsize);
  }

  std::size_t EncodedNumValues() override { return compressed.size(); }

  std::size_t EncodedSizeValue() override { return sizeof(uint8_t); }

  virtual ~MaskedVByteCodec() {}

  std::string name() const override { return "MASKEDVBYTE"; }

  std::size_t GetOverflowSize(size_t) const override { return 0; }

  StatefulIntegerCodec<int32_t> *CloneFresh() const override {
    return new MaskedVByteCodec();
  }

  void AllocEncoded(const int32_t *in, size_t length) override {
    compressed.resize((2 * length) * sizeof(int32_t));
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

/*
// MASKEDVBYTE DELTA
// does bitpacking after delta
// doesn't preseve input type :/
*/

class MaskedVByteDeltaCodec : public StatefulIntegerCodec<int32_t> {
 public:
  std::vector<uint8_t> compressed;

  void EncodeArray(const int32_t *in, const size_t length) override {
    size_t compsize =
        vbyte_encode_delta(reinterpret_cast<const uint32_t *>(in), length,
                           compressed.data(), /* prev */ 0);  // encoding
    compressed.resize(compsize);
  }

  void DecodeArray(int32_t *out, const std::size_t length) override {
    size_t compsize = compressed.size();
    assert(masked_vbyte_decode_delta(compressed.data(),
                                     reinterpret_cast<uint32_t *>(out), length,
                                     /* prev */ 0) == compsize);
  }

  std::size_t EncodedNumValues() override { return compressed.size(); }

  std::size_t EncodedSizeValue() override { return sizeof(uint8_t); }

  virtual ~MaskedVByteDeltaCodec() {}

  std::string name() const override { return "MASKEDVBYTE_DELTA"; }

  std::size_t GetOverflowSize(size_t) const override { return 0; }

  StatefulIntegerCodec<int32_t> *CloneFresh() const override {
    return new MaskedVByteDeltaCodec();
  }

  void AllocEncoded(const int32_t *in, size_t length) override {
    compressed.resize(2 * length * sizeof(int32_t));
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