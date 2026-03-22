#pragma once

#include <chrono>
#include <cstdint>
#include <vector>

//////////////////////////
// general single codec //
//////////////////////////

template <typename T>  // T: type of data being compressed
class StatefulIntegerCodec {
 public:
  virtual void AllocEncoded(const T *in, size_t length) = 0;

  virtual void EncodeArray(const T *in, const size_t length) = 0;

  // Allocates storage for encoded data and times how long it takes to encode
  // it. Returns the duration in **nanoseconds**.
  virtual size_t BenchEncode(const T *in, const size_t length) {
    AllocEncoded(in, length);
    auto tencStart = std::chrono::steady_clock::now();
    EncodeArray(in, length);
    auto tencEnd = std::chrono::steady_clock::now();
    return std::chrono::duration_cast<std::chrono::nanoseconds>(tencEnd -
                                                                tencStart)
        .count();
  }

  virtual void DecodeArray(T *out, const size_t length) = 0;

  virtual size_t BenchDecode(T *out, const size_t length) {
    auto tdecStart = std::chrono::steady_clock::now();
    DecodeArray(out, length);
    auto tdecEnd = std::chrono::steady_clock::now();
    return std::chrono::duration_cast<std::chrono::nanoseconds>(tdecEnd -
                                                                tdecStart)
        .count();
  }

  virtual std::size_t EncodedNumValues() = 0;

  virtual std::size_t EncodedSizeValue() = 0;

  virtual ~StatefulIntegerCodec() {}

  virtual std::string name() const = 0;

  virtual std::size_t GetOverflowSize(size_t length) const = 0;

  virtual StatefulIntegerCodec<T> *CloneFresh() const = 0;

  virtual void clear() = 0;

  virtual std::vector<T> &GetEncoded() = 0;
};