#include "generic_codecs.h"
#include <string>
#include <memory>
#include <stdexcept>

///////////////////////////////////////////////////////////////////////
// composite codec for one followed by another //
// ! assumes the first codec encodes to the same type as the input ! //
// ! assumes both codecs encode the same type ! //
///////////////////////////////////////////////////////////////////////

template<typename T>
class CompositeStatefulIntegerCodec : public StatefulIntegerCodec<T> {
private:
    std::unique_ptr<StatefulIntegerCodec<T>> firstCodec;
    std::unique_ptr<StatefulIntegerCodec<T>> secondCodec;
    size_t intermediateEncodedSize; // Cache the size of the intermediate array

public:
    CompositeStatefulIntegerCodec(std::unique_ptr<StatefulIntegerCodec<T>> first, std::unique_ptr<StatefulIntegerCodec<T>> second)
        : firstCodec(std::move(first)), secondCodec(std::move(second)), intermediateEncodedSize(0) {}

    // Need to allocate for both codecs but don't know how to allocate the intermediate data in advance,
    // so this is not supported.
    void allocEncoded(const T* in, size_t length) {
      
    }

    void encodeArray(const T *in, const size_t length) override {
        firstCodec->allocEncoded(in, length);
        firstCodec->encodeArray(in, length);
        auto& intermediateData = firstCodec->getEncoded();
        intermediateEncodedSize = intermediateData.size(); // Cache the size of the intermediate array
        secondCodec->allocEncoded(intermediateData.data(), intermediateEncodedSize);
        secondCodec->encodeArray(intermediateData.data(), intermediateEncodedSize);
        firstCodec->clear();
    }

    void decodeArray(T *out, const size_t length) override {
        // Directly decode the second codec into the first.
        std::vector<T>& decodedIntermediateData = firstCodec->getEncoded(); // Destination.
        decodedIntermediateData.resize(intermediateEncodedSize + secondCodec->getOverflowSize(intermediateEncodedSize)); // NOTE: reserve doesn't work.
        secondCodec->decodeArray(decodedIntermediateData.data(), intermediateEncodedSize);
        decodedIntermediateData.resize(intermediateEncodedSize);
        // Now the first codec has the original intermediate data. Decode.
        firstCodec->decodeArray(out, length);
    }

    size_t benchEncode(const T *in, const size_t length) override {
        firstCodec->allocEncoded(in, length);

        auto tenc1Start = std::chrono::high_resolution_clock::now();
        firstCodec->encodeArray(in, length);
        auto tenc1End = std::chrono::high_resolution_clock::now();

        auto& intermediateData = firstCodec->getEncoded();
        intermediateEncodedSize = intermediateData.size(); // Cache the size of the intermediate array

        secondCodec->allocEncoded(intermediateData.data(), intermediateEncodedSize);

        auto tenc2Start = std::chrono::high_resolution_clock::now();
        secondCodec->encodeArray(intermediateData.data(), intermediateEncodedSize);
        auto tenc2End = std::chrono::high_resolution_clock::now();

        firstCodec->clear();

        return std::chrono::duration_cast<std::chrono::nanoseconds>(tenc1End - tenc1Start).count()
             + std::chrono::duration_cast<std::chrono::nanoseconds>(tenc2End - tenc2Start).count();
    }

    size_t benchDecode(T *out, const size_t length) override {
        // Directly decode the second codec into the first.
        std::vector<T>& decodedIntermediateData = firstCodec->getEncoded(); // Destination.
        decodedIntermediateData.reserve(intermediateEncodedSize + secondCodec->getOverflowSize(intermediateEncodedSize));

        auto tdec1Start = std::chrono::high_resolution_clock::now();
        secondCodec->decodeArray(decodedIntermediateData.data(), intermediateEncodedSize);
        auto tdec1End = std::chrono::high_resolution_clock::now();

        decodedIntermediateData.resize(intermediateEncodedSize);
        // Now the first codec has the original intermediate data. Decode.

        auto tdec2Start = std::chrono::high_resolution_clock::now();
        firstCodec->decodeArray(out, length);
        auto tdec2End = std::chrono::high_resolution_clock::now();

        secondCodec->clear();

        return std::chrono::duration_cast<std::chrono::nanoseconds>(tdec1End - tdec1Start).count()
             + std::chrono::duration_cast<std::chrono::nanoseconds>(tdec2End - tdec2Start).count();
    }

    std::size_t encodedNumValues() override {
      return secondCodec->encodedNumValues();
    }

    std::size_t encodedSizeValue() override {
      return secondCodec->encodedSizeValue();
    }

    virtual ~CompositeStatefulIntegerCodec() {
      firstCodec.reset();
      secondCodec.reset();
    }

    std::string name() const override {
      return "[+]_" + firstCodec->name() + "+" + secondCodec->name();
    }

    std::size_t getOverflowSize(size_t length) const override {
      return firstCodec->getOverflowSize(length); // Overflow is used for decoding, and the last decoder is the first codec.
    }

    void clear() override {
      firstCodec->clear();
      secondCodec->clear();
    }
    
    StatefulIntegerCodec<T>* cloneFresh() const override {
        auto clonedFirstCodec = firstCodec->cloneFresh();
        auto clonedSecondCodec = secondCodec->cloneFresh();
        return new CompositeStatefulIntegerCodec<T>(
            std::move(std::unique_ptr<StatefulIntegerCodec<T>>(clonedFirstCodec)), 
            std::move(std::unique_ptr<StatefulIntegerCodec<T>>(clonedSecondCodec))
        );
    }

    std::vector<T>& getEncoded() override {
      return secondCodec->getEncoded();
    }
};