#include <zlib.h>

#include <cstdint>
#include <stdexcept>
#include <string>

#include "generic_codecs.h"

#ifdef __clang__
#pragma clang diagnostic ignored "-Wreturn-local-addr"
#endif

class DeflateCodec : public StatefulIntegerCodec<int32_t> {
 public:
  std::vector<uint8_t> compressed;

  void EncodeArray(const int32_t* in, const size_t length) override {
    uLongf outputSize = compressBound(length * sizeof(int32_t));
    if (compress2(compressed.data(), &outputSize,
                  reinterpret_cast<const Bytef*>(in), length * sizeof(int32_t),
                  Z_BEST_COMPRESSION) != Z_OK)
      throw std::runtime_error("DEFLATE compression failed");
    compressed.resize(outputSize);
  }

  void DecodeArray(int32_t* out, const std::size_t length) override {
    uLongf outputSize = length;
    uLongf compressedSize = length * sizeof(int32_t);

    if (uncompress(reinterpret_cast<Bytef*>(out), &compressedSize,
                   compressed.data(), compressed.size()) != Z_OK) {
      std::cerr << "DEFLATE decompression failed." << std::endl;
      return;
    }
  }

  std::size_t EncodedNumValues() override { return compressed.size(); }

  std::size_t EncodedSizeValue() override { return sizeof(uint8_t); }

  virtual ~DeflateCodec() {}

  std::string name() const override { return "DEFLATE"; }

  std::size_t GetOverflowSize(size_t) const override { return 0; }

  StatefulIntegerCodec<int32_t>* CloneFresh() const override {
    return new DeflateCodec();
  }

  void AllocEncoded(const int32_t* in, size_t length) override {
    uLongf outputSize = compressBound(length * sizeof(int32_t));
    compressed.resize(outputSize);  // NOTE: reserve doesn't work.
  };

  void clear() override {
    compressed.clear();
    compressed.shrink_to_fit();
  }

  std::vector<int32_t>& GetEncoded() override {
    throw std::runtime_error(
        "Encoded format does not match input. Cannot forward.");
  };
};