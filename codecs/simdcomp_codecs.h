#include "generic_codecs.h"
#include <cstdint>
#include <string>
#include <iostream>
#include <cassert>
#include "simdcomp.h"

#ifdef __clang__
#pragma clang diagnostic ignored "-Wreturn-local-addr"
#endif

class SimdCompCodec : public StatefulIntegerCodec<int32_t> {
public:

    std::vector<uint8_t> compressed;
    uint32_t b;

  void encodeArray(const int32_t *in, const size_t length) override {
    __m128i * endofbuf = simdpack_length(reinterpret_cast<const uint32_t *>(in), length, (__m128i *)compressed.data(), b);
    int howmanybytes = (endofbuf-(__m128i *)compressed.data())*sizeof(__m128i);
    compressed.resize(howmanybytes);
  }

  void decodeArray(int32_t *out, const std::size_t length) override {
    simdunpack_length((const __m128i *)compressed.data(), length, reinterpret_cast<uint32_t *>(out), b);
  }

  std::size_t encodedNumValues() override {
    return compressed.size();
  }

  std::size_t encodedSizeValue() override {
    return sizeof(uint8_t);
  }

  virtual ~SimdCompCodec() {}

  std::string name() const override {
    return "simdcomp";
  }

  std::size_t getOverflowSize(size_t) const override {
    return 0;
  }

  StatefulIntegerCodec<int32_t>* cloneFresh() const override {
    return new SimdCompCodec();
  }

  void allocEncoded(const int32_t* in, size_t length) override {
    b = maxbits_length(reinterpret_cast<const uint32_t*>(in), length);
    compressed.resize(simdpack_compressedbytes(length, b));
  };

  void clear() override {
      compressed.clear();
      compressed.shrink_to_fit();
  }

  std::vector<int32_t>& getEncoded() override {
      throw std::runtime_error("Encoded format does not match input. Cannot forward.");
      std::vector<int32_t> dummy{};
      return dummy;
  };
};

class SimdCompScratchCodec : public StatefulIntegerCodec<int32_t> {
public:
    std::vector<uint8_t> compressed;
    uint32_t b;

    static inline std::vector<uint8_t> compressScratch = 
        std::vector<uint8_t>(TILE_WIDTH * TILE_HEIGHT * 3);

    void encodeArray(const int32_t *in, const size_t length) override {
        // compute bit width
        b = maxbits_length(reinterpret_cast<const uint32_t*>(in), length);

        // compute how many bytes the compressed data will take
        size_t neededBytes = simdpack_compressedbytes(length, b);
        if (neededBytes > compressScratch.size()) {
            throw std::runtime_error("compressScratch too small for this block");
        }

        // encode into scratch buffer
        __m128i* endofbuf = simdpack_length(
            reinterpret_cast<const uint32_t*>(in),
            length,
            reinterpret_cast<__m128i*>(compressScratch.data()),
            b);

        // verify size
        size_t howmanybytes = 
            (reinterpret_cast<uint8_t*>(endofbuf) - compressScratch.data());

        // copy into compressed (single resize + memcpy)
        compressed.assign(compressScratch.data(),
                  compressScratch.data() + neededBytes);
    }

    void decodeArray(int32_t *out, const std::size_t length) override {
        simdunpack_length(
            reinterpret_cast<const __m128i*>(compressed.data()),
            length,
            reinterpret_cast<uint32_t*>(out),
            b);
    }

    std::size_t encodedNumValues() override {
        return compressed.size();
    }

    std::size_t encodedSizeValue() override {
        return sizeof(uint8_t);
    }

    std::string name() const override {
        return "simdcomp_scratch";
    }

    std::size_t getOverflowSize(size_t) const override {
        return 0;
    }

    StatefulIntegerCodec<int32_t>* cloneFresh() const override {
        return new SimdCompScratchCodec();
    }

    void allocEncoded(const int32_t* in, size_t length) override {
        // nothing to allocate up front — handled in encodeArray
        (void)in;
        (void)length;
    }

    void clear() override {
        compressed.clear();
        compressed.shrink_to_fit();
    }

    std::vector<int32_t>& getEncoded() override {
        throw std::runtime_error("Encoded format does not match input. Cannot forward.");
    }
};

class SimdCompDeltaPadCodec : public StatefulIntegerCodec<int32_t> {
public:
    std::vector<uint8_t> compressed;

