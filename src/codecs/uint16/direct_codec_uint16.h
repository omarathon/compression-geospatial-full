#pragma once

#include <cstdint>
#include <cstring>
#include <string>

#include "generic_codecs.h"

class DirectAccessCodecU16 : public StatefulIntegerCodec<uint16_t> {
 public:
  std::vector<uint16_t> compressed;

  void EncodeArray(const uint16_t* in, const size_t length) override {
    std::memcpy(compressed.data(), in, length * sizeof(uint16_t));
  }

  void DecodeArray(uint16_t* out, const std::size_t length) override {}

  std::size_t EncodedNumValues() override { return compressed.size(); }

  std::size_t EncodedSizeValue() override { return sizeof(uint16_t); }

  virtual ~DirectAccessCodecU16() {}

  std::string name() const override { return "custom_direct_access"; }

  std::size_t GetOverflowSize(size_t) const override { return 0; }

  StatefulIntegerCodec<uint16_t>* CloneFresh() const override {
    return new DirectAccessCodecU16();
  }

  void AllocEncoded(const uint16_t* in, size_t length) override {
    compressed.resize(length);
  };

  void clear() override {
    compressed.clear();
    compressed.shrink_to_fit();
  }

  std::vector<uint16_t>& GetEncoded() override { return compressed; };
};
