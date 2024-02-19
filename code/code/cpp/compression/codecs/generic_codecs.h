#pragma once

#include <vector>
#include <cstdint>
#include <chrono>

//////////////////////////
// general single codec //
//////////////////////////

template <typename T> // T: type of data being compressed
class StatefulIntegerCodec {
public:
  virtual void allocEncoded(const T* in, size_t length) = 0;

  virtual void encodeArray(const T *in, const size_t length) = 0;

  // Allocates storage for encoded data and times how long it takes to encode it.
  // Returns the duration in **nanoseconds**.
  virtual size_t benchEncode(const T *in, const size_t length) {
    allocEncoded(in, length);
    auto tencStart = std::chrono::high_resolution_clock::now();
    encodeArray(in, length);
    auto tencEnd = std::chrono::high_resolution_clock::now();
    return std::chrono::duration_cast<std::chrono::nanoseconds>
      (tencEnd - tencStart).count();
  }

  virtual void decodeArray(T *out, const size_t length) = 0;

  virtual size_t benchDecode(T *out, const size_t length) {
    auto tdecStart = std::chrono::high_resolution_clock::now();
    decodeArray(out, length);
    auto tdecEnd = std::chrono::high_resolution_clock::now();
    return std::chrono::duration_cast<std::chrono::nanoseconds>
      (tdecEnd - tdecStart).count();
  }

  virtual std::size_t encodedNumValues() = 0;

  virtual std::size_t encodedSizeValue() = 0;

  virtual ~StatefulIntegerCodec() {}

  virtual std::string name() const = 0;

  virtual std::size_t getOverflowSize(size_t length) const = 0;

  virtual StatefulIntegerCodec<T>* cloneFresh() const = 0;

  virtual void clear() = 0;

  virtual std::vector<T>& getEncoded() = 0;
};