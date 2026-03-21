#include <algorithm>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <memory>
#include <string>

#include "generic_codecs.h"

class DeltaCodec : public StatefulIntegerCodec<int32_t> {
 private:
  std::vector<int32_t> compressed_data;

 public:
  DeltaCodec() {}

  void EncodeArray(const int32_t* in, const size_t length) override {
    if (length > 0) {
      compressed_data.push_back(in[0]);
      for (size_t i = 1; i < length; ++i) {
        int32_t delta = in[i] - in[i - 1];
        uint32_t zigzagged = (delta << 1) ^ (delta >> 31);
        compressed_data.push_back(static_cast<int32_t>(
            zigzagged));  // Cast to int32_t if your vector is of type int32_t
      }
    }
  }

  void DecodeArray(int32_t* out, const size_t length) override {
    if (length > 0) {
      out[0] = compressed_data[0];
      for (size_t i = 1; i < length; ++i) {
        uint32_t zigzagged = static_cast<uint32_t>(compressed_data[i]);
        int32_t delta = (zigzagged >> 1) ^ -(zigzagged & 1);
        out[i] = delta + out[i - 1];
      }
    }
  }

  std::size_t EncodedNumValues() override { return compressed_data.size(); }

  std::size_t EncodedSizeValue() override { return sizeof(int32_t); }

  virtual ~DeltaCodec() {}

  std::string name() const override { return "custom_delta_unvec"; }

  std::size_t GetOverflowSize(size_t) const override { return 0; }

  StatefulIntegerCodec<int32_t>* CloneFresh() const override {
    return new DeltaCodec();
  }

  void AllocEncoded(const int32_t* in, size_t length) override {
    compressed_data.reserve(length);
  };

  void clear() override {
    compressed_data.clear();
    compressed_data.shrink_to_fit();
  }

  std::vector<int32_t>& GetEncoded() override { return compressed_data; };
};

class FORCodec : public StatefulIntegerCodec<int32_t> {
 private:
  std::vector<int32_t> compressed_data;

 public:
  FORCodec() {}

  void EncodeArray(const int32_t* in, const size_t length) override {
    if (length == 0) return;

    int32_t referenceValue = *std::min_element(in, in + length);

    compressed_data.push_back(referenceValue);

    for (size_t i = 0; i < length; ++i) {
      int32_t delta = in[i] - referenceValue;
      uint32_t zigzag = (delta << 1) ^ (delta >> 31);
      compressed_data.push_back(static_cast<int32_t>(zigzag));
    }
  }

  void DecodeArray(int32_t* out, const size_t length) override {
    if (length == 0 || compressed_data.empty()) return;

    int32_t referenceValue = compressed_data[0];

    for (size_t i = 1; i <= length; ++i) {
      uint32_t zigzag = static_cast<uint32_t>(compressed_data[i]);
      int32_t delta = (zigzag >> 1) ^ -(zigzag & 1);
      out[i - 1] = delta + referenceValue;
    }
  }

  std::size_t EncodedNumValues() override { return compressed_data.size(); }

  std::size_t EncodedSizeValue() override { return sizeof(int32_t); }

  virtual ~FORCodec() {}

  std::string name() const override { return "custom_for_unvec"; }

  std::size_t GetOverflowSize(size_t) const override { return 0; }

  StatefulIntegerCodec<int32_t>* CloneFresh() const override {
    return new FORCodec();
  }

  void AllocEncoded(const int32_t* in, size_t length) override {
    compressed_data.reserve(length + 1);
  };

  void clear() override {
    compressed_data.clear();
    compressed_data.shrink_to_fit();
  }

  std::vector<int32_t>& GetEncoded() override { return compressed_data; };
};

class RLECodec : public StatefulIntegerCodec<int32_t> {
 private:
  std::vector<int32_t> compressed_data;

 public:
  RLECodec() {}

  void EncodeArray(const int32_t* in, const size_t length) override {
    for (size_t i = 0; i < length;) {
      int32_t currentValue = in[i];
      size_t runLength = 1;
      while (i + runLength < length && in[i + runLength] == currentValue) {
        ++runLength;
      }

      compressed_data.push_back(currentValue);
      compressed_data.push_back(runLength);
      i += runLength;
    }
    compressed_data.shrink_to_fit();
  }

  void DecodeArray(int32_t* out, const size_t length) override {
    size_t outIndex = 0;
    for (size_t i = 0; i < compressed_data.size(); i += 2) {
      int32_t value = compressed_data[i];
      size_t runLength = compressed_data[i + 1];
      std::fill_n(out + outIndex, runLength, value);
      outIndex += runLength;
    }
  }

  std::size_t EncodedNumValues() override { return compressed_data.size(); }

  std::size_t EncodedSizeValue() override { return sizeof(int32_t); }

  virtual ~RLECodec() {}

  std::string name() const override { return "custom_rle_unvec"; }

  std::size_t GetOverflowSize(size_t) const override { return 0; }

  StatefulIntegerCodec<int32_t>* CloneFresh() const override {
    return new RLECodec();
  }

  void AllocEncoded(const int32_t* in, size_t length) override {
    compressed_data.resize(2);
  };

  void clear() override {
    compressed_data.clear();
    compressed_data.shrink_to_fit();
  }

  std::vector<int32_t>& GetEncoded() override { return compressed_data; };
};
