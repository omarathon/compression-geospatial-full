#include <immintrin.h>  // AVX2 Intrinsics
#include <limits.h>
#include <nmmintrin.h>  // SSE4.2 Intrinsics

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <memory>
#include <string>

class DeltaCodecSSE42 : public StatefulIntegerCodec<int32_t> {
 private:
  std::vector<int32_t> compressed_data;

 public:
  void EncodeArray(const int32_t* in, const size_t length) override {
    if (length == 0) return;

    compressed_data[0] = (in[0] << 1) ^ (in[0] >> 31);

    __m128i prev = _mm_set1_epi32(in[0]);
    size_t i = 1;
    for (; i < length - 4; i += 4) {
      __m128i current =
          _mm_loadu_si128(reinterpret_cast<const __m128i*>(in + i));
      __m128i delta = _mm_sub_epi32(current, prev);

      __m128i shift1 = _mm_slli_epi32(delta, 1);
      __m128i shiftR = _mm_srai_epi32(delta, 31);
      __m128i zigzag = _mm_xor_si128(shift1, shiftR);

      // Store directly into compressed_data
      _mm_storeu_si128(reinterpret_cast<__m128i*>(&compressed_data[i]), zigzag);

      prev = _mm_shuffle_epi32(current, _MM_SHUFFLE(3, 3, 3, 3));
    }

    for (; i < length; ++i) {
      int32_t delta = in[i] - in[i - 1];
      compressed_data[i] = (delta << 1) ^ (delta >> 31);
    }
  }

  void DecodeArray(int32_t* out, const size_t length) override {
    if (length == 0) return;

    out[0] = (compressed_data[0] >> 1) ^ (-(compressed_data[0] & 1));

    __m128i prev = _mm_set1_epi32(out[0]);
    size_t i = 1;
    for (; i < length - 4; i += 4) {
      __m128i zigzag = _mm_loadu_si128(
          reinterpret_cast<const __m128i*>(&compressed_data[i]));

      __m128i shiftR = _mm_srai_epi32(zigzag, 1);
      __m128i mask = _mm_and_si128(zigzag, _mm_set1_epi32(1));
      __m128i negate = _mm_sub_epi32(_mm_setzero_si128(), mask);
      __m128i delta = _mm_xor_si128(shiftR, negate);

      __m128i current = _mm_add_epi32(delta, prev);
      _mm_storeu_si128(reinterpret_cast<__m128i*>(out + i), current);

      prev = _mm_shuffle_epi32(current, _MM_SHUFFLE(3, 3, 3, 3));
    }

    for (; i < length; ++i) {
      int32_t delta = (compressed_data[i] >> 1) ^ (-(compressed_data[i] & 1));
      out[i] = delta + out[i - 1];
    }
  }

  std::size_t EncodedNumValues() override { return compressed_data.size(); }

  std::size_t EncodedSizeValue() override { return sizeof(int32_t); }

  virtual ~DeltaCodecSSE42() {}

  std::string name() const override { return "custom_delta_vecsse"; }

  std::size_t GetOverflowSize(size_t) const override { return 0; }

  StatefulIntegerCodec<int32_t>* CloneFresh() const override {
    return new DeltaCodecSSE42();
  }

  void AllocEncoded(const int32_t* in, size_t length) override {
    compressed_data.resize(length);
  };

  void clear() override {
    compressed_data.clear();
    compressed_data.shrink_to_fit();
  }

  std::vector<int32_t>& GetEncoded() override { return compressed_data; };
};

class DeltaCodecAVX2 : public StatefulIntegerCodec<int32_t> {
 private:
  std::vector<int32_t> compressed_data;

 public:
  void EncodeArray(const int32_t* in, const size_t length) override {
    if (length < 8) {
      std::copy(in, in + length, compressed_data.begin());
      return;
    }

    for (size_t i = 0; i < 8; ++i) {
      compressed_data[i] = (in[i] << 1) ^ (in[i] >> 31);
    }

    __m256i prev = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(in));
    size_t i = 8;
    for (; i < length - 8; i += 8) {
      __m256i current =
          _mm256_loadu_si256(reinterpret_cast<const __m256i*>(in + i));
      __m256i delta = _mm256_sub_epi32(current, prev);

      __m256i sign = _mm256_srai_epi32(delta, 31);
      delta = _mm256_xor_si256(_mm256_add_epi32(delta, delta), sign);

      _mm256_storeu_si256(
          reinterpret_cast<__m256i*>(compressed_data.data() + i), delta);
      prev = _mm256_permute4x64_epi64(current, _MM_SHUFFLE(3, 3, 3, 3));
    }

