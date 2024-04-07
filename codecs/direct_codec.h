#include "generic_codecs.h"
#include <cstdint>
#include <string>
#include <cstring>

#ifdef __clang__
#pragma clang diagnostic ignored "-Wreturn-local-addr"
#endif

/////////////////////////
// direct access codec //
/////////////////////////

class DirectAccessCodec : public StatefulIntegerCodec<int32_t> {
public:

  std::vector<int32_t> compressed;

  void encodeArray(const int32_t *in, const size_t length) override {
    std::memcpy(compressed.data(), in, length * sizeof(int32_t));
  }

  void decodeArray(int32_t *out, const std::size_t length) override {
  }

  std::size_t encodedNumValues() override {
    return compressed.size();
  }

  std::size_t encodedSizeValue() override {
    return sizeof(int32_t);
  }

  virtual ~DirectAccessCodec() {}

  std::string name() const override {
    return "custom_direct_access";
  }

  std::size_t getOverflowSize(size_t) const override {
    return 0;
  }

  StatefulIntegerCodec<int32_t>* cloneFresh() const override {
    return new DirectAccessCodec();
  }

  void allocEncoded(const int32_t* in, size_t length) override {
    compressed.resize(length);
  };

  void clear() override {
      compressed.clear();
      compressed.shrink_to_fit();
  }

  std::vector<int32_t>& getEncoded() override {
      return compressed;
  };
};
