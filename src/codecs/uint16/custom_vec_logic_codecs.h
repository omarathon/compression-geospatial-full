#pragma once

#include <nmmintrin.h>  // SSE4.2 Intrinsics

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

#include "generic_codecs.h"

class DeltaCodecSSE42U16 : public StatefulIntegerCodec<uint16_t> {
 private:
  std::vector<uint16_t> compressed_data;

 public:
  void EncodeArray(const uint16_t* in, const size_t length) override {
    if (length == 0) return;

    // First element: treat as delta from 0, zigzag encode
    {
      uint16_t d = in[0];  // delta from 0
      compressed_data[0] = (d << 1) ^ static_cast<uint16_t>(
                               static_cast<int16_t>(d) >> 15);
    }

    __m128i prev = _mm_set1_epi16(static_cast<int16_t>(in[0]));
    size_t i = 1;
    for (; i + 8 <= length; i += 8) {
      __m128i current =
          _mm_loadu_si128(reinterpret_cast<const __m128i*>(in + i));
      // Subtraction wraps mod 65536 — correct for both signed/unsigned
      __m128i delta = _mm_sub_epi16(current, prev);

      // Zigzag: (delta << 1) ^ (delta >> 15)  [arithmetic shift]
      __m128i shift1 = _mm_slli_epi16(delta, 1);
      __m128i shiftR = _mm_srai_epi16(delta, 15);
      __m128i zigzag = _mm_xor_si128(shift1, shiftR);

      _mm_storeu_si128(reinterpret_cast<__m128i*>(&compressed_data[i]), zigzag);

      // Broadcast last element of current as prev for next iteration
      prev = _mm_shufflehi_epi16(current, _MM_SHUFFLE(3, 3, 3, 3));
      prev = _mm_unpackhi_epi64(prev, prev);
    }

    for (; i < length; ++i) {
      // Modular uint16 subtraction — wraps correctly
      uint16_t d = in[i] - in[i - 1];
      compressed_data[i] = (d << 1) ^ static_cast<uint16_t>(
                               static_cast<int16_t>(d) >> 15);
    }
  }

  void DecodeArray(uint16_t* out, const size_t length) override {
    if (length == 0) return;

    // First element: zigzag decode
    {
      uint16_t z = compressed_data[0];
      out[0] = (z >> 1) ^ static_cast<uint16_t>(-(z & 1));
    }

    __m128i prev = _mm_set1_epi16(static_cast<int16_t>(out[0]));
    size_t i = 1;
    for (; i + 8 <= length; i += 8) {
      __m128i zigzag = _mm_loadu_si128(
          reinterpret_cast<const __m128i*>(&compressed_data[i]));

      // Zigzag decode: (z >> 1) ^ -(z & 1)  — logical shift for unsigned
      __m128i shiftR = _mm_srli_epi16(zigzag, 1);
      __m128i mask = _mm_and_si128(zigzag, _mm_set1_epi16(1));
      __m128i negate = _mm_sub_epi16(_mm_setzero_si128(), mask);
      __m128i delta = _mm_xor_si128(shiftR, negate);

      // Modular add recovers original
      __m128i current = _mm_add_epi16(delta, prev);
      _mm_storeu_si128(reinterpret_cast<__m128i*>(out + i), current);

      prev = _mm_shufflehi_epi16(current, _MM_SHUFFLE(3, 3, 3, 3));
      prev = _mm_unpackhi_epi64(prev, prev);
    }

    for (; i < length; ++i) {
      uint16_t z = compressed_data[i];
      // Logical right shift (z is unsigned, so >> is logical)
      uint16_t delta = (z >> 1) ^ static_cast<uint16_t>(
                           -static_cast<uint16_t>(z & 1));
      out[i] = out[i - 1] + delta;
    }
  }

  std::size_t EncodedNumValues() override { return compressed_data.size(); }
  std::size_t EncodedSizeValue() override { return sizeof(uint16_t); }
  virtual ~DeltaCodecSSE42U16() {}
  std::string name() const override { return "custom_delta_vecsse"; }
  std::size_t GetOverflowSize(size_t) const override { return 0; }

  StatefulIntegerCodec<uint16_t>* CloneFresh() const override {
    return new DeltaCodecSSE42U16();
  }

  void AllocEncoded(const uint16_t*, size_t length) override {
    compressed_data.resize(length);
  }

  void clear() override {
    compressed_data.clear();
    compressed_data.shrink_to_fit();
  }

  std::vector<uint16_t>& GetEncoded() override { return compressed_data; }
};

class FORCodecSSE42U16 : public StatefulIntegerCodec<uint16_t> {
 private:
  std::vector<uint16_t> compressed_data;

 public:
  FORCodecSSE42U16() {}

