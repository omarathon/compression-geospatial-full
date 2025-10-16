#include "generic_codecs.h"
#include <cstdint>
#include <string>
#include <iostream>
#include <cstring>
#include <memory>

#include "ic.h"

#define CBUFN(_n_) (((size_t)(_n_))*5/3+1024*1024)
#define CBUF4(_n_) (((size_t)(_n_))*sizeof(int32_t)*5/3+1024*1024)

#define CBUF2(_n_) (((size_t)(_n_))*sizeof(int16_t)*5/3+1024*1024)

//------------------ TurboRLE (Run Length Encoding) + zigzag/xor -------------------------
#define RLE8  0xdau
#define RLE16 0xdadau
#define RLE32 0xdadadadau
#define RLE64 0xdadadadadadadadaull
unsigned trlezc( uint8_t      *in, unsigned n, unsigned char *out, unsigned char *tmp) { bitzenc8(in, n, tmp, 0, 0); unsigned rc = trlec(tmp, n, out); if(rc >=n) { rc = n; memcpy(out,in,n); } return rc; }
unsigned trlezd(unsigned char *in, unsigned inlen, uint8_t *out, unsigned n) { if(inlen >= n) { memcpy(out,in,n); return inlen; } trled(in, inlen, out, n); bitzdec8(out, n, 0); return n; }
unsigned trlexc( uint8_t      *in, unsigned n, unsigned char *out, unsigned char *tmp) { bitxenc8(in, n, tmp, 0); unsigned rc = trlec(tmp, n, out); if(rc >=n) { rc = n; memcpy(out,in,n); } return rc; }
unsigned trlexd(unsigned char *in, unsigned inlen, uint8_t *out, unsigned n) { if(inlen >= n) { memcpy(out,in,n); return inlen; } trled(in, inlen, out, n); bitxdec8(out, n, 0); return n; }

//------------------ TurboVSsimple zigzag------------------------------------------
unsigned char *vszenc32(uint32_t *in, unsigned n, unsigned char *out, uint32_t *tmp) { bitzenc32(in, n, tmp, 0, 0); return vsenc32(tmp, n, out); }
unsigned char *vszdec32(unsigned char *in, unsigned n, uint32_t *out) { unsigned char *p = vsdec32(in,n,out); bitzdec32(out, n, 0); return p; }

template <typename T> // T: type of data being stored
class TurboPForCodec : public StatefulIntegerCodec<T> {};

/* Specializations */

template <>
class TurboPForCodec<int32_t> : public StatefulIntegerCodec<int32_t> {
protected:
    std::vector<uint8_t> compressed;

    static inline std::vector<uint8_t> compressScratch = 
        std::vector<uint8_t>(CBUF4(TILE_WIDTH * TILE_HEIGHT));

    static inline std::vector<uint8_t> tmp = std::vector<uint8_t>(CBUF4(TILE_WIDTH * TILE_HEIGHT));
    static inline std::vector<uint32_t> tmp32 = std::vector<uint32_t>(CBUFN(TILE_WIDTH * TILE_HEIGHT));
    
    const size_t method;

public:

  TurboPForCodec(const size_t method) : method{method} {}

