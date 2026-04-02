#include <cstdint>
#include <cstring>
#include <iostream>
#include <memory>
#include <string>

#include "generic_codecs.h"
#include "ic.h"

#define CBUF2(_n_) (((size_t)(_n_)) * 5 / 3 + 1024 * 1024)
#define CBUF1(_n_) (((size_t)(_n_)) * sizeof(uint16_t) * 5 / 3 + 1024 * 1024)

class TurboPForCodecU16 : public StatefulIntegerCodec<uint16_t> {
 private:
  std::vector<uint8_t> compressed;
  std::vector<uint8_t> tmp;
  std::vector<uint16_t> tmp16;
  const size_t method;

 public:
  TurboPForCodecU16(const size_t method) : method{method} {}

  void EncodeArray(const uint16_t *in, const size_t length) override {
    uint16_t *in_nconst =
        const_cast<uint16_t *>(in);  // Necessary as TurboPFor is a C library
    uint16_t *in_tpf = reinterpret_cast<uint16_t *>(in_nconst);

    size_t compsize;
    switch (method) {
      case 3:
        compsize = p4nenc128v16(in_tpf, length, compressed.data());
        break;
      case 7:
        compsize = bitnpack128v16(in_tpf, length, compressed.data());
        break;
      default:
        throw std::runtime_error("Unknown TurboPFor method used.");
        return;
    }
    compressed.resize(compsize);
  }

  void DecodeArray(uint16_t *out, const std::size_t length) override {
    uint16_t *out_tpf = reinterpret_cast<uint16_t *>(out);
    switch (method) {
      case 3:
        p4ndec128v16(compressed.data(), length, out_tpf);
        break;
      case 7:
        bitnunpack128v16(compressed.data(), length, out_tpf);
        break;

      default:
        throw std::runtime_error("Unknown TurboPFor method used.");
        return;
    }
  }

  std::size_t EncodedNumValues() override { return compressed.size(); }

  std::size_t EncodedSizeValue() override { return sizeof(unsigned char); }

  virtual ~TurboPForCodecU16() {}

  std::string name() const override {
    switch (method) {
      case 3:
        return "TurboPFor_TurboPFor128";
      case 7:
        return "TurboPFor_TurboPack128";
      default:
        throw std::runtime_error("Unknown TurboPFor method used.");
        return "ERROR";
    }
  }

  std::size_t GetOverflowSize(size_t length) const override {
    return CBUF2(length) - length;
  }

  StatefulIntegerCodec<uint16_t> *CloneFresh() const override {
    return new TurboPForCodecU16(method);
  }

  void AllocEncoded(const uint16_t *in, size_t length) override {
    compressed.resize(CBUF1(length));
    if (method == 15) {
      tmp16.resize(CBUF2(length));
    } else if (method == 19 || method == 20) {
      tmp.resize(CBUF1(length));
    }
  };

  void clear() override {
    compressed.clear();
    compressed.shrink_to_fit();
  }

  std::vector<uint16_t> &GetEncoded() override {
    throw std::runtime_error(
        "Encoded format does not match input. Cannot forward.");
  };
};