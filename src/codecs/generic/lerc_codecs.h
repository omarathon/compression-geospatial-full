#pragma once

#ifdef HAVE_LERC

#include <Lerc_c_api.h>
#include <Lerc_types.h>

#include <cstdint>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <vector>

#include "generic_codecs.h"

// Lossless LERC codec (maxZErr = 0).
// Encodes a 1D raster band as a single-band, single-row image.
// uint16_t → LERC dataType ushort (3)
// int32_t  → LERC dataType int (4)

template <typename T>
class LERCCodec : public StatefulIntegerCodec<T> {
  static constexpr unsigned int kDataType() {
    if constexpr (std::is_same_v<T, uint16_t>) return 3;  // ushort
    else return 4;                                          // int
  }

 public:
  std::vector<uint8_t> compressed;

  void AllocEncoded(const T*, size_t) override {}

  void EncodeArray(const T* in, const size_t length) override {
    // Query exact compressed size before encoding
    unsigned int numBytesNeeded = 0;
    lerc_status status = lerc_computeCompressedSize(
        in,
        kDataType(),
        1,                         // nDepth
        static_cast<int>(length),  // nCols
        1,                         // nRows
        1,                         // nBands
        0,                         // nMasks (all valid)
        nullptr,                   // pValidBytes
        0.0,                       // maxZErr = 0 → lossless
        &numBytesNeeded);
    if (status != 0)
      throw std::runtime_error("LERC: computeCompressedSize failed with status " +
                               std::to_string(status));
    compressed.resize(numBytesNeeded);

    unsigned int nBytesWritten = 0;
    status = lerc_encode(
        in,
        kDataType(),
        1,
        static_cast<int>(length),
        1,
        1,
        0,
        nullptr,
        0.0,
        compressed.data(),
        static_cast<unsigned int>(compressed.size()),
        &nBytesWritten);
    if (status != 0)
      throw std::runtime_error("LERC: encode failed with status " +
                               std::to_string(status));
    compressed.resize(nBytesWritten);
  }

  void DecodeArray(T* out, const size_t length) override {
    lerc_status status = lerc_decode(
        compressed.data(),
        static_cast<unsigned int>(compressed.size()),
        0,                         // nMasks
        nullptr,                   // pValidBytes
        1,                         // nDepth
        static_cast<int>(length),  // nCols
        1,                         // nRows
        1,                         // nBands
        kDataType(),
        out);
    if (status != 0)
      throw std::runtime_error("LERC: decode failed with status " +
                               std::to_string(status));
    (void)length;
  }

  std::size_t EncodedNumValues() override { return compressed.size(); }
  std::size_t EncodedSizeValue() override { return sizeof(uint8_t); }
  std::string name() const override { return "Heavy_LERC"; }
  std::size_t GetOverflowSize(size_t) const override { return 0; }
  StatefulIntegerCodec<T>* CloneFresh() const override { return new LERCCodec<T>(); }

  void clear() override { compressed.clear(); compressed.shrink_to_fit(); }
  std::vector<T>& GetEncoded() override {
    throw std::runtime_error("LERC: GetEncoded not supported");
  }
};

#endif  // HAVE_LERC