    void encodeArray(const int32_t *in, const size_t length) override {
        uint8_t* out = compressed.data();   // byte pointer
        uint32_t offset = 0;
        size_t fullBlocks = length / SIMDBlockSize;
        size_t remainder  = length % SIMDBlockSize;

        // full blocks
        for (size_t i = 0; i < fullBlocks; ++i) {
            const uint32_t* block = reinterpret_cast<const uint32_t*>(in) + i * SIMDBlockSize;
            uint32_t b = simdmaxbitsd1(offset, block);

            *out++ = static_cast<uint8_t>(b);
            simdpackwithoutmaskd1(offset, block, reinterpret_cast<__m128i*>(out), b);
            offset = block[SIMDBlockSize - 1];
            out += b * sizeof(__m128i);   // advance by packed size
        }

        // remainder padded
        if (remainder) {
            uint32_t tmp[SIMDBlockSize];
            const uint32_t* tail = reinterpret_cast<const uint32_t*>(in) + fullBlocks * SIMDBlockSize;
            for (size_t j = 0; j < remainder; ++j) tmp[j] = tail[j];
            for (size_t j = remainder; j < SIMDBlockSize; ++j) tmp[j] = tmp[remainder - 1];

            uint32_t b = simdmaxbitsd1(offset, tmp);

            *out++ = static_cast<uint8_t>(b);
            simdpackwithoutmaskd1(offset, tmp, reinterpret_cast<__m128i*>(out), b);
            offset = tmp[SIMDBlockSize - 1];
            out += b * sizeof(__m128i);
        }

        compressed.resize(out - compressed.data());  // real size in bytes
    }

    void decodeArray(int32_t *out, const std::size_t length) override {
        const uint8_t* in = compressed.data();
        uint32_t offset = 0;
        size_t blocks = (length + SIMDBlockSize - 1) / SIMDBlockSize;

        for(size_t i = 0; i < blocks; ++i) {
            uint32_t b = *in++;
            simdunpackd1(offset, reinterpret_cast<const __m128i*>(in),
                         reinterpret_cast<uint32_t*>(out) + i * SIMDBlockSize,
                         b);
            offset = reinterpret_cast<uint32_t*>(out)[(i+1)*SIMDBlockSize - 1];
            in += b * sizeof(__m128i);
        }
    }

    std::size_t encodedNumValues() override {
        return compressed.size();
    }

    std::size_t encodedSizeValue() override {
        return sizeof(uint8_t);
    }

    std::string name() const override {
        return "simdcomp_delta_pad";
    }

    StatefulIntegerCodec<int32_t>* cloneFresh() const override {
        return new SimdCompDeltaPadCodec();
    }

    void allocEncoded(const int32_t*, size_t length) override {
        size_t blocks = (length + SIMDBlockSize - 1) / SIMDBlockSize;
        compressed.resize(blocks * (1 + 32 * sizeof(__m128i)));
    }

    void clear() override {
        compressed.clear();
        compressed.shrink_to_fit();
    }

    std::vector<int32_t>& getEncoded() override {
        throw std::runtime_error("Encoded format does not match input. Cannot forward.");
    }

    std::size_t getOverflowSize(size_t length) const override {
        size_t remainder = length % SIMDBlockSize;
        return remainder ? (SIMDBlockSize - remainder) : 0;
    }
};


class SimdCompDeltaTailCodec : public StatefulIntegerCodec<int32_t> {
public:
    std::vector<uint8_t> compressed;
    uint32_t tailBits = 0; // bit width for the remainder

    void encodeArray(const int32_t *in, const size_t length) override {
        uint8_t* out = compressed.data();   // byte pointer
        uint32_t offset = 0;
        size_t fullBlocks = length / SIMDBlockSize;
        size_t remainder  = length % SIMDBlockSize;

        // delta-coded full blocks
        for (size_t i = 0; i < fullBlocks; ++i) {
            const uint32_t* block = reinterpret_cast<const uint32_t*>(in) + i * SIMDBlockSize;
            uint32_t b = simdmaxbitsd1(offset, block);

            *out++ = static_cast<uint8_t>(b);
            simdpackwithoutmaskd1(offset, block, reinterpret_cast<__m128i*>(out), b);
            offset = block[SIMDBlockSize - 1];
            out += b * sizeof(__m128i);
        }

        // plain simdcomp for tail
        if (remainder) {
            const uint32_t* tail = reinterpret_cast<const uint32_t*>(in) + fullBlocks * SIMDBlockSize;
            tailBits = maxbits_length(tail, remainder);
            __m128i* end = simdpack_length(tail, remainder,
                                          reinterpret_cast<__m128i*>(out),
                                          tailBits);
            out = reinterpret_cast<uint8_t*>(end);
        }

        compressed.resize(out - compressed.data());  // real size in bytes
    }