    for (; i < length; ++i) {
      int32_t delta = in[i] - in[i - 1];
      compressed_data[i] = (delta << 1) ^ (delta >> 31);
    }
  }

  void DecodeArray(int32_t* out, const size_t length) override {
    if (length < 8) {
      std::copy(compressed_data.begin(), compressed_data.begin() + length, out);
      return;
    }

    for (size_t i = 0; i < 8; ++i) {
      int32_t value = compressed_data[i];
      out[i] = (value >> 1) ^ (-(value & 1));
    }

    __m256i prev = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(out));
    size_t i = 8;
    for (; i < length - 8; i += 8) {
      __m256i delta = _mm256_loadu_si256(
          reinterpret_cast<const __m256i*>(compressed_data.data() + i));

      __m256i sign = _mm256_and_si256(delta, _mm256_set1_epi32(1));
      sign = _mm256_sub_epi32(_mm256_setzero_si256(), sign);
      delta = _mm256_xor_si256(_mm256_srli_epi32(delta, 1), sign);

      __m256i current = _mm256_add_epi32(delta, prev);
      _mm256_storeu_si256(reinterpret_cast<__m256i*>(out + i), current);
      prev = _mm256_permute4x64_epi64(current, _MM_SHUFFLE(3, 3, 3, 3));
    }

    for (; i < length; ++i) {
      int32_t value = compressed_data[i];
      out[i] = out[i - 1] + ((value >> 1) ^ (-(value & 1)));
    }
  }

  std::size_t EncodedNumValues() override { return compressed_data.size(); }

  std::size_t EncodedSizeValue() override { return sizeof(int32_t); }

  virtual ~DeltaCodecAVX2() {}

  std::string name() const override { return "custom_delta_vecavx"; }

  std::size_t GetOverflowSize(size_t) const override { return 0; }

  StatefulIntegerCodec<int32_t>* CloneFresh() const override {
    return new DeltaCodecAVX2();
  }

  void AllocEncoded(const int32_t* in, size_t length) override {
    compressed_data.resize(length);
  };

  void clear() override {
    compressed_data.clear();
    compressed_data.shrink_to_fit();
  }

  std::vector<int32_t>& GetEncoded() override { return compressed_data; };
};

class DeltaCodecAVX512 : public StatefulIntegerCodec<int32_t> {
 private:
  std::vector<int32_t> compressed_data;

 public:
  DeltaCodecAVX512() {}

  void EncodeArray(const int32_t* in, const size_t length) override {
    if (length == 0) return;

    size_t i = 0;
    if (length >= 16) {
      for (size_t i = 0; i < std::min(size_t(16), length); ++i) {
        compressed_data[i] = (in[i] << 1) ^ (in[i] >> 31);
      }

      __m512i prev = _mm512_loadu_si512(reinterpret_cast<const __m512i*>(in));

      i = 16;
      for (; i < length - 16; i += 16) {
        __m512i current =
            _mm512_loadu_si512(reinterpret_cast<const __m512i*>(in + i));
        __m512i delta = _mm512_sub_epi32(current, prev);

        __m512i sign = _mm512_srai_epi32(delta, 31);
        delta = _mm512_xor_si512(_mm512_add_epi32(delta, delta), sign);

        _mm512_storeu_si512(
            reinterpret_cast<__m512i*>(compressed_data.data() + i), delta);
        prev = _mm512_permutexvar_epi32(_mm512_set1_epi32(15), current);
      }
    }
    for (; i < length; ++i) {
      int32_t delta = in[i] - (i > 0 ? in[i - 1] : 0);
      compressed_data[i] = (delta << 1) ^ (delta >> 31);
    }
  }

