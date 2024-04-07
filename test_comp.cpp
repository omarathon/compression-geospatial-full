#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <vector>
#include <memory>
#include <random>
#include <unordered_set>
#include <unordered_map>
#include <cmath>
#include <limits>
#include <any>
#include <variant>

#include "codecs/generic_codecs.h"
#include "codecs/composite_codec.h"
#include "codecs/deflate_codecs.h"
#include "codecs/fastpfor_codecs.h"
#include "codecs/custom_unvec_logic_codecs.h"
#include "codecs/custom_vec_logic_codecs.h"
#include "codecs/maskedvbyte_codecs.h"
#include "codecs/streamvbyte_codecs.h"
#include "codecs/lz4_codecs.h"
#include "codecs/lzma_codecs.h"
#include "codecs/zstd_codecs.h"
#include "codecs/turbopfor_codecs.h"
#include "codecs/frameofreference_codecs.h"
#include "codecs/simdcomp_codecs.h"

bool test_codec(std::vector<int32_t>& data, StatefulIntegerCodec<int32_t>& codec) {
    try {
        codec.clear();
        codec.allocEncoded(data.data(), data.size());
        codec.encodeArray(data.data(), data.size());
    }
    catch (const std::exception& error) {
        std::cerr << "error encoding " << codec.name() << ": " << error.what() << std::endl;
        return false;
    }
    
    size_t numCodedValues = codec.encodedNumValues();
    size_t sizeCodedValue = codec.encodedSizeValue();
    std::vector<int32_t> data_back(data.size() + codec.getOverflowSize(data.size()));

    try {
        codec.decodeArray(data_back.data(), data.size());
    }
    catch (const std::exception& error) {
        std::cerr << "error decoding " << codec.name() << ": " << error.what() << std::endl;
        return false;
    }

    for (int i = 0; i < data.size(); i++) {
        if (data[i] != data_back[i]) {
            std::cerr << "in!=out " << codec.name() << std::endl;
            return false;
        }
    }

    auto compressionFactor = (float)(numCodedValues * sizeCodedValue) / (float)(data.size() * sizeof(int32_t));
    auto bitsPerInt = (float)(numCodedValues * sizeCodedValue) / (float)data.size();
    std::cout << "cf " << compressionFactor << " bpi " << bitsPerInt << std::endl;   
    
    codec.clear();

    return true;
}


