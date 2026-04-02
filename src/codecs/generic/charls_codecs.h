#pragma once

#ifdef HAVE_CHARLS

#include <charls/charls.h>

#include <cstdint>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <vector>

#include "generic_codecs.h"

// Lossless JPEG-LS codec via CharLS.
// uint16_t → width=length, height=1, bitsPerSample=16, 1 component.
// int32_t  → width=length*2, height=1, bitsPerSample=16, 1 component
//            (each int32 treated as 2 uint16 values).

template <typename T>
class CharLSCodec : public StatefulIntegerCodec<T> {
 public:
  std::vector<uint8_t> compressed;

  void AllocEncoded(const T*, size_t) override {}

  void EncodeArray(const T* in, const size_t length) override {
    charls::jpegls_encoder encoder;
    if constexpr (std::is_same_v<T, uint16_t>) {
      encoder.frame_info({static_cast<uint32_t>(length), 1, 16, 1});
      compressed.resize(encoder.estimated_destination_size());
      encoder.destination(compressed.data(), compressed.size());
      size_t encoded = encoder.encode(in, length * sizeof(uint16_t));
      compressed.resize(encoded);
    } else {
      // int32_t: explicitly split into (low16, high16) pairs — portable byte order
      std::vector<uint16_t> samples(length * 2);
      for (size_t i = 0; i < length; i++) {
        uint32_t v = static_cast<uint32_t>(in[i]);
        samples[i * 2 + 0] = static_cast<uint16_t>(v & 0xFFFF);
        samples[i * 2 + 1] = static_cast<uint16_t>((v >> 16) & 0xFFFF);
      }
      encoder.frame_info({static_cast<uint32_t>(length * 2), 1, 16, 1});
      compressed.resize(encoder.estimated_destination_size());
      encoder.destination(compressed.data(), compressed.size());
      size_t encoded = encoder.encode(samples.data(), samples.size() * sizeof(uint16_t));
      compressed.resize(encoded);
    }
  }

  void DecodeArray(T* out, const size_t length) override {
    charls::jpegls_decoder decoder;
    decoder.source(compressed.data(), compressed.size());
    decoder.read_header();
    const auto fi = decoder.frame_info();
    if constexpr (std::is_same_v<T, uint16_t>) {
      if (fi.width != static_cast<uint32_t>(length) || fi.height != 1 ||
          fi.bits_per_sample != 16 || fi.component_count != 1)
        throw std::runtime_error("CharLS: header mismatch on decode");
      decoder.decode(out, length * sizeof(uint16_t));
    } else {
      if (fi.width != static_cast<uint32_t>(length * 2) || fi.height != 1 ||
          fi.bits_per_sample != 16 || fi.component_count != 1)
        throw std::runtime_error("CharLS: header mismatch on decode");
      std::vector<uint16_t> samples(length * 2);
      decoder.decode(samples.data(), samples.size() * sizeof(uint16_t));
      for (size_t i = 0; i < length; i++) {
        uint32_t lo = samples[i * 2 + 0];
        uint32_t hi = samples[i * 2 + 1];
        out[i] = static_cast<int32_t>(lo | (hi << 16));
      }
    }
  }

  std::size_t EncodedNumValues() override { return compressed.size(); }
  std::size_t EncodedSizeValue() override { return sizeof(uint8_t); }
  std::string name() const override { return "Heavy_CharLS"; }
  std::size_t GetOverflowSize(size_t) const override { return 0; }
  StatefulIntegerCodec<T>* CloneFresh() const override { return new CharLSCodec<T>(); }

  void clear() override { compressed.clear(); compressed.shrink_to_fit(); }
  std::vector<T>& GetEncoded() override {
    throw std::runtime_error("CharLS: GetEncoded not supported");
  }
};

#endif  // HAVE_CHARLS
