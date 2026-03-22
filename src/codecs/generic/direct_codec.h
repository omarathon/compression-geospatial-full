#include <cstdint>
#include <cstring>
#include <string>

#include "generic_codecs.h"

#ifdef __clang__
#pragma clang diagnostic ignored "-Wreturn-local-addr"
#endif

/////////////////////////
// direct access codec //
/////////////////////////

class DirectAccessCodec : public StatefulIntegerCodec<int32_t> {
 public:
  std::vector<int32_t> compressed;

  void EncodeArray(const int32_t* in, const size_t length) override {
    std::memcpy(compressed.data(), in, length * sizeof(int32_t));
  }

  void DecodeArray(int32_t* out, const std::size_t length) override {}

  std::size_t EncodedNumValues() override { return compressed.size(); }

  std::size_t EncodedSizeValue() override { return sizeof(int32_t); }

  virtual ~DirectAccessCodec() {}

  std::string name() const override { return "custom_direct_access"; }

  std::size_t GetOverflowSize(size_t) const override { return 0; }

  StatefulIntegerCodec<int32_t>* CloneFresh() const override {
    return new DirectAccessCodec();
  }

  void AllocEncoded(const int32_t* in, size_t length) override {
    compressed.resize(length);
  };

  void clear() override {
    compressed.clear();
    compressed.shrink_to_fit();
  }

  std::vector<int32_t>& GetEncoded() override { return compressed; };
};