int main(int argc, char* argv[]) {
    if (argc != 3) {
        std::cerr << "Usage: " << argv[0] << " <range> <num values>" << std::endl;
        return 1;
    }
    int range = std::stoi(argv[1]);
    int n_val = std::stoi(argv[2]); 

    // Ranges:
    // FastPFor_Simple16: requires [0,2^28)
    std::vector<int32_t> data = {10,1,9,3,4,5,6,7,2,8};

    int32_t min;
    int32_t max;
    if (range == 0) { // Maximum range.
        min = 0x7FFFFFFF;
        max = 0x80000000;
    }
    else if (range == 1) {
        // smaller for FastPFor_Simple16 (FastPFor_Simple16: requires [0,2^28) -- error message from running it)
        min = 0; // Cannot compress negatives.
        max = pow(2,28) - 1;
    }
    else if (range == 2) {
        // even smaller for 2ibench_interpolativeblock
        // from testing, it works up to [0,2^26). but, 2ibench's test only tests up to 2^24: https://github.com/jermp/2i_bench/blob/df6be0c98b63ff85938cc4992e1e031afa8df244/test/test_block_codecs.cpp#L13
        // NOTE: we might want to just implement the codecs for uint32s...
        min  = 0; // Cannot compress negatives.
        max = pow(2,24) - 1;
    }

    std::random_device rd;  // Obtain a random number from hardware
    std::mt19937 gen(rd()); // Seed the generator
    std::uniform_int_distribution<> distr(min, max); // Define the range

    std::vector<int32_t> data_large;
    // NOTE: 250,000 number of values means the dictionary must be int32s so dictionary compressors don't give a <1 compression ratio.
    // Corresponds to 1MB of data
    for (int i = 0; i < n_val; i++) {
        data_large.push_back(i);
        data_large.push_back(min + i);
        data_large.push_back(max - i);

        data_large.push_back(distr(gen));
    }

    std::unordered_set<int32_t> unique_values(data_large.begin(), data_large.end());
    for (auto& val : data) {
        unique_values.insert(val);
    }
    size_t num_unique_values = unique_values.size();

    int num_bits_required = std::ceil(std::log2(num_unique_values));

    std::any dict;
    std::vector<int32_t> reverseDict;
    // std::any zstdDict; // TODO: implement zstd dict

    std::vector<std::unique_ptr<StatefulIntegerCodec<int32_t>>> codecs;

    // Function to create and add codecs
    auto createAndAddCodecs = [&](auto dummy) {
        using dict_type = decltype(dummy);
        std::unordered_map<int32_t, dict_type> localDict;
        reverseDict.resize(num_unique_values);
        dict_type index = 0;
        for (int32_t value : unique_values) {
            localDict[value] = index;
            reverseDict[index] = value;
            ++index;
        }

        dict = std::move(localDict); // Store the dictionary in std::any

        codecs.push_back(std::make_unique<DictCodec<dict_type>>(
            std::any_cast<std::unordered_map<int32_t, dict_type>&>(dict), reverseDict));
        codecs.push_back(std::make_unique<DictCodecAVX2<dict_type>>(
            std::any_cast<std::unordered_map<int32_t, dict_type>&>(dict), reverseDict));
        codecs.push_back(std::make_unique<DictCodecPacking<dict_type>>(
            std::any_cast<std::unordered_map<int32_t, dict_type>&>(dict), reverseDict));
        codecs.push_back(std::make_unique<DictCodecPackingAVX2<dict_type>>(
            std::any_cast<std::unordered_map<int32_t, dict_type>&>(dict), reverseDict));
    };

    if (num_bits_required <= 8) {
        createAndAddCodecs(uint8_t{});
    } else if (num_bits_required <= 16) {
        createAndAddCodecs(uint16_t{});
    } else {
        createAndAddCodecs(uint32_t{});
    }

    codecs.push_back(std::make_unique<DeltaCodec>());
    codecs.push_back(std::make_unique<DeltaCodecSSE42>());
    codecs.push_back(std::make_unique<DeltaCodecAVX2>());
    codecs.push_back(std::make_unique<DeltaCodecAVX512>());
    codecs.push_back(std::make_unique<FORCodecAVX512>());
    codecs.push_back(std::make_unique<RLECodecAVX512>());
    codecs.push_back(std::make_unique<FORCodec>());
    codecs.push_back(std::make_unique<FORCodecSSE42>());
    codecs.push_back(std::make_unique<FORCodecAVX2>());
    codecs.push_back(std::make_unique<RLECodec>());
    codecs.push_back(std::make_unique<RLECodecSSE42>());
    codecs.push_back(std::make_unique<RLECodecAVX2>());
    
    codecs.push_back(std::make_unique<DeflateCodec>());
    codecs.push_back(std::make_unique<MaskedVByteCodec>());
    codecs.push_back(std::make_unique<MaskedVByteDeltaCodec>());
    codecs.push_back(std::make_unique<StreamVByteCodec>());
    codecs.push_back(std::make_unique<FrameOfReferenceCodec>());
    codecs.push_back(std::make_unique<FrameOfReferenceTurboCodec>());
    codecs.push_back(std::make_unique<SimdCompCodec>());
    codecs.push_back(std::make_unique<LZ4Codec>()); // WORKS but slow
    codecs.push_back(std::make_unique<ZstdCodec>(3));
    for (size_t tpfCodecId = 1; tpfCodecId <= 20; tpfCodecId++) {
        if (tpfCodecId != 11) {
            codecs.push_back(std::make_unique<TurboPForCodec>(tpfCodecId));

            // Make composites
            auto deltaCodec = std::make_unique<DeltaCodecAVX512>();
            auto turboPForCodec = std::make_unique<TurboPForCodec>(tpfCodecId);
            auto compositeCodec = std::make_unique<CompositeStatefulIntegerCodec<int32_t>>(
                std::move(deltaCodec), std::move(turboPForCodec)
            );
            codecs.push_back(std::move(compositeCodec));

            auto rleCodec = std::make_unique<RLECodecAVX512>();
            turboPForCodec = std::make_unique<TurboPForCodec>(tpfCodecId);
            compositeCodec = std::make_unique<CompositeStatefulIntegerCodec<int32_t>>(
                std::move(rleCodec), std::move(turboPForCodec)
            );
            codecs.push_back(std::move(compositeCodec));

            auto forCodec = std::make_unique<FORCodecAVX512>();
            turboPForCodec = std::make_unique<TurboPForCodec>(tpfCodecId);
            compositeCodec = std::make_unique<CompositeStatefulIntegerCodec<int32_t>>(
                std::move(forCodec), std::move(turboPForCodec)
            );
            codecs.push_back(std::move(compositeCodec));
        }
    }

    CODECFactory fastpfor_codecfactory;
    for (auto& fastpfor_codec : fastpfor_codecfactory.allSchemes()) {
        if (fastpfor_codec->name() == "Simple8b_RLE"
            || fastpfor_codec->name() == "Simple9_RLE"
            || fastpfor_codec->name() == "SimplePFor+VariableByte"
            || fastpfor_codec->name() == "VSEncoding") {
            // Broken.
            continue;
        }
        codecs.push_back(std::make_unique<FastPForCodec>(fastpfor_codec));

        // Make composites
        auto deltaCodec = std::make_unique<DeltaCodecAVX512>();
        auto fastPForCodec = std::make_unique<FastPForCodec>(fastpfor_codec);
        auto compositeCodec = std::make_unique<CompositeStatefulIntegerCodec<int32_t>>(
            std::move(deltaCodec), std::move(fastPForCodec)
        );
        codecs.push_back(std::move(compositeCodec));

        auto rleCodec = std::make_unique<RLECodecAVX512>();
        fastPForCodec = std::make_unique<FastPForCodec>(fastpfor_codec);
        compositeCodec = std::make_unique<CompositeStatefulIntegerCodec<int32_t>>(
            std::move(rleCodec), std::move(fastPForCodec)
        );
        codecs.push_back(std::move(compositeCodec));

        auto forCodec = std::make_unique<FORCodecAVX512>();
        fastPForCodec = std::make_unique<FastPForCodec>(fastpfor_codec);
        compositeCodec = std::make_unique<CompositeStatefulIntegerCodec<int32_t>>(
            std::move(forCodec), std::move(fastPForCodec)
        );
        codecs.push_back(std::move(compositeCodec));
    }

    std::cout << "#codecs=" << codecs.size() << std::endl;

    int ci = 1;
    for (auto& codec : codecs) {
        std::cout << "codec " << ci++ << " / " << codecs.size() << std::endl;
        std::cout << codec->name() << " small" << std::endl;
        bool s1 = test_codec(data, *codec);
        std::cout << codec->name() << " large" << std::endl;
        bool s2 = test_codec(data_large, *codec);
        if (s1 && s2) {
            std::cout << codec->name() << " ok" << std::endl;
        }
        else {
            std::cout << codec->name() << " bad" << std::endl;
        }
    }
}