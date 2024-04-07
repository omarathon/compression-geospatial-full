#include "generic_codecs.h"
#include <cstdint>
#include <string>
#include <iostream>
#include <cstring>
#include <memory>

#include "ic.h"

#define CBUF4(_n_) (((size_t)(_n_))*5/3+1024*1024)
#define CBUF1(_n_) (((size_t)(_n_))*sizeof(int32_t)*5/3+1024*1024)

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

class TurboPForCodec : public StatefulIntegerCodec<int32_t> {
private:
    std::vector<uint8_t> compressed;
    std::vector<uint8_t> tmp;
    std::vector<uint32_t> tmp32;
    const size_t method;

public:

  TurboPForCodec(const size_t method) : method{method} {}

  void encodeArray(const int32_t *in, const size_t length) override {
    int32_t *in_nconst = const_cast<int32_t *>(in); // Necessary as TurboPFor is a C library
    uint32_t *in_tpf = reinterpret_cast<uint32_t *>(in_nconst);

    size_t compsize;
    switch (method) {
        case 1:
            compsize = p4nenc32(in_tpf, length, compressed.data());
            break;
        case 2:
            compsize = p4nenc128v32(in_tpf, length, compressed.data());
            break;
        case 3:
            compsize = p4nenc256v32(in_tpf, length, compressed.data());
            break;
        case 4:
            compsize = p4ndenc256v32(in_tpf, length, compressed.data());
            break;
        case 5:
            compsize = p4nd1enc256v32(in_tpf, length, compressed.data());
            break;
        case 6:
            compsize = p4nzenc256v32(in_tpf, length, compressed.data());
            break;
        case 7:
            compsize = bitnpack256v32(in_tpf, length, compressed.data());
            break;
        case 8:
            compsize = bitndpack256v32(in_tpf, length, compressed.data());
            break;
        case 9:
            compsize = bitnd1pack256v32(in_tpf, length, compressed.data());
            break;
        case 10:
            compsize = bitnzpack256v32(in_tpf, length, compressed.data());
            break;
        // case 11:
        //     compsize = bitnfpack256v32(in_tpf, length, compressed.data());
        //     break;
        case 12:
            compsize = bitnxpack256v32(in_tpf, length, compressed.data());
            break;
        case 13:
            compsize = p4nzzenc128v32(in_tpf, length, compressed.data(), /* start */ 0);
            break;
        case 14: {
            uint8_t *compend = vsenc32(in_tpf, length, compressed.data());
            if (compend < compressed.data() || compend >= (compressed.data() + compressed.size())) {
                throw std::runtime_error("vsenc32 failed.");
                return;
            }
            compsize = compend - compressed.data();
            break;
        }
        case 15: {
            uint8_t *compend2 = vszenc32(in_tpf, length, compressed.data(), tmp32.data());
            if (compend2 < compressed.data() || compend2 >= compressed.data() + compressed.size()) {
                throw std::runtime_error("vszenc32 failed.");
                return;
            }
            compsize = compend2 - compressed.data();
            tmp32.clear();
            tmp32.shrink_to_fit();
            break;
        }
        case 16:
            compsize = bvzzenc32(in_tpf, length, compressed.data(), /* start */ 0);
            break;
        case 17:
            compsize = bvzenc32(in_tpf, length, compressed.data(), /* start */ 0);
            break;
        case 18:
            compsize = trlec(reinterpret_cast<const uint8_t *>(in), length * 4, compressed.data());
            break;
        case 19:
            compsize = trlexc(reinterpret_cast<uint8_t *>(in_nconst), length * 4, compressed.data(), tmp.data());
            tmp.clear();
            tmp.shrink_to_fit();
            break;
        case 20:
            compsize = trlezc(reinterpret_cast<uint8_t *>(in_nconst), length * 4, compressed.data(), tmp.data());
            tmp.clear();
            tmp.shrink_to_fit();
            break;
        // case 21:
        //     compsize = srlec32(reinterpret_cast<const uint8_t *>(in), length, compressed.data(), RLE32);
        //     break;
        // case 22:
        //     compsize = srlezc32(in_tpf, length, compressed.data(), tmp.data(), RLE32);
        //     tmp.clear();
        //     tmp.shrink_to_fit();
        //     break;

        default:
            throw std::runtime_error("Unknown TurboPFor method used.");
            return;
    }
    compressed.resize(compsize);
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
        // case 11:
        //     bitnfunpack256v32(compressed.data(), length, out_tpf);
        //     break;
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
        // case 21:
        //     srled32(compressed.data(), compressed.size(), out_tpf, length, RLE32);
        //     break;
        // case 22:
        //     srlezd32(compressed.data(), compressed.size(), out_tpf, length, RLE32);
        //     break;
        
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
        // TODO: FIX
        // case 11:
        //     return "TurboPFor_FOR+TurboPack256";
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
        // TODO: Fix below.
        // case 21:
        //     return "TurboPFor_ESC+TurboRLE"; // Req RLE32
        // case 22:
        //     return "TurboPFor_ESC+zigzag+TurboRLE"; // Req tmp, RLE32


        default:
            throw std::runtime_error("Unknown TurboPFor method used.");
            return "ERROR";
    }
  }

  std::size_t getOverflowSize(size_t length) const override {
    return CBUF4(length) - length;
  }

  StatefulIntegerCodec<int32_t>* cloneFresh() const override {
    return new TurboPForCodec(method);
  }

  
  void allocEncoded(const int32_t* in, size_t length) override {
    compressed.resize(CBUF1(length));
    if (method == 15) {
        tmp32.resize(CBUF4(length));
    }
    else if (method == 19 || method == 20) {
        tmp.resize(CBUF1(length));
    }
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