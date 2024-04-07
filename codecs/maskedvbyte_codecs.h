#include "generic_codecs.h"
#include <cstdint>
#include <string>
#include <iostream>
#include "varintencode.h"
#include "varintdecode.h"
#include <cassert>

#ifdef __clang__
#pragma clang diagnostic ignored "-Wreturn-local-addr"
#endif

class MaskedVByteCodec : public StatefulIntegerCodec<int32_t> {
public:

  std::vector<uint8_t> compressed;

  void encodeArray(const int32_t *in, const size_t length) override {
    size_t compsize = vbyte_encode(reinterpret_cast<const uint32_t *>(in), length, compressed.data()); // encoding
    compressed.resize(compsize);
  }

  void decodeArray(int32_t *out, const std::size_t length) override {
    size_t compsize = compressed.size();
    assert(
        masked_vbyte_decode(compressed.data(), reinterpret_cast<uint32_t *>(out), length)
        == compsize);
  }

  std::size_t encodedNumValues() override {
    return compressed.size();
  }

  std::size_t encodedSizeValue() override {
    return sizeof(uint8_t);
  }

  virtual ~MaskedVByteCodec() {}

  std::string name() const override {
    return "MASKEDVBYTE";
  }

  std::size_t getOverflowSize(size_t) const override {
    return 0;
  }

  StatefulIntegerCodec<int32_t>* cloneFresh() const override {
    return new MaskedVByteCodec();
  }

  void allocEncoded(const int32_t* in, size_t length) override {
    compressed.resize((2 * length) * sizeof(int32_t));
  };

  void clear() override {
      compressed.clear();
      compressed.shrink_to_fit();
  }

  std::vector<int32_t>& getEncoded() override {
      throw std::runtime_error("Encoded format does not match input. Cannot forward.");
      std::vector<int32_t> dummy{};
      return dummy;
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

  void encodeArray(const int32_t *in, const size_t length) override {
    size_t compsize = vbyte_encode_delta(reinterpret_cast<const uint32_t *>(in), length, compressed.data(), /* prev */ 0); // encoding
    compressed.resize(compsize);
  }

  void decodeArray(int32_t *out, const std::size_t length) override {
    size_t compsize = compressed.size();
    assert(
        masked_vbyte_decode_delta(compressed.data(), reinterpret_cast<uint32_t *>(out), length, /* prev */ 0)
        == compsize);
  }

  std::size_t encodedNumValues() override {
    return compressed.size();
  }

  std::size_t encodedSizeValue() override {
    return sizeof(uint8_t);
  }

  virtual ~MaskedVByteDeltaCodec() {}

  std::string name() const override {
    return "MASKEDVBYTE_DELTA";
  }

  std::size_t getOverflowSize(size_t) const override {
    return 0;
  }

  StatefulIntegerCodec<int32_t>* cloneFresh() const override {
    return new MaskedVByteDeltaCodec();
  }

  void allocEncoded(const int32_t* in, size_t length) override {
    compressed.resize(2 * length * sizeof(int32_t));
  };

  void clear() override {
      compressed.clear();
      compressed.shrink_to_fit();
  }

  std::vector<int32_t>& getEncoded() override {
      throw std::runtime_error("Encoded format does not match input. Cannot forward.");
      std::vector<int32_t> dummy{};
      return dummy;
  };
};