#pragma once

#include <cassert>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "generic_codecs.h"
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