  void DecodeArray(int32_t* out, const size_t length) override {
    if (length == 0) return;

    size_t i = 0;

    if (length >= 16) {
      for (size_t i = 0; i < std::min(size_t(16), length); ++i) {
        int32_t value = compressed_data[i];
        out[i] = (value >> 1) ^ (-(value & 1));
      }

      __m512i prev = _mm512_loadu_si512(reinterpret_cast<const __m512i*>(out));
      i = 16;
      for (; i < length - 16; i += 16) {
        __m512i delta = _mm512_loadu_si512(
            reinterpret_cast<const __m512i*>(compressed_data.data() + i));

        __m512i sign = _mm512_and_si512(delta, _mm512_set1_epi32(1));
        sign = _mm512_sub_epi32(_mm512_setzero_si512(), sign);
        delta = _mm512_xor_si512(_mm512_srli_epi32(delta, 1), sign);

        __m512i current = _mm512_add_epi32(delta, prev);
        _mm512_storeu_si512(reinterpret_cast<__m512i*>(out + i), current);
        prev = _mm512_permutexvar_epi32(_mm512_set1_epi32(15), current);
      }
    }

    for (; i < length; ++i) {
      int32_t value = compressed_data[i];
      out[i] = (i > 0 ? out[i - 1] : 0) + ((value >> 1) ^ (-(value & 1)));
    }
  }

  std::size_t EncodedNumValues() override { return compressed_data.size(); }

  std::size_t EncodedSizeValue() override { return sizeof(int32_t); }

  virtual ~DeltaCodecAVX512() {}

  std::string name() const override { return "custom_delta_vecavx512"; }

  std::size_t GetOverflowSize(size_t) const override { return 0; }

  StatefulIntegerCodec<int32_t>* CloneFresh() const override {
    return new DeltaCodecAVX512();
  }

  void AllocEncoded(const int32_t* in, size_t length) override {
    compressed_data.resize(length);
  }

  void clear() override { compressed_data.clear(); }

  std::vector<int32_t>& GetEncoded() override { return compressed_data; }
};

// Following Damme et al.
class FORCodecSSE42 : public StatefulIntegerCodec<int32_t> {
 private:
  std::vector<int32_t> compressed_data;

 public:
  FORCodecSSE42() {}

  void EncodeArray(const int32_t* in, const size_t length) override {
    if (length == 0) return;

    __m128i minValVec = _mm_set1_epi32(INT_MAX);
    int i = 0;
    for (; i < length - 4; i += 4) {
      __m128i current =
          _mm_loadu_si128(reinterpret_cast<const __m128i*>(in + i));
      minValVec = _mm_min_epi32(minValVec, current);
    }
    int32_t minBuffer[4];
    _mm_storeu_si128(reinterpret_cast<__m128i*>(minBuffer), minValVec);
    int32_t referenceValue =
        std::min({minBuffer[0], minBuffer[1], minBuffer[2], minBuffer[3]});
    for (; i < length; ++i) {
      referenceValue = std::min(referenceValue, in[i]);
    }

    compressed_data[0] = referenceValue;

    __m128i refValVec = _mm_set1_epi32(referenceValue);
    i = 0;
    for (; i < length - 4; i += 4) {
      __m128i current =
          _mm_loadu_si128(reinterpret_cast<const __m128i*>(in + i));
      __m128i diff = _mm_sub_epi32(current, refValVec);

      __m128i shiftLeft = _mm_slli_epi32(diff, 1);
      __m128i shiftRight = _mm_srai_epi32(diff, 31);
      __m128i zigzagEncoded = _mm_xor_si128(shiftLeft, shiftRight);

      _mm_storeu_si128(reinterpret_cast<__m128i*>(&compressed_data[i + 1]),
                       zigzagEncoded);
    }
    for (; i < length; ++i) {
      int32_t diff = in[i] - referenceValue;
      compressed_data[i + 1] = (diff << 1) ^ (diff >> 31);
    }
  }

  void DecodeArray(int32_t* out, const size_t length) override {
    if (length == 0) return;

    int32_t referenceValue = compressed_data[0];
    __m128i refValVec = _mm_set1_epi32(referenceValue);

    int i = 0;
    for (; i < length - 4; i += 4) {
      __m128i zigzagEncoded = _mm_loadu_si128(
          reinterpret_cast<const __m128i*>(&compressed_data[i + 1]));

      // Zig-zag decode
      __m128i shiftRight = _mm_srai_epi32(zigzagEncoded, 1);
      __m128i negate = _mm_and_si128(zigzagEncoded, _mm_set1_epi32(1));
      negate = _mm_sub_epi32(_mm_setzero_si128(), negate);
      __m128i diff = _mm_xor_si128(shiftRight, negate);

      __m128i originalValues = _mm_add_epi32(diff, refValVec);
      _mm_storeu_si128(reinterpret_cast<__m128i*>(out + i), originalValues);
    }

    // Process remaining elements
    for (; i < length; ++i) {
      int32_t encoded = compressed_data[i + 1];
      // Inline Zig-Zag decoding
      int32_t delta = (encoded >> 1) ^ -(encoded & 1);
      out[i] = delta + referenceValue;
    }
  }

