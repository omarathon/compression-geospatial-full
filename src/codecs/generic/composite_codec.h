#include <memory>
#include <stdexcept>
#include <string>

#include "generic_codecs.h"

///////////////////////////////////////////////////////////////////////
// composite codec for one followed by another //
// ! assumes the first codec encodes to the same type as the input ! //
// ! assumes both codecs encode the same type ! //
///////////////////////////////////////////////////////////////////////

template <typename T>
class CompositeStatefulIntegerCodec : public StatefulIntegerCodec<T> {
 private:
  std::unique_ptr<StatefulIntegerCodec<T>> firstCodec;
  std::unique_ptr<StatefulIntegerCodec<T>> secondCodec;
  size_t intermediateEncodedSize;  // Cache the size of the intermediate array

 public:
  CompositeStatefulIntegerCodec(std::unique_ptr<StatefulIntegerCodec<T>> first,
                                std::unique_ptr<StatefulIntegerCodec<T>> second)
      : firstCodec(std::move(first)),
        secondCodec(std::move(second)),
        intermediateEncodedSize(0) {}

  // Need to allocate for both codecs but don't know how to allocate the
  // intermediate data in advance, so this is not supported.
  void AllocEncoded(const T* in, size_t length) {}

  void EncodeArray(const T* in, const size_t length) override {
    firstCodec->AllocEncoded(in, length);
    firstCodec->EncodeArray(in, length);
    auto& intermediateData = firstCodec->GetEncoded();
    intermediateEncodedSize =
        intermediateData.size();  // Cache the size of the intermediate array
    secondCodec->AllocEncoded(intermediateData.data(), intermediateEncodedSize);
    secondCodec->EncodeArray(intermediateData.data(), intermediateEncodedSize);
    firstCodec->clear();
  }

  void DecodeArray(T* out, const size_t length) override {
    // Directly decode the second codec into the first.
    std::vector<T>& decodedIntermediateData =
        firstCodec->GetEncoded();  // Destination.
    decodedIntermediateData.resize(
        intermediateEncodedSize +
        secondCodec->GetOverflowSize(
            intermediateEncodedSize));  // NOTE: reserve doesn't work.
    secondCodec->DecodeArray(decodedIntermediateData.data(),
                             intermediateEncodedSize);
    decodedIntermediateData.resize(intermediateEncodedSize);
    // Now the first codec has the original intermediate data. Decode.
    firstCodec->DecodeArray(out, length);
  }

  size_t BenchEncode(const T* in, const size_t length) override {
    firstCodec->AllocEncoded(in, length);

    auto tenc1Start = std::chrono::steady_clock::now();
    firstCodec->EncodeArray(in, length);
    auto tenc1End = std::chrono::steady_clock::now();

    auto& intermediateData = firstCodec->GetEncoded();
    intermediateEncodedSize =
        intermediateData.size();  // Cache the size of the intermediate array

    secondCodec->AllocEncoded(intermediateData.data(), intermediateEncodedSize);

    auto tenc2Start = std::chrono::steady_clock::now();
    secondCodec->EncodeArray(intermediateData.data(), intermediateEncodedSize);
    auto tenc2End = std::chrono::steady_clock::now();

    firstCodec->clear();

    return std::chrono::duration_cast<std::chrono::nanoseconds>(tenc1End -
                                                                tenc1Start)
               .count() +
           std::chrono::duration_cast<std::chrono::nanoseconds>(tenc2End -
                                                                tenc2Start)
               .count();
  }

  size_t BenchDecode(T* out, const size_t length) override {
    // Directly decode the second codec into the first.
    std::vector<T>& decodedIntermediateData =
        firstCodec->GetEncoded();  // Destination.
    decodedIntermediateData.reserve(
        intermediateEncodedSize +
        secondCodec->GetOverflowSize(intermediateEncodedSize));

    auto tdec1Start = std::chrono::steady_clock::now();
    secondCodec->DecodeArray(decodedIntermediateData.data(),
                             intermediateEncodedSize);
    auto tdec1End = std::chrono::steady_clock::now();

    decodedIntermediateData.resize(intermediateEncodedSize);
    // Now the first codec has the original intermediate data. Decode.

    auto tdec2Start = std::chrono::steady_clock::now();
    firstCodec->DecodeArray(out, length);
    auto tdec2End = std::chrono::steady_clock::now();

    secondCodec->clear();

    return std::chrono::duration_cast<std::chrono::nanoseconds>(tdec1End -
                                                                tdec1Start)
               .count() +
           std::chrono::duration_cast<std::chrono::nanoseconds>(tdec2End -
                                                                tdec2Start)
               .count();
  }

  std::size_t EncodedNumValues() override {
    return secondCodec->EncodedNumValues();
  }

  std::size_t EncodedSizeValue() override {
    return secondCodec->EncodedSizeValue();
  }

  virtual ~CompositeStatefulIntegerCodec() {
    firstCodec.reset();
    secondCodec.reset();
  }

  std::string name() const override {
    return "[+]_" + firstCodec->name() + "+" + secondCodec->name();
  }

  std::size_t GetOverflowSize(size_t length) const override {
    return firstCodec->GetOverflowSize(
        length);  // Overflow is used for decoding, and the last decoder is the
                  // first codec.
  }

  void clear() override {
    firstCodec->clear();
    secondCodec->clear();
  }

  StatefulIntegerCodec<T>* CloneFresh() const override {
    auto clonedFirstCodec = firstCodec->CloneFresh();
    auto clonedSecondCodec = secondCodec->CloneFresh();
    return new CompositeStatefulIntegerCodec<T>(
        std::move(std::unique_ptr<StatefulIntegerCodec<T>>(clonedFirstCodec)),
        std::move(std::unique_ptr<StatefulIntegerCodec<T>>(clonedSecondCodec)));
  }

  std::vector<T>& GetEncoded() override { return secondCodec->GetEncoded(); }
};