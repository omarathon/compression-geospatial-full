#pragma once

#ifdef HAVE_SNAPPY

#include <snappy.h>

#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

#include "generic_codecs.h"

template <typename T>
class SnappyCodec : public StatefulIntegerCodec<T> {
 public:
  std::string compressed;

  void AllocEncoded(const T*, size_t) override { compressed.clear(); }

  void EncodeArray(const T* in, const size_t length) override {
    snappy::Compress(reinterpret_cast<const char*>(in), length * sizeof(T),
                     &compressed);
  }

  void DecodeArray(T* out, const size_t length) override {
    size_t uncompressedLen = 0;
    if (!snappy::GetUncompressedLength(compressed.data(), compressed.size(),
                                       &uncompressedLen))
      throw std::runtime_error("Snappy: GetUncompressedLength failed");
    if (uncompressedLen != length * sizeof(T))
      throw std::runtime_error("Snappy: decompressed size mismatch");
    if (!snappy::RawUncompress(compressed.data(), compressed.size(),
                               reinterpret_cast<char*>(out)))
      throw std::runtime_error("Snappy: decompression failed");
  }

  std::size_t EncodedNumValues() override { return compressed.size(); }
  std::size_t EncodedSizeValue() override { return sizeof(char); }
  std::string name() const override { return "Heavy_Snappy"; }
  std::size_t GetOverflowSize(size_t) const override { return 0; }
  StatefulIntegerCodec<T>* CloneFresh() const override { return new SnappyCodec<T>(); }

  void clear() override { compressed.clear(); compressed.shrink_to_fit(); }
  std::vector<T>& GetEncoded() override {
    throw std::runtime_error("Snappy: GetEncoded not supported");
  }
};

#endif  // HAVE_SNAPPY