  std::size_t EncodedNumValues() override { return compressed_data.size(); }

  std::size_t EncodedSizeValue() override { return sizeof(int32_t); }

  virtual ~FORCodecSSE42() {}

  std::string name() const override { return "custom_for_vecsse"; }

  std::size_t GetOverflowSize(size_t) const override { return 0; }

  StatefulIntegerCodec<int32_t>* CloneFresh() const override {
    return new FORCodecSSE42();
  }

  void AllocEncoded(const int32_t* in, size_t length) override {
    compressed_data.resize(length + 1);  // +1 for the reference value
  }

  void clear() override { compressed_data.clear(); }

  std::vector<int32_t>& GetEncoded() override { return compressed_data; }
};

// Following Damme et al.
class FORCodecAVX2 : public StatefulIntegerCodec<int32_t> {
 private:
  std::vector<int32_t> compressed_data;

 public:
  FORCodecAVX2() {}

  void EncodeArray(const int32_t* in, const size_t length) override {
    if (length == 0) return;

    __m256i minValVec = _mm256_set1_epi32(INT_MAX);
    size_t i = 0;
    for (; i + 8 <= length; i += 8) {
      __m256i current =
          _mm256_loadu_si256(reinterpret_cast<const __m256i*>(in + i));
      minValVec = _mm256_min_epi32(minValVec, current);
    }

    int32_t minBuffer[8];
    _mm256_storeu_si256(reinterpret_cast<__m256i*>(minBuffer), minValVec);
    int32_t referenceValue =
        std::min({minBuffer[0], minBuffer[1], minBuffer[2], minBuffer[3],
                  minBuffer[4], minBuffer[5], minBuffer[6], minBuffer[7]});

    for (; i < length; ++i) {
      referenceValue = std::min(referenceValue, in[i]);
    }

    compressed_data[0] = referenceValue;

    __m256i refValVec = _mm256_set1_epi32(referenceValue);
    i = 0;
    for (; i + 8 <= length; i += 8) {
      __m256i current =
          _mm256_loadu_si256(reinterpret_cast<const __m256i*>(in + i));
      __m256i diff = _mm256_sub_epi32(current, refValVec);

      // Zig-zag encode the diff values
      __m256i shiftLeft = _mm256_slli_epi32(diff, 1);
      __m256i shiftRight = _mm256_srai_epi32(diff, 31);
      __m256i zigzagEncoded = _mm256_xor_si256(shiftLeft, shiftRight);

      _mm256_storeu_si256(reinterpret_cast<__m256i*>(&compressed_data[i + 1]),
                          zigzagEncoded);  // +1 to skip the reference value
    }

    for (; i < length; ++i) {
      int32_t diff = in[i] - referenceValue;
      compressed_data[i + 1] = (diff << 1) ^ (diff >> 31);  // Zig-zag encode
    }
  }

  void DecodeArray(int32_t* out, const size_t length) override {
    if (length == 0) return;

    int32_t referenceValue = compressed_data[0];
    __m256i refValVec = _mm256_set1_epi32(referenceValue);

    size_t i = 0;
    for (; i + 8 <= length; i += 8) {
      __m256i zigzagEncoded =
          _mm256_loadu_si256(reinterpret_cast<const __m256i*>(
              &compressed_data[i + 1]));  // +1 to skip the reference value

      // Zig-zag decode
      __m256i shiftRight = _mm256_srai_epi32(zigzagEncoded, 1);
      __m256i negate = _mm256_and_si256(zigzagEncoded, _mm256_set1_epi32(1));
      negate = _mm256_sub_epi32(_mm256_setzero_si256(), negate);
      __m256i diff = _mm256_xor_si256(shiftRight, negate);

      __m256i originalValues = _mm256_add_epi32(diff, refValVec);
      _mm256_storeu_si256(reinterpret_cast<__m256i*>(out + i), originalValues);
    }

    // Process remaining elements
    for (; i < length; ++i) {
      int32_t encoded = compressed_data[i + 1];
      int32_t delta = (encoded >> 1) ^ -(encoded & 1);
      out[i] = delta + referenceValue;
    }
  }