    void decodeArray(int32_t *out, const std::size_t length) override {
        const uint8_t* in = compressed.data();
        uint32_t offset = 0;
        size_t fullBlocks = length / SIMDBlockSize;
        size_t remainder  = length % SIMDBlockSize;

        // delta-coded full blocks
        for(size_t i = 0; i < fullBlocks; ++i) {
            uint32_t b = *in++;
            simdunpackd1(offset, reinterpret_cast<const __m128i*>(in),
                         reinterpret_cast<uint32_t*>(out) + i * SIMDBlockSize,
                         b);
            offset = reinterpret_cast<uint32_t*>(out)[(i+1)*SIMDBlockSize - 1];
            in += b * sizeof(__m128i);
        }

        // plain unpack for tail
        if(remainder) {
            simdunpack_length(reinterpret_cast<const __m128i*>(in),
                              remainder,
                              reinterpret_cast<uint32_t*>(out) + fullBlocks * SIMDBlockSize,
                              tailBits);
        }
    }

    std::size_t encodedNumValues() override {
        return compressed.size();
    }

    std::size_t encodedSizeValue() override {
        return sizeof(uint8_t);
    }

    std::string name() const override {
        return "simdcomp_delta_tail";
    }

    StatefulIntegerCodec<int32_t>* cloneFresh() const override {
        return new SimdCompDeltaTailCodec();
    }

    void allocEncoded(const int32_t* in, size_t length) override {
        size_t fullBlocks = length / SIMDBlockSize;
        size_t remainder  = length % SIMDBlockSize;

        size_t size = fullBlocks * (1 + 32 * sizeof(__m128i));
        if (remainder) {
            uint32_t b = maxbits_length(reinterpret_cast<const uint32_t*>(in) + fullBlocks * SIMDBlockSize,
                                        remainder);
            size += simdpack_compressedbytes(remainder, b);
        }
        compressed.resize(size);
    }

    void clear() override {
        compressed.clear();
        compressed.shrink_to_fit();
        tailBits = 0;
    }

    std::vector<int32_t>& getEncoded() override {
        throw std::runtime_error("Encoded format does not match input. Cannot forward.");
    }

    std::size_t getOverflowSize(size_t) const override {
        // simdunpack_length writes exactly `remainder` values
        return 0;
    }
};


#include "../arena.h"


class ArenaSimdCompCodec : public StatefulIntegerCodec<int32_t> {
    Arena* arena;        // not owned
    size_t offset = 0;   // where this block's data starts in arena
    size_t size_used = 0;
    uint32_t b = 0;

public:
    ArenaSimdCompCodec(Arena* a) : arena(a) {}

    void allocEncoded(const int32_t* in, size_t length) override {
        b = maxbits_length(reinterpret_cast<const uint32_t*>(in), length);
        size_t needed = simdpack_compressedbytes(length, b);
        arena->alloc(needed, offset);
        size_used = needed;
    }

    void encodeArray(const int32_t* in, const size_t length) override {
        uint8_t* buf = arena->buffer.data() + offset;
        __m128i* endofbuf = simdpack_length(
            reinterpret_cast<const uint32_t*>(in),
            length,
            reinterpret_cast<__m128i*>(buf),
            b
        );
        size_used = (endofbuf - reinterpret_cast<__m128i*>(buf)) * sizeof(__m128i);
    }

    void decodeArray(int32_t* out, const size_t length) override {
        const uint8_t* buf = arena->buffer.data() + offset;
        simdunpack_length(
            reinterpret_cast<const __m128i*>(buf),
            length,
            reinterpret_cast<uint32_t*>(out),
            b
        );
    }

    std::size_t encodedNumValues() override { return size_used; }
    std::size_t encodedSizeValue() override { return sizeof(uint8_t); }

    StatefulIntegerCodec<int32_t>* cloneFresh() const override {
        throw std::runtime_error("ArenaSimdCompCodec must be constructed with an arena");
    }

    void clear() override { size_used = 0; }
    std::vector<int32_t>& getEncoded() override {
        throw std::runtime_error("ArenaSimdCompCodec does not expose raw vector");
    }

    std::string name() const override { return "arena_simdcomp"; }
    std::size_t getOverflowSize(size_t) const override { return 0; }
};