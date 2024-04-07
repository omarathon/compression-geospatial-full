#include "generic_codecs.h"
#include <cstdint>
#include <string>
#include <iostream>
#include <cstring>
#include <memory>

#include "FastPFor/headers/codecs.h"
#include "FastPFor/headers/codecfactory.h"

using namespace FastPForLib;

class FastPForCodec : public StatefulIntegerCodec<int32_t> {
private:
    std::shared_ptr<IntegerCODEC> codec;

public:
    
  std::vector<uint32_t> compressed;

  FastPForCodec(std::shared_ptr<IntegerCODEC> in_codec) 
  : codec{std::move(in_codec)} {}

  FastPForCodec(CODECFactory& codec_factory, std::string codec_name) 
  : FastPForCodec(codec_factory.getFromName(codec_name)) {}

  void encodeArray(const int32_t *in, const size_t length) override {
    size_t compressed_size = compressed.size();
    codec->encodeArray(reinterpret_cast<const uint32_t *>(in), length, compressed.data(), compressed_size);
    // Optional shrinking - NOTE: need to store the size if not shrinking
    compressed.resize(compressed_size);
    compressed.shrink_to_fit();
  }

  void decodeArray(int32_t *out, const std::size_t length) override {
    size_t recovered_size = length;
    codec->decodeArray(
        compressed.data(), compressed.size(), 
        reinterpret_cast<uint32_t *>(out), recovered_size);
    assert(recovered_size == length);
  }

  std::size_t encodedNumValues() override {
    return compressed.size();
  }

  std::size_t encodedSizeValue() override {
    return sizeof(uint32_t);
  }

  virtual ~FastPForCodec() {}

  std::string name() const override {
    return "FastPFor_" + codec->name();
  }

  std::size_t getOverflowSize(size_t) const override {
    return 32; // Resources fail to be freed without `8`. `32` is required for Simple16.
  }

  StatefulIntegerCodec<int32_t>* cloneFresh() const override {
    return new FastPForCodec(codec);
  }

  
  void allocEncoded(const int32_t* in, size_t length) override {
    compressed.resize(length * 2); // Emirically found that this works. NOTE: reserve doesn't work.
  };

  void clear() override {
      compressed.clear();
      compressed.shrink_to_fit();
  }

  std::vector<int32_t>& getEncoded() override {
      return reinterpret_cast<std::vector<int32_t>&>(compressed);
  };
};