  std::size_t EncodedNumValues() override { return compressed_data.size(); }

  std::size_t EncodedSizeValue() override { return sizeof(int32_t); }

  virtual ~FORCodecAVX2() {}

  std::string name() const override { return "custom_for_vecavx"; }

  std::size_t GetOverflowSize(size_t) const override { return 0; }

  StatefulIntegerCodec<int32_t>* CloneFresh() const override {
    return new FORCodecAVX2();
  }

  void AllocEncoded(const int32_t* in, size_t length) override {
    compressed_data.resize(length + 1);
  };

  void clear() override {
    compressed_data.clear();
    compressed_data.shrink_to_fit();
  }

  std::vector<int32_t>& GetEncoded() override { return compressed_data; };
};

class FORCodecAVX512 : public StatefulIntegerCodec<int32_t> {
 private:
  std::vector<int32_t> compressed_data;

 public:
  FORCodecAVX512() {}

  void EncodeArray(const int32_t* in, const size_t length) override {
    if (length == 0) return;

    __m512i minValVec = _mm512_set1_epi32(INT_MAX);
    size_t i = 0;
    for (; i + 16 <= length; i += 16) {
      __m512i current =
          _mm512_loadu_si512(reinterpret_cast<const __m512i*>(in + i));
      minValVec = _mm512_min_epi32(minValVec, current);
    }

    int32_t minBuffer[16];
    _mm512_storeu_si512(reinterpret_cast<__m512i*>(minBuffer), minValVec);
    int32_t referenceValue = minBuffer[0];
    for (int j = 1; j < 16; ++j) {
      referenceValue = std::min(referenceValue, minBuffer[j]);
    }

    for (; i < length; ++i) {
      referenceValue = std::min(referenceValue, in[i]);
    }

    compressed_data[0] = referenceValue;

    __m512i refValVec = _mm512_set1_epi32(referenceValue);
    i = 0;
    for (; i + 16 <= length; i += 16) {
      __m512i current =
          _mm512_loadu_si512(reinterpret_cast<const __m512i*>(in + i));
      __m512i diff = _mm512_sub_epi32(current, refValVec);

      // Zig-Zag encode the diff values
      __m512i shiftLeft = _mm512_slli_epi32(diff, 1);
      __m512i shiftRight = _mm512_srai_epi32(diff, 31);
      __m512i zigzagEncoded = _mm512_xor_si512(shiftLeft, shiftRight);

      _mm512_storeu_si512(reinterpret_cast<__m512i*>(&compressed_data[i + 1]),
                          zigzagEncoded);  // +1 to skip the reference value
    }

    // Handle remaining elements
    for (; i < length; ++i) {
      int32_t diff = in[i] - referenceValue;
      compressed_data[i + 1] = (diff << 1) ^ (diff >> 31);  // Zig-Zag encode
    }
  }

  void DecodeArray(int32_t* out, const size_t length) override {
    if (length == 0) return;

    int32_t referenceValue = compressed_data[0];
    __m512i refValVec = _mm512_set1_epi32(referenceValue);

    size_t i = 0;
    for (; i + 16 <= length; i += 16) {
      __m512i zigzagEncoded =
          _mm512_loadu_si512(reinterpret_cast<const __m512i*>(
              &compressed_data[i + 1]));  // +1 to skip the reference value

      // Zig-Zag decode
      __m512i shiftRight = _mm512_srai_epi32(zigzagEncoded, 1);
      __m512i negate = _mm512_and_si512(zigzagEncoded, _mm512_set1_epi32(1));
      negate = _mm512_sub_epi32(_mm512_setzero_si512(), negate);
      __m512i diff = _mm512_xor_si512(shiftRight, negate);

      __m512i originalValues = _mm512_add_epi32(diff, refValVec);
      _mm512_storeu_si512(reinterpret_cast<__m512i*>(out + i), originalValues);
    }

    // Handle remaining elements
    for (; i < length; ++i) {
      int32_t encoded = compressed_data[i + 1];
      int32_t delta = (encoded >> 1) ^ -(encoded & 1);
      out[i] = delta + referenceValue;
    }
  }

  std::size_t EncodedNumValues() override { return compressed_data.size(); }

  std::size_t EncodedSizeValue() override { return sizeof(int32_t); }

