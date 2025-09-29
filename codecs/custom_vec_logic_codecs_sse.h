#include <immintrin.h> // AVX2 Intrinsics
#include <nmmintrin.h> // SSE4.2 Intrinsics

#include <cstdint>
#include <string>
#include <iostream>
#include <cstring>
#include <memory>
#include <algorithm>

#include <limits.h>

// NB: removed zigzag encoding - only work for positive integers!

class DeltaCodecSSE42NoZigZag : public StatefulIntegerCodec<int32_t> {
private:
    std::vector<int32_t> compressed_data;
public:
    void encodeArray(const int32_t* in, const size_t length) override {
        if (length == 0) return;

        compressed_data[0] = in[0];

        __m128i prev = _mm_set1_epi32(in[0]);
        size_t i = 1;
        for (; i < length - 4; i += 4) {
            __m128i current = _mm_loadu_si128(reinterpret_cast<const __m128i*>(in + i));
            __m128i delta = _mm_sub_epi32(current, prev);

            // __m128i shift1 = _mm_slli_epi32(delta, 1);
            // __m128i shiftR = _mm_srai_epi32(delta, 31);
            // __m128i zigzag = _mm_xor_si128(shift1, shiftR);

            // Store directly into compressed_data
            _mm_storeu_si128(reinterpret_cast<__m128i*>(&compressed_data[i]), delta);

            prev = _mm_shuffle_epi32(current, _MM_SHUFFLE(3, 3, 3, 3));
        }

        for (; i < length; ++i) {
            int32_t delta = in[i] - in[i - 1];
            compressed_data[i] = delta;
        }
    }

    void decodeArray(int32_t* out, const size_t length) override {
        if (length == 0) return;

        out[0] = compressed_data[0];

        __m128i prev = _mm_set1_epi32(out[0]);
        size_t i = 1;
        for (; i < length - 4; i += 4) {
            __m128i delta = _mm_loadu_si128(reinterpret_cast<const __m128i*>(&compressed_data[i]));

            // __m128i shiftR = _mm_srai_epi32(zigzag, 1);
            // __m128i mask = _mm_and_si128(zigzag, _mm_set1_epi32(1));
            // __m128i negate = _mm_sub_epi32(_mm_setzero_si128(), mask);
            // __m128i delta = _mm_xor_si128(shiftR, negate);

            __m128i current = _mm_add_epi32(delta, prev);
            _mm_storeu_si128(reinterpret_cast<__m128i*>(out + i), current);

            prev = _mm_shuffle_epi32(current, _MM_SHUFFLE(3, 3, 3, 3));
        }

        for (; i < length; ++i) {
            int32_t delta = compressed_data[i];
            out[i] = delta + out[i - 1];
        }
    }

    std::size_t encodedNumValues() override {
      return compressed_data.size();
    }

    std::size_t encodedSizeValue() override {
      return sizeof(int32_t);
    }

    virtual ~DeltaCodecSSE42NoZigZag() {}

    std::string name() const override {
        return "custom_delta_no_zigzag_vecsse";
    }

    std::size_t getOverflowSize(size_t) const override {
      return 0;
    }

    StatefulIntegerCodec<int32_t>* cloneFresh() const override {
        return new DeltaCodecSSE42NoZigZag();
    }

    void allocEncoded(const int32_t* in, size_t length) override {
        compressed_data.resize(length);
    };

    void clear() override {
        compressed_data.clear();
        // compressed_data.shrink_to_fit();
    }

    std::vector<int32_t>& getEncoded() override {
        return compressed_data;
    };
};

class DeltaCodecSSE42 : public StatefulIntegerCodec<int32_t> {
private:
    std::vector<int32_t> compressed_data;
public:
    void encodeArray(const int32_t* in, const size_t length) override {
        if (length == 0) return;

        compressed_data[0] = (in[0] << 1) ^ (in[0] >> 31);

        __m128i prev = _mm_set1_epi32(in[0]);
        size_t i = 1;
        for (; i < length - 4; i += 4) {
            __m128i current = _mm_loadu_si128(reinterpret_cast<const __m128i*>(in + i));
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

    void decodeArray(int32_t* out, const size_t length) override {
        if (length == 0) return;

        out[0] = (compressed_data[0] >> 1) ^ (-(compressed_data[0] & 1));

        __m128i prev = _mm_set1_epi32(out[0]);
        size_t i = 1;
        for (; i < length - 4; i += 4) {
            __m128i zigzag = _mm_loadu_si128(reinterpret_cast<const __m128i*>(&compressed_data[i]));

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

    std::size_t encodedNumValues() override {
      return compressed_data.size();
    }

    std::size_t encodedSizeValue() override {
      return sizeof(int32_t);
    }

    virtual ~DeltaCodecSSE42() {}

    std::string name() const override {
        return "custom_delta_vecsse";
    }

    std::size_t getOverflowSize(size_t) const override {
      return 0;
    }

    StatefulIntegerCodec<int32_t>* cloneFresh() const override {
        return new DeltaCodecSSE42();
    }

    void allocEncoded(const int32_t* in, size_t length) override {
        compressed_data.resize(length);
    };

    void clear() override {
        compressed_data.clear();
        // compressed_data.shrink_to_fit();
    }

    std::vector<int32_t>& getEncoded() override {
        return compressed_data;
    };
};


// Following Damme et al.
class FORCodecSSE42NoZigZag : public StatefulIntegerCodec<int32_t> {
private:
    std::vector<int32_t> compressed_data;

public:
    FORCodecSSE42NoZigZag() {}

    void encodeArray(const int32_t* in, const size_t length) override {
        if (length == 0) return;

        __m128i minValVec = _mm_set1_epi32(INT_MAX);
        int i = 0;
        for (; i < length - 4; i += 4) {
            __m128i current = _mm_loadu_si128(reinterpret_cast<const __m128i*>(in + i));
            minValVec = _mm_min_epi32(minValVec, current);
        }
        int32_t minBuffer[4];
        _mm_storeu_si128(reinterpret_cast<__m128i*>(minBuffer), minValVec);
        int32_t referenceValue = std::min({minBuffer[0], minBuffer[1], minBuffer[2], minBuffer[3]});
        for (; i < length; ++i) {
            referenceValue = std::min(referenceValue, in[i]);
        }

        compressed_data[0] = referenceValue;

        __m128i refValVec = _mm_set1_epi32(referenceValue);
        i = 0;
        for (; i < length - 4; i += 4) {
            __m128i current = _mm_loadu_si128(reinterpret_cast<const __m128i*>(in + i));
            __m128i diff = _mm_sub_epi32(current, refValVec);

            _mm_storeu_si128(reinterpret_cast<__m128i*>(&compressed_data[i + 1]), diff);
        }
        for (; i < length; ++i) {
            int32_t diff = in[i] - referenceValue;
            compressed_data[i + 1] = diff;
        }
    }

    void decodeArray(int32_t* out, const size_t length) override {
        if (length == 0) return;

        int32_t referenceValue = compressed_data[0];
        __m128i refValVec = _mm_set1_epi32(referenceValue);

        int i = 0;
        for (; i < length - 4; i += 4) {
            __m128i diff = _mm_loadu_si128(reinterpret_cast<const __m128i*>(&compressed_data[i + 1]));

            // Zig-zag decode
            // __m128i shiftRight = _mm_srai_epi32(zigzagEncoded, 1);
            // __m128i negate = _mm_and_si128(zigzagEncoded, _mm_set1_epi32(1));
            // negate = _mm_sub_epi32(_mm_setzero_si128(), negate);
            // __m128i diff = _mm_xor_si128(shiftRight, negate);

            __m128i originalValues = _mm_add_epi32(diff, refValVec);
            _mm_storeu_si128(reinterpret_cast<__m128i*>(out + i), originalValues);
        }

        // Process remaining elements
        for (; i < length; ++i) {
            int32_t encoded = compressed_data[i + 1];
            out[i] = encoded + referenceValue;
        }
    }

    std::size_t encodedNumValues() override {
      return compressed_data.size();
    }

    std::size_t encodedSizeValue() override {
      return sizeof(int32_t);
    }

    virtual ~FORCodecSSE42NoZigZag() {}

    std::string name() const override {
        return "custom_for_no_zigzag_vecsse";
    }

    std::size_t getOverflowSize(size_t) const override {
      return 0;
    }

    StatefulIntegerCodec<int32_t>* cloneFresh() const override {
        return new FORCodecSSE42NoZigZag();
    }

    void allocEncoded(const int32_t* in, size_t length) override {
        compressed_data.resize(length + 1); // +1 for the reference value
    }

    void clear() override {
        compressed_data.clear();
    }

    std::vector<int32_t>& getEncoded() override {
        return compressed_data;
    }
};

// Following Damme et al.
class FORCodecSSE42 : public StatefulIntegerCodec<int32_t> {
private:
    std::vector<int32_t> compressed_data;

public:
    FORCodecSSE42() {}

    void encodeArray(const int32_t* in, const size_t length) override {
        if (length == 0) return;

        __m128i minValVec = _mm_set1_epi32(INT_MAX);
        int i = 0;
        for (; i < length - 4; i += 4) {
            __m128i current = _mm_loadu_si128(reinterpret_cast<const __m128i*>(in + i));
            minValVec = _mm_min_epi32(minValVec, current);
        }
        int32_t minBuffer[4];
        _mm_storeu_si128(reinterpret_cast<__m128i*>(minBuffer), minValVec);
        int32_t referenceValue = std::min({minBuffer[0], minBuffer[1], minBuffer[2], minBuffer[3]});
        for (; i < length; ++i) {
            referenceValue = std::min(referenceValue, in[i]);
        }

        compressed_data[0] = referenceValue;

        __m128i refValVec = _mm_set1_epi32(referenceValue);
        i = 0;
        for (; i < length - 4; i += 4) {
            __m128i current = _mm_loadu_si128(reinterpret_cast<const __m128i*>(in + i));
            __m128i diff = _mm_sub_epi32(current, refValVec);

            __m128i shiftLeft = _mm_slli_epi32(diff, 1);
            __m128i shiftRight = _mm_srai_epi32(diff, 31);
            __m128i zigzagEncoded = _mm_xor_si128(shiftLeft, shiftRight);

            _mm_storeu_si128(reinterpret_cast<__m128i*>(&compressed_data[i + 1]), zigzagEncoded);
        }
        for (; i < length; ++i) {
            int32_t diff = in[i] - referenceValue;
            compressed_data[i + 1] = (diff << 1) ^ (diff >> 31);
        }
    }

    void decodeArray(int32_t* out, const size_t length) override {
        if (length == 0) return;

        int32_t referenceValue = compressed_data[0];
        __m128i refValVec = _mm_set1_epi32(referenceValue);

        int i = 0;
        for (; i < length - 4; i += 4) {
            __m128i zigzagEncoded = _mm_loadu_si128(reinterpret_cast<const __m128i*>(&compressed_data[i + 1]));

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

    std::size_t encodedNumValues() override {
      return compressed_data.size();
    }

    std::size_t encodedSizeValue() override {
      return sizeof(int32_t);
    }

    virtual ~FORCodecSSE42() {}

    std::string name() const override {
        return "custom_for_vecsse";
    }

    std::size_t getOverflowSize(size_t) const override {
      return 0;
    }

    StatefulIntegerCodec<int32_t>* cloneFresh() const override {
        return new FORCodecSSE42();
    }

    void allocEncoded(const int32_t* in, size_t length) override {
        compressed_data.resize(length + 1); // +1 for the reference value
    }

    void clear() override {
        compressed_data.clear();
    }

    std::vector<int32_t>& getEncoded() override {
        return compressed_data;
    }
};



// Following Damme et al.
class RLECodecSSE42 : public StatefulIntegerCodec<int32_t> {
private:
    std::vector<int32_t> compressed_data;

public:
    RLECodecSSE42() {}

    void encodeArray(const int32_t *in, const size_t length) override {
        if (length == 0) return;

        size_t i = 0;
        while (i < length) {
            int32_t currentValue = in[i];
            size_t runLength = 1;

            for (; (i + runLength + 3) < length; runLength += 4) {
                __m128i currentVec = _mm_loadu_si128(reinterpret_cast<const __m128i*>(in + i + runLength - 1));
                __m128i nextVec = _mm_loadu_si128(reinterpret_cast<const __m128i*>(in + i + runLength));
                __m128i cmpResult = _mm_cmpeq_epi32(currentVec, nextVec);

                int mask = _mm_movemask_ps(_mm_castsi128_ps(cmpResult));
                if (mask != 0xF) { // Not all equal
                    // Find the first unequal element in this chunk
                    runLength += __builtin_ctz(~mask); // Count trailing zeros in mask
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

    void decodeArray(int32_t *out, const size_t length) override {
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
    
    std::size_t encodedNumValues() override {
      return compressed_data.size();
    }

    std::size_t encodedSizeValue() override {
      return sizeof(int32_t);
    }

    virtual ~RLECodecSSE42() {}

    std::string name() const override {
        return "custom_rle_vecsse";
    }

    std::size_t getOverflowSize(size_t) const override {
      return 0;
    }

    StatefulIntegerCodec<int32_t>* cloneFresh() const override {
        return new RLECodecSSE42();
    }

    void allocEncoded(const int32_t* in, size_t length) override {
        compressed_data.reserve(2 * (length / 4 + 1));
    };

    void clear() override {
        compressed_data.clear();
        // compressed_data.shrink_to_fit();
    }

    std::vector<int32_t>& getEncoded() override {
        return compressed_data;
    };
};