  void EncodeArray(const uint16_t* in, const size_t length) override {
    if (length == 0) return;

    // Find minimum using SSE
    __m128i minValVec = _mm_set1_epi16(static_cast<int16_t>(0xFFFF));
    size_t i = 0;
    for (; i + 8 <= length; i += 8) {
      __m128i current =
          _mm_loadu_si128(reinterpret_cast<const __m128i*>(in + i));
      minValVec = _mm_min_epu16(minValVec, current);
    }
    uint16_t minBuffer[8];
    _mm_storeu_si128(reinterpret_cast<__m128i*>(minBuffer), minValVec);
    uint16_t referenceValue = minBuffer[0];
    for (int j = 1; j < 8; ++j)
      referenceValue = std::min(referenceValue, minBuffer[j]);
    for (; i < length; ++i)
      referenceValue = std::min(referenceValue, in[i]);

    compressed_data[0] = referenceValue;

    // Subtract reference — diffs are non-negative uint16, no zigzag needed
    __m128i refValVec = _mm_set1_epi16(static_cast<int16_t>(referenceValue));
    i = 0;
    for (; i + 8 <= length; i += 8) {
      __m128i current =
          _mm_loadu_si128(reinterpret_cast<const __m128i*>(in + i));
      __m128i diff = _mm_sub_epi16(current, refValVec);
      _mm_storeu_si128(reinterpret_cast<__m128i*>(&compressed_data[i + 1]),
                       diff);
    }
    for (; i < length; ++i) {
      compressed_data[i + 1] = in[i] - referenceValue;
    }
  }

  void DecodeArray(uint16_t* out, const size_t length) override {
    if (length == 0) return;

    uint16_t referenceValue = compressed_data[0];
    __m128i refValVec = _mm_set1_epi16(static_cast<int16_t>(referenceValue));

    size_t i = 0;
    for (; i + 8 <= length; i += 8) {
      __m128i diff = _mm_loadu_si128(
          reinterpret_cast<const __m128i*>(&compressed_data[i + 1]));
      __m128i originalValues = _mm_add_epi16(diff, refValVec);
      _mm_storeu_si128(reinterpret_cast<__m128i*>(out + i), originalValues);
    }
    for (; i < length; ++i) {
      out[i] = compressed_data[i + 1] + referenceValue;
    }
  }

  std::size_t EncodedNumValues() override { return compressed_data.size(); }
  std::size_t EncodedSizeValue() override { return sizeof(uint16_t); }
  virtual ~FORCodecSSE42U16() {}
  std::string name() const override { return "custom_for_vecsse"; }
  std::size_t GetOverflowSize(size_t) const override { return 0; }

  StatefulIntegerCodec<uint16_t>* CloneFresh() const override {
    return new FORCodecSSE42U16();
  }

  void AllocEncoded(const uint16_t*, size_t length) override {
    compressed_data.resize(length + 1);  // +1 for the reference value
  }

  void clear() override { compressed_data.clear(); }

  std::vector<uint16_t>& GetEncoded() override { return compressed_data; }
};

class RLECodecSSE42U16 : public StatefulIntegerCodec<uint16_t> {
 private:
  std::vector<uint16_t> compressed_data;

 public:
  RLECodecSSE42U16() {}

  void EncodeArray(const uint16_t* in, const size_t length) override {
    if (length == 0) return;

    size_t i = 0;
    while (i < length) {
      uint16_t currentValue = in[i];
      size_t runLength = 1;

      // SSE comparison: 8 uint16 at a time
      for (; (i + runLength + 7) < length; runLength += 8) {
        __m128i currentVec = _mm_loadu_si128(
            reinterpret_cast<const __m128i*>(in + i + runLength - 1));
        __m128i nextVec = _mm_loadu_si128(
            reinterpret_cast<const __m128i*>(in + i + runLength));
        __m128i cmpResult = _mm_cmpeq_epi16(currentVec, nextVec);

        int mask = _mm_movemask_epi8(cmpResult);
        if (mask != 0xFFFF) {  // Not all equal
          // Each 16-bit comparison produces 2 mask bytes, find first mismatch
          int first_diff = __builtin_ctz(~mask) / 2;
          runLength += first_diff;
          break;
        }
      }

      if (i + runLength > length) {
        runLength = length - i;
      }

      while (i + runLength < length && in[i + runLength] == currentValue) {
        ++runLength;
      }

      compressed_data.push_back(currentValue);
      compressed_data.push_back(static_cast<uint16_t>(runLength));
      i += runLength;
    }
    compressed_data.shrink_to_fit();
  }

  void DecodeArray(uint16_t* out, const size_t length) override {
    size_t outIndex = 0;
    for (size_t i = 0; i < compressed_data.size(); i += 2) {
      uint16_t value = compressed_data[i];
      size_t runLength = compressed_data[i + 1];

      __m128i val_vec = _mm_set1_epi16(static_cast<int16_t>(value));
      while (runLength >= 8) {
        _mm_storeu_si128(reinterpret_cast<__m128i*>(out + outIndex), val_vec);
        outIndex += 8;
        runLength -= 8;
      }

      for (size_t j = 0; j < runLength; ++j) {
        out[outIndex++] = value;
      }
    }
  }

  std::size_t EncodedNumValues() override { return compressed_data.size(); }
  std::size_t EncodedSizeValue() override { return sizeof(uint16_t); }
  virtual ~RLECodecSSE42U16() {}
  std::string name() const override { return "custom_rle_vecsse"; }
  std::size_t GetOverflowSize(size_t) const override { return 0; }

  StatefulIntegerCodec<uint16_t>* CloneFresh() const override {
    return new RLECodecSSE42U16();
  }

  void AllocEncoded(const uint16_t*, size_t length) override {
    compressed_data.resize(2 * (length / 8 + 1));
  }

  void clear() override {
    compressed_data.clear();
    compressed_data.shrink_to_fit();
  }

  std::vector<uint16_t>& GetEncoded() override { return compressed_data; }
};
