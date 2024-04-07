#include "generic_codecs.h"
#include <cstdint>
#include <string>
#include <iostream>
#include <cstring>
#include <memory>

class DeltaCodec : public StatefulIntegerCodec<int32_t> {
private:
    std::vector<int32_t> compressed_data;

public:
    DeltaCodec() {}

    void encodeArray(const int32_t *in, const size_t length) override {
        if (length > 0) {
            compressed_data.push_back(in[0]);
            for (size_t i = 1; i < length; ++i) {
                int32_t delta = in[i] - in[i - 1];
                uint32_t zigzagged = (delta << 1) ^ (delta >> 31);
                compressed_data.push_back(static_cast<int32_t>(zigzagged)); // Cast to int32_t if your vector is of type int32_t
            }
        }
    }

    void decodeArray(int32_t *out, const size_t length) override {
        if (length > 0) {
            out[0] = compressed_data[0];
            for (size_t i = 1; i < length; ++i) {
                uint32_t zigzagged = static_cast<uint32_t>(compressed_data[i]);
                int32_t delta = (zigzagged >> 1) ^ -(zigzagged & 1);
                out[i] = delta + out[i - 1];
            }
        }
    }

    std::size_t encodedNumValues() override {
      return compressed_data.size();
    }

    std::size_t encodedSizeValue() override {
      return sizeof(int32_t);
    }

    virtual ~DeltaCodec() {}

    std::string name() const override {
        return "custom_delta_unvec";
    }

    std::size_t getOverflowSize(size_t) const override {
      return 0;
    }

    StatefulIntegerCodec<int32_t>* cloneFresh() const override {
        return new DeltaCodec();
    }

    void allocEncoded(const int32_t* in, size_t length) override {
        compressed_data.reserve(length);
    };

    void clear() override {
        compressed_data.clear();
        compressed_data.shrink_to_fit();
    }

    std::vector<int32_t>& getEncoded() override {
        return compressed_data;
    };
};

template<typename T>
class DictCodecPacking : public StatefulIntegerCodec<int32_t> {
    static_assert(std::is_integral<T>::value, "Dictionary index type must be an integral type.");

private:
    std::unordered_map<int32_t, T>& dict;
    std::vector<int32_t>& reverseDict;
    std::vector<int32_t> compressed_data;

public:
    DictCodecPacking(std::unordered_map<int32_t, T>& dict, 
              std::vector<int32_t>& reverseDict)
        : dict{dict}, reverseDict{reverseDict} {}

    void encodeArray(const int32_t *in, const size_t length) override {
        uint32_t buffer = 0;
        uint8_t bufferIndex = 0;
        const size_t bitsPerIndex = sizeof(T) * 8;
        const size_t indexesPerInt = 32 / bitsPerIndex;

        for (size_t i = 0; i < length; ++i) {
            if (dict.find(in[i]) == dict.end()) {
                throw std::runtime_error("Dictionary encoding failed: value not found in dictionary.");
            }
            T index = dict.at(in[i]);
            buffer |= (static_cast<uint32_t>(index) << (bitsPerIndex * bufferIndex));
            bufferIndex++;

            if (bufferIndex == indexesPerInt || i == length - 1) {
                compressed_data.push_back(static_cast<int32_t>(buffer));
                buffer = 0;
                bufferIndex = 0;
            }
        }
    }

    void decodeArray(int32_t *out, const size_t length) override {
        size_t outIndex = 0;
        const size_t bitsPerIndex = sizeof(T) * 8;

        for (int32_t packed : compressed_data) {
            for (size_t i = 0; i < 32 / bitsPerIndex && outIndex < length; ++i) {
                T index = static_cast<T>((packed >> (bitsPerIndex * i)) & ((static_cast<uint64_t>(1) << bitsPerIndex) - 1));
                if (index >= static_cast<T>(reverseDict.size())) {
                    throw std::runtime_error("Dictionary decoding failed: index out of bounds.");
                }
                out[outIndex++] = reverseDict[index];
            }
        }
    }

    std::size_t encodedNumValues() override {
      return compressed_data.size();
    }

    std::size_t encodedSizeValue() override {
      return sizeof(int32_t);
    }

    virtual ~DictCodecPacking() {}

    std::string name() const override {
        return "custom_dict_packing_unvec";
    }

    std::size_t getOverflowSize(size_t) const override {
      return 0;
    }

    StatefulIntegerCodec<int32_t>* cloneFresh() const override {
        return new DictCodecPacking<T>(dict, reverseDict);
    }

    void allocEncoded(const int32_t* in, size_t length) override {
        const size_t bitsPerIndex = sizeof(T) * 8;
        const size_t indexesPerInt = 32 / bitsPerIndex;
        const size_t numPackedInts = (length + indexesPerInt - 1) / indexesPerInt;
        compressed_data.reserve(numPackedInts);
    };

    void clear() override {
        compressed_data.clear();
        compressed_data.shrink_to_fit();
    }

    std::vector<int32_t>& getEncoded() override {
        return compressed_data;
    };
};

template<typename T>
class DictCodec : public StatefulIntegerCodec<int32_t> {
    static_assert(std::is_integral<T>::value, "Dictionary index type must be an integral type.");

private:
    std::vector<T> compressed_data;
    std::unordered_map<int32_t, T>& dict;
    std::vector<int32_t>& reverseDict;

public:
    DictCodec(std::unordered_map<int32_t, T>& dict, 
              std::vector<int32_t>& reverseDict)
        : dict{dict}, reverseDict{reverseDict} {}

    void encodeArray(const int32_t *in, const size_t length) override {
        for (size_t i = 0; i < length; ++i) {
            if (dict.find(in[i]) == dict.end()) {
                throw std::runtime_error("Dictionary encoding failed: value not found in dictionary.");
            }
            T index = dict.at(in[i]);
            this->compressed_data.push_back(index);
        }
    }

    void decodeArray(int32_t *out, const size_t length) override {
        for (size_t i = 0; i < length; ++i) {
            T index = this->compressed_data[i];
            if (index >= static_cast<T>(reverseDict.size())) {
                throw std::runtime_error("Dictionary decoding failed: index out of bounds.");
            }
            out[i] = reverseDict[index];
        }
    }

    std::size_t encodedNumValues() override {
      return compressed_data.size();
    }

    std::size_t encodedSizeValue() override {
      return sizeof(T);
    }

    virtual ~DictCodec() {}

    std::string name() const override {
        return "custom_dict_unvec";
    }

    std::size_t getOverflowSize(size_t) const override {
      return 0;
    }

    StatefulIntegerCodec<int32_t>* cloneFresh() const override {
        return new DictCodec<T>(dict, reverseDict);
    }

    void allocEncoded(const int32_t* in, size_t length) override {
        compressed_data.reserve(length);
    };

    void clear() override {
        compressed_data.clear();
        compressed_data.shrink_to_fit();
    }

    std::vector<int32_t>& getEncoded() override {
        if (sizeof(T) == sizeof(int32_t)) {
            return reinterpret_cast<std::vector<int32_t>&>(compressed_data);
        }
        throw std::runtime_error("Encoded format does not match input. Cannot forward.");
        std::vector<int32_t> dummy{};
        return dummy;
    };
};

class FORCodec : public StatefulIntegerCodec<int32_t> {
private:
    std::vector<int32_t> compressed_data;

public:
    FORCodec() {}

    void encodeArray(const int32_t *in, const size_t length) override {
        if (length == 0) return;

        int32_t referenceValue = *std::min_element(in, in + length);

        compressed_data.push_back(referenceValue);

        for (size_t i = 0; i < length; ++i) {
            int32_t delta = in[i] - referenceValue;
            uint32_t zigzag = (delta << 1) ^ (delta >> 31);
            compressed_data.push_back(static_cast<int32_t>(zigzag));
        }
    }

    void decodeArray(int32_t *out, const size_t length) override {
        if (length == 0 || compressed_data.empty()) return;

        int32_t referenceValue = compressed_data[0];

        for (size_t i = 1; i <= length; ++i) {
            uint32_t zigzag = static_cast<uint32_t>(compressed_data[i]);
            int32_t delta = (zigzag >> 1) ^ -(zigzag & 1);
            out[i - 1] = delta + referenceValue;
        }
    }
    
    std::size_t encodedNumValues() override {
      return compressed_data.size();
    }

    std::size_t encodedSizeValue() override {
      return sizeof(int32_t);
    }

    virtual ~FORCodec() {}

    std::string name() const override {
        return "custom_for_unvec";
    }

    std::size_t getOverflowSize(size_t) const override {
      return 0;
    }

    StatefulIntegerCodec<int32_t>* cloneFresh() const override {
        return new FORCodec();
    }

    void allocEncoded(const int32_t* in, size_t length) override {
        compressed_data.reserve(length + 1);
    };

    void clear() override {
        compressed_data.clear();
        compressed_data.shrink_to_fit();
    }

    std::vector<int32_t>& getEncoded() override {
        return compressed_data;
    };
};

class RLECodec : public StatefulIntegerCodec<int32_t> {
private:
    std::vector<int32_t> compressed_data;

public:
    RLECodec() {}

    void encodeArray(const int32_t *in, const size_t length) override {
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

    void decodeArray(int32_t *out, const size_t length) override {
        size_t outIndex = 0;
        for (size_t i = 0; i < compressed_data.size(); i += 2) {
            int32_t value = compressed_data[i];
            size_t runLength = compressed_data[i + 1];
            std::fill_n(out + outIndex, runLength, value);
            outIndex += runLength;
        }
    }
    
    std::size_t encodedNumValues() override {
      return compressed_data.size();
    }

    std::size_t encodedSizeValue() override {
      return sizeof(int32_t);
    }

    virtual ~RLECodec() {}

    std::string name() const override {
        return "custom_rle_unvec";
    }

    std::size_t getOverflowSize(size_t) const override {
      return 0;
    }

    StatefulIntegerCodec<int32_t>* cloneFresh() const override {
        return new RLECodec();
    }

    void allocEncoded(const int32_t* in, size_t length) override {
        compressed_data.resize(2);
    };

    void clear() override {
        compressed_data.clear();
        compressed_data.shrink_to_fit();
    }

    std::vector<int32_t>& getEncoded() override {
        return compressed_data;
    };
};