  void encodeArray(const int32_t *in, const size_t length) override {
    int32_t *in_nconst = const_cast<int32_t *>(in); // Necessary as TurboPFor is a C library
    uint32_t *in_tpf = reinterpret_cast<uint32_t *>(in_nconst);

    size_t compsize;
    switch (method) {
        case 1:
            compsize = p4nenc32(in_tpf, length, compressScratch.data());
            break;
        case 2:
            compsize = p4nenc128v32(in_tpf, length, compressScratch.data());
            break;
        case 3:
            compsize = p4nenc256v32(in_tpf, length, compressScratch.data());
            break;
        case 4:
            compsize = p4ndenc256v32(in_tpf, length, compressScratch.data());
            break;
        case 5:
            compsize = p4nd1enc256v32(in_tpf, length, compressScratch.data());
            break;
        case 6:
            compsize = p4nzenc256v32(in_tpf, length, compressScratch.data());
            break;
        case 7:
            compsize = bitnpack256v32(in_tpf, length, compressScratch.data());
            break;
        case 8:
            compsize = bitndpack256v32(in_tpf, length, compressScratch.data());
            break;
        case 9:
            compsize = bitnd1pack256v32(in_tpf, length, compressScratch.data());
            break;
        case 10:
            compsize = bitnzpack256v32(in_tpf, length, compressScratch.data());
            break;
        case 12:
            compsize = bitnxpack256v32(in_tpf, length, compressScratch.data());
            break;
        case 13:
            compsize = p4nzzenc128v32(in_tpf, length, compressScratch.data(), /* start */ 0);
            break;
        case 14: {
            uint8_t *compend = vsenc32(in_tpf, length, compressScratch.data());
            if (compend < compressScratch.data() || compend >= (compressScratch.data() + compressScratch.size())) {
                throw std::runtime_error("vsenc32 failed.");
                return;
            }
            compsize = compend - compressScratch.data();
            break;
        }
        case 15: {
            uint8_t *compend2 = vszenc32(in_tpf, length, compressScratch.data(), tmp32.data());
            if (compend2 < compressScratch.data() || compend2 >= compressScratch.data() + compressScratch.size()) {
                throw std::runtime_error("vszenc32 failed.");
                return;
            }
            compsize = compend2 - compressScratch.data();
            break;
        }
        case 16:
            compsize = bvzzenc32(in_tpf, length, compressScratch.data(), /* start */ 0);
            break;
        case 17:
            compsize = bvzenc32(in_tpf, length, compressScratch.data(), /* start */ 0);
            break;
        case 18:
            compsize = trlec(reinterpret_cast<const uint8_t *>(in), length * 4, compressScratch.data());
            break;
        case 19:
            compsize = trlexc(reinterpret_cast<uint8_t *>(in_nconst), length * 4, compressScratch.data(), tmp.data());
            break;
        case 20:
            compsize = trlezc(reinterpret_cast<uint8_t *>(in_nconst), length * 4, compressScratch.data(), tmp.data());
            break;

        default:
            throw std::runtime_error("Unknown TurboPFor method used.");
            return;
    }
    compressed.assign(compressScratch.data(), compressScratch.data() + compsize);
  }

  void decodeArray(int32_t *out, const std::size_t length) override {
    uint32_t *out_tpf = reinterpret_cast<uint32_t *>(out);
    switch (method) {
        case 1:
            p4ndec32(compressed.data(), length, out_tpf);
            break;
        case 2:
            p4ndec128v32(compressed.data(), length, out_tpf);
            break;
        case 3:
            p4ndec256v32(compressed.data(), length, out_tpf);
            break;
        case 4:
            p4nddec256v32(compressed.data(), length, out_tpf);
            break;
        case 5:
            p4nd1dec256v32(compressed.data(), length, out_tpf);
            break;
        case 6:
            p4nzdec256v32(compressed.data(), length, out_tpf);
            break;
        case 7:
            bitnunpack256v32(compressed.data(), length, out_tpf);
            break;
        case 8:
            bitndunpack256v32(compressed.data(), length, out_tpf);
            break;
        case 9:
            bitnd1unpack256v32(compressed.data(), length, out_tpf);
            break;
        case 10:
            bitnzunpack256v32(compressed.data(), length, out_tpf);
            break;
        case 12:
            bitnxunpack256v32(compressed.data(), length, out_tpf);
            break;
        case 13:
            p4nzzdec128v32(compressed.data(), length, out_tpf, /* start */ 0);
            break;
        case 14:
            vsdec32(compressed.data(), length, out_tpf);
            break;
        case 15:
            vszdec32(compressed.data(), length, out_tpf);
            break;
        case 16:
            bvzzdec32(compressed.data(), length, out_tpf, /* start */ 0);
            break;
        case 17:
            bvzdec32(compressed.data(), length, out_tpf, /* start */ 0);
            break;
        case 18:
            trled(compressed.data(), compressed.size(), reinterpret_cast<uint8_t *>(out_tpf), length * 4);
            break;
        case 19:
            trlexd(compressed.data(), compressed.size(), reinterpret_cast<uint8_t *>(out_tpf), length * 4);
            break;
        case 20:
            trlezd(compressed.data(), compressed.size(), reinterpret_cast<uint8_t *>(out_tpf), length * 4);
            break;
        
        default:
            throw std::runtime_error("Unknown TurboPFor method used.");
            return;
    }
  }

  std::size_t encodedNumValues() override {
    return compressed.size();
  }

  std::size_t encodedSizeValue() override {
    return sizeof(unsigned char);
  }

  virtual ~TurboPForCodec() {}

