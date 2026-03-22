#pragma once

#include <cassert>
#include <cstdint>
#include <memory>
#include <string>

#include "FastPFor/headers/codecfactory.h"
#include "FastPFor/headers/codecs.h"
#include "generic_codecs.h"

using namespace FastPForLib;

// FastPFor variant that explicitly models the decode-time sum written to the
// two overflow slots by the fused CompositeCodec::_decodeArray.
// Used with AccessTransformation::LinearSumFused.
class FastPForFusedCodec : public StatefulIntegerCodec<int32_t> {
 private:
  std::shared_ptr<IntegerCODEC> codec;

 public:
  std::vector<uint32_t> compressed;

  FastPForFusedCodec(std::shared_ptr<IntegerCODEC> in_codec)
      : codec{std::move(in_codec)} {}

  FastPForFusedCodec(CODECFactory& codec_factory, std::string codec_name)
      : FastPForFusedCodec(codec_factory.getFromName(codec_name)) {}

  void EncodeArray(const int32_t* in, const size_t length) override {
    size_t compressed_size = compressed.size();
    codec->encodeArray(reinterpret_cast<const uint32_t*>(in), length,
                       compressed.data(), compressed_size);
    compressed.resize(compressed_size);
    compressed.shrink_to_fit();
  }

  void DecodeArray(int32_t* out, const std::size_t length) override {
    size_t recovered_size = length;
    codec->decodeArray(compressed.data(), compressed.size(),
                       reinterpret_cast<uint32_t*>(out), recovered_size);
    assert(recovered_size == length);
  }

  std::size_t EncodedNumValues() override { return compressed.size(); }

  std::size_t EncodedSizeValue() override { return sizeof(uint32_t); }

  virtual ~FastPForFusedCodec() {}

  std::string name() const override {
    return "FastPFor_fused_" + codec->name();
  }

  std::size_t GetOverflowSize(size_t) const override {
    return 32;  // Resources fail to be freed without `8`. `32` is required for
                // Simple16.
  }

  StatefulIntegerCodec<int32_t>* CloneFresh() const override {
    return new FastPForFusedCodec(codec);
  }

  void AllocEncoded(const int32_t* in, size_t length) override {
    compressed.resize(length * 2);
  };

  void clear() override {
    compressed.clear();
    compressed.shrink_to_fit();
  }

  std::vector<int32_t>& GetEncoded() override {
    return reinterpret_cast<std::vector<int32_t>&>(compressed);
  };
};
