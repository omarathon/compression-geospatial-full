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

template <typename T> // T: type of data being compressed
class DirectAccessCodec : public StatefulIntegerCodec<T> {
public:

  std::vector<T> compressed;

  void encodeArray(const T *in, const size_t length) override {
    compressed.assign(in, in + length);
    // std::memcpy(compressed.data(), in, length * sizeof(T));
  }

  void decodeArray(T *out, const std::size_t length) override {
  }

  std::size_t encodedNumValues() override {
    return compressed.size();
  }

  std::size_t encodedSizeValue() override {
    return sizeof(T);
  }

  virtual ~DirectAccessCodec() {}

  std::string name() const override {
    return "custom_direct_access";
  }

  std::size_t getOverflowSize(size_t) const override {
    return 0;
  }

  StatefulIntegerCodec<T>* cloneFresh() const override {
    return new DirectAccessCodec<T>();
  }

  void allocEncoded(const T* in, size_t length) override {
    // compressed.resize(length);
  };

  void clear() override {
      compressed.clear();
      compressed.shrink_to_fit();
  }

  std::vector<T>& getEncoded() override {
      return compressed;
  };
};

template <typename T> // T: type of data being compressed
class PointerDirectAccessCodec : public StatefulIntegerCodec<T> {
public:
    std::shared_ptr<T[]> data;
    size_t length = 0;

    void encodeArray(const T* in, const size_t len) override {
        throw std::runtime_error("PointerDirectAccessCodec has no std::vector");
    }

    void decodeArray(T* out, const std::size_t len) override {
        // No-op: we already point to the original data
        (void)out;
        (void)len;
    }

    std::size_t encodedNumValues() override {
        return length;
    }

    std::size_t encodedSizeValue() override {
        return sizeof(T);
    }

    ~PointerDirectAccessCodec() override = default;

    std::string name() const override {
        return "custom_pointer_direct_access";
    }

    std::size_t getOverflowSize(size_t) const override {
        return 0;
    }

    StatefulIntegerCodec<T>* cloneFresh() const override {
        return new PointerDirectAccessCodec<T>();
    }

    void allocEncoded(const T* in, size_t len) override {
        throw std::runtime_error("PointerDirectAccessCodec has no std::vector");
    }

    void clear() override {
        data.reset();
        length = 0;
    }

    std::vector<T>& getEncoded() override {
        throw std::runtime_error("PointerDirectAccessCodec has no std::vector");
    }

    void setOwnedBuffer(std::shared_ptr<T[]> buf, size_t len) {
        data = std::move(buf);
        length = len;
    }

    T* getPointer() const { return data.get(); }
};