  std::string name() const override {
    switch (method) {
        case 1:
            return "TurboPFor_TurboPFor";
        case 2:
            return "TurboPFor_TurboPFor128";
        case 3:
            return "TurboPFor_TurboPFor256";
        case 4:
            return "TurboPFor_Delta+TurboPFor256";
        case 5:
            return "TurboPFor_Delta1+TurboPFor256";
        case 6:
            return "TurboPFor_Zigzag+TurboPFor256";
        case 7:
            return "TurboPFor_TurboPack256";
        case 8:
            return "TurboPFor_Delta+TurboPack256";
        case 9:
            return "TurboPFor_Delta1+TurboPack256";
        case 10:
            return "TurboPFor_Zigzag+TurboPack256";
        case 12:
            return "TurboPFor_xor+TurboPack256";
        case 13:
            return "TurboPFor_zzag/delta+TurboPFor128";
        case 14:
            return "TurboPFor_TurboVSimple";
        case 15:
            return "TurboPFor_Zigzag+TurboVSimple"; // Req tmp
        case 16:
            return "TurboPFor_Zigzag/delta_bitio";
        case 17:
            return "TurboPFor_Zigzag_bitio";
        case 18:
            return "TurboPFor_TurboRLE";
        case 19:
            return "TurboPFor_Xor+TurboRLE"; // Req tmp
        case 20:
            return "TurboPFor_Zigzag+TurboRLE"; // Req tmp

        default:
            throw std::runtime_error("Unknown TurboPFor method used.");
            return "ERROR";
    }
  }

  std::size_t getOverflowSize(size_t length) const override {
    return CBUFN(length) - length;
  }

  StatefulIntegerCodec<int32_t>* cloneFresh() const override {
    return new TurboPForCodec(method);
  }

  
  void allocEncoded(const int32_t* in, size_t length) override {
    (void)in; (void)length;
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


template <>
class TurboPForCodec<int16_t> : public StatefulIntegerCodec<int16_t> {
protected:
    std::vector<uint8_t> compressed;

    static inline std::vector<uint8_t> compressScratch = 
        std::vector<uint8_t>(CBUF2(TILE_WIDTH * TILE_HEIGHT));

    static inline std::vector<uint8_t> tmp = std::vector<uint8_t>(CBUF2(TILE_WIDTH * TILE_HEIGHT));
    static inline std::vector<uint16_t> tmp32 = std::vector<uint16_t>(CBUFN(TILE_WIDTH * TILE_HEIGHT));
    
    const size_t method;

public:

  TurboPForCodec(const size_t method) : method{method} {}

  void encodeArray(const int16_t *in, const size_t length) override {
    int16_t *in_nconst = const_cast<int16_t *>(in); // Necessary as TurboPFor is a C library
    uint16_t *in_tpf = reinterpret_cast<uint16_t *>(in_nconst);

    size_t compsize;
    switch (method) {
        case 10:
            compsize = bitnzpack128v16(in_tpf, length, compressScratch.data());
            break;
        case 12:
            compsize = bitnxpack128v16(in_tpf, length, compressScratch.data());
            break;
        default:
            throw std::runtime_error("Unknown TurboPFor method used.");
            return;
    }
    compressed.assign(compressScratch.data(), compressScratch.data() + compsize);
  }

  void decodeArray(int16_t *out, const std::size_t length) override {
    uint16_t *out_tpf = reinterpret_cast<uint16_t *>(out);
    switch (method) {
        case 10:
            bitnzunpack128v16(compressed.data(), length, out_tpf); // for some reason this doesnt support 256 bit register with 16 bits... no idea why. maybe use low-level funcitons to achieve it. just add a function here https://github.com/powturbo/TurboPFor-Integer-Compression/blob/06d6aad98b4be5471289f35d5d04fac4469cf6df/lib/bitunpack.c#L1394
            break;
        case 12:
            bitnxunpack128v16(compressed.data(), length, out_tpf);
            break;
        
        default:
            throw std::runtime_error("Unknown TurboPFor method used.");
            return;
    }
  }

  std::size_t encodedNumValues() override {
    return compressed.size();
  }

  std::size_t encodedSizeValue() override {
    return sizeof(unsigned char);
  }

  virtual ~TurboPForCodec() {}

  std::string name() const override {
    switch (method) {
        case 10:
            return "TurboPFor_Zigzag+TurboPack256";
        case 12:
            return "TurboPFor_xor+TurboPack256";

        default:
            throw std::runtime_error("Unknown TurboPFor method used.");
            return "ERROR";
    }
  }

  std::size_t getOverflowSize(size_t length) const override {
    return CBUFN(length) - length;
  }

  StatefulIntegerCodec<int16_t>* cloneFresh() const override {
    return new TurboPForCodec(method);
  }

  
  void allocEncoded(const int16_t* in, size_t length) override {
    (void)in; (void)length;
  };

  void clear() override {
      compressed.clear();
      compressed.shrink_to_fit();
  }

  std::vector<int16_t>& getEncoded() override {
      throw std::runtime_error("Encoded format does not match input. Cannot forward.");
      std::vector<int16_t> dummy{};
      return dummy;
  };
};