  virtual ~FORCodecAVX512() {}

  std::string name() const override { return "custom_for_vecavx512"; }

  std::size_t GetOverflowSize(size_t) const override { return 0; }

  StatefulIntegerCodec<int32_t>* CloneFresh() const override {
    return new FORCodecAVX512();
  }

  void AllocEncoded(const int32_t* in, size_t length) override {
    compressed_data.resize(length + 1);  // +1 for the reference value
  }

  void clear() override { compressed_data.clear(); }

  std::vector<int32_t>& GetEncoded() override { return compressed_data; }
};

// Following Damme et al.
class RLECodecSSE42 : public StatefulIntegerCodec<int32_t> {
 private:
  std::vector<int32_t> compressed_data;

 public:
  RLECodecSSE42() {}

  void EncodeArray(const int32_t* in, const size_t length) override {
    if (length == 0) return;

    size_t i = 0;
    while (i < length) {
      int32_t currentValue = in[i];
      size_t runLength = 1;

      for (; (i + runLength + 3) < length; runLength += 4) {
        __m128i currentVec = _mm_loadu_si128(
            reinterpret_cast<const __m128i*>(in + i + runLength - 1));
        __m128i nextVec = _mm_loadu_si128(
            reinterpret_cast<const __m128i*>(in + i + runLength));
        __m128i cmpResult = _mm_cmpeq_epi32(currentVec, nextVec);

        int mask = _mm_movemask_ps(_mm_castsi128_ps(cmpResult));
        if (mask != 0xF) {  // Not all equal
          // Find the first unequal element in this chunk
          runLength += __builtin_ctz(~mask);  // Count trailing zeros in mask
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
      compressed_data.push_back(runLength);
      i += runLength;
    }
    compressed_data.shrink_to_fit();
  }

  void DecodeArray(int32_t* out, const size_t length) override {
    size_t outIndex = 0;
    for (size_t i = 0; i < compressed_data.size(); i += 2) {
      int32_t value = compressed_data[i];
      size_t runLength = compressed_data[i + 1];

      __m128i val_vec = _mm_set1_epi32(value);
      while (runLength >= 4) {
        _mm_storeu_si128(reinterpret_cast<__m128i*>(out + outIndex), val_vec);
        outIndex += 4;
        runLength -= 4;
      }

      for (size_t j = 0; j < runLength; ++j) {
        out[outIndex++] = value;
      }
    }
  }

  std::size_t EncodedNumValues() override { return compressed_data.size(); }

  std::size_t EncodedSizeValue() override { return sizeof(int32_t); }

  virtual ~RLECodecSSE42() {}

  std::string name() const override { return "custom_rle_vecsse"; }

  std::size_t GetOverflowSize(size_t) const override { return 0; }

  StatefulIntegerCodec<int32_t>* CloneFresh() const override {
    return new RLECodecSSE42();
  }

  void AllocEncoded(const int32_t* in, size_t length) override {
    compressed_data.resize(2 * (length / 4 + 1));
  };

  void clear() override {
    compressed_data.clear();
    compressed_data.shrink_to_fit();
  }

  std::vector<int32_t>& GetEncoded() override { return compressed_data; };
};

// Following Damme et al.
class RLECodecAVX2 : public StatefulIntegerCodec<int32_t> {
 private:
  std::vector<int32_t> compressed_data;

 public:
  RLECodecAVX2() {}

  void EncodeArray(const int32_t* in, const size_t length) override {
    if (length == 0) return;

    size_t i = 0;
    while (i < length) {
      int32_t currentValue = in[i];
      size_t runLength = 1;

      for (; (i + runLength + 7) < length; runLength += 8) {
        __m256i currentVec = _mm256_loadu_si256(
            reinterpret_cast<const __m256i*>(in + i + runLength - 1));
        __m256i nextVec = _mm256_loadu_si256(
            reinterpret_cast<const __m256i*>(in + i + runLength));
        __m256i cmpResult = _mm256_cmpeq_epi32(currentVec, nextVec);

        int mask = _mm256_movemask_ps(_mm256_castsi256_ps(cmpResult));
        if (mask != 0xFF) {  // Not all equal
          // Find the first unequal element in this chunk
          runLength += __builtin_ctz(~mask);
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
      compressed_data.push_back(runLength);
      i += runLength;
    }
    compressed_data.shrink_to_fit();
  }

  void DecodeArray(int32_t* out, const size_t length) override {
    size_t outIndex = 0;
    for (size_t i = 0; i < compressed_data.size(); i += 2) {
      int32_t value = compressed_data[i];
      size_t runLength = compressed_data[i + 1];

      // Vectorized filling of output array using AVX2
      __m256i val_vec = _mm256_set1_epi32(value);
      while (runLength >= 8) {
        _mm256_storeu_si256(reinterpret_cast<__m256i*>(out + outIndex),
                            val_vec);
        outIndex += 8;
        runLength -= 8;
      }

      // Handle remaining elements
      for (size_t j = 0; j < runLength; ++j) {
        out[outIndex++] = value;
      }
    }
  }

  std::size_t EncodedNumValues() override { return compressed_data.size(); }

  std::size_t EncodedSizeValue() override { return sizeof(int32_t); }

  virtual ~RLECodecAVX2() {}

  std::string name() const override { return "custom_rle_vecavx"; }

  std::size_t GetOverflowSize(size_t) const override { return 0; }

  StatefulIntegerCodec<int32_t>* CloneFresh() const override {
    return new RLECodecAVX2();
  }

  void AllocEncoded(const int32_t* in, size_t length) override {
    compressed_data.resize(2 * (length / 8 + 1));
  };

  void clear() override {
    compressed_data.clear();
    compressed_data.shrink_to_fit();
  }

  std::vector<int32_t>& GetEncoded() override { return compressed_data; };
};

class RLECodecAVX512 : public StatefulIntegerCodec<int32_t> {
 private:
  std::vector<int32_t> compressed_data;

 public:
  RLECodecAVX512() {}

  void EncodeArray(const int32_t* in, const size_t length) override {
    if (length == 0) return;

    size_t i = 0;
    while (i < length) {
      int32_t currentValue = in[i];
      size_t runLength = 1;

      // Vectorized comparison for adjacent elements using AVX-512
      for (; (i + runLength + 15) < length; runLength += 16) {
        __m512i currentVec = _mm512_loadu_si512(
            reinterpret_cast<const __m512i*>(in + i + runLength - 1));
        __m512i nextVec = _mm512_loadu_si512(
            reinterpret_cast<const __m512i*>(in + i + runLength));
        __mmask16 cmpResult = _mm512_cmpeq_epi32_mask(currentVec, nextVec);

        if (cmpResult != 0xFFFF) {  // Not all equal
          // Find the first unequal element in this chunk
          runLength += __builtin_ctz(~cmpResult);
          break;
        }
      }

      if (i + runLength > length) {
        runLength = length - i;
      }

      while (i + runLength < length && in[i + runLength] == currentValue) {
        ++runLength;
      }

      compressed_data.push_back(currentValue);  // Store the value
      compressed_data.push_back(runLength);     // Store the length of the run
      i += runLength;
    }
    compressed_data.shrink_to_fit();
  }

  void DecodeArray(int32_t* out, const size_t length) override {
    size_t outIndex = 0;
    for (size_t i = 0; i < compressed_data.size(); i += 2) {
      int32_t value = compressed_data[i];
      size_t runLength = compressed_data[i + 1];

      // Vectorized filling of output array using AVX-512
      __m512i val_vec = _mm512_set1_epi32(value);
      while (runLength >= 16) {
        _mm512_storeu_si512(reinterpret_cast<__m512i*>(out + outIndex),
                            val_vec);
        outIndex += 16;
        runLength -= 16;
      }

      // Handle remaining elements
      for (size_t j = 0; j < runLength; ++j) {
        out[outIndex++] = value;
      }
    }
  }

  std::size_t EncodedNumValues() override { return compressed_data.size(); }

  std::size_t EncodedSizeValue() override { return sizeof(int32_t); }

  virtual ~RLECodecAVX512() {}

  std::string name() const override { return "custom_rle_vecavx512"; }

  std::size_t GetOverflowSize(size_t) const override { return 0; }

  StatefulIntegerCodec<int32_t>* CloneFresh() const override {
    return new RLECodecAVX512();
  }

  void AllocEncoded(const int32_t* in, size_t length) override {
    compressed_data.resize(2 * (length / 16 + 1));  // Reserve more efficiently
  };

  void clear() override {
    compressed_data.clear();
    compressed_data.shrink_to_fit();
  }

  std::vector<int32_t>& GetEncoded() override { return compressed_data; }
};
