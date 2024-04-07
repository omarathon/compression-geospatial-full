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
#include <vector>
#include <unordered_map>

template <typename T> // T : dictionary type
std::vector<std::unique_ptr<StatefulIntegerCodec<int32_t>>> initLogicalCodecs
    (std::unordered_map<int32_t, T>& dict, std::vector<int32_t>& reverseDict) 
{
    std::vector<std::unique_ptr<StatefulIntegerCodec<int32_t>>> codecs;

    codecs.push_back(std::make_unique<DictCodec<T>>(dict, reverseDict));
    codecs.push_back(std::make_unique<DictCodecAVX2<T>>(dict, reverseDict));

    codecs.push_back(std::make_unique<DeltaCodec>());
    codecs.push_back(std::make_unique<DeltaCodecSSE42>());
    codecs.push_back(std::make_unique<DeltaCodecAVX2>());
    codecs.push_back(std::make_unique<DeltaCodecAVX512>());
    
    codecs.push_back(std::make_unique<FORCodec>());
    codecs.push_back(std::make_unique<FORCodecSSE42>());
    codecs.push_back(std::make_unique<FORCodecAVX2>());
    codecs.push_back(std::make_unique<FORCodecAVX512>());

    codecs.push_back(std::make_unique<RLECodec>());
    codecs.push_back(std::make_unique<RLECodecSSE42>());
    codecs.push_back(std::make_unique<RLECodecAVX2>());
    codecs.push_back(std::make_unique<RLECodecAVX512>());

    return codecs;
}

template <typename T> // T : dictionary type
std::vector<std::unique_ptr<StatefulIntegerCodec<int32_t>>> initPhysicalCodecs
    (std::unordered_map<int32_t, T>& dict, std::vector<int32_t>& reverseDict) 
{
    std::vector<std::unique_ptr<StatefulIntegerCodec<int32_t>>> codecs;

    codecs.push_back(std::make_unique<DeflateCodec>());
    codecs.push_back(std::make_unique<MaskedVByteCodec>());
    codecs.push_back(std::make_unique<MaskedVByteDeltaCodec>());
    codecs.push_back(std::make_unique<StreamVByteCodec>());
    codecs.push_back(std::make_unique<FrameOfReferenceTurboCodec>());
    codecs.push_back(std::make_unique<SimdCompCodec>());
    codecs.push_back(std::make_unique<LZ4Codec>());
    codecs.push_back(std::make_unique<ZstdCodec>(1));
    codecs.push_back(std::make_unique<ZstdCodec>(3));
    codecs.push_back(std::make_unique<ZstdCodec>(5));

    // TurboPFor Codecs
    for (size_t tpfCodecId = 1; tpfCodecId <= 20; tpfCodecId++) {
        if (tpfCodecId != 11) {
            codecs.push_back(std::make_unique<TurboPForCodec>(tpfCodecId));
        }
    }

    // FastPFor Codecs
    CODECFactory fastpfor_codecfactory;
    for (auto& fastpfor_codec : fastpfor_codecfactory.allSchemes()) {
        if (fastpfor_codec->name() == "Simple8b_RLE"
            || fastpfor_codec->name() == "Simple9_RLE"
            || fastpfor_codec->name() == "SimplePFor+VariableByte"
            || fastpfor_codec->name() == "SIMDGroupSimple+VariableByte" // Working in bench but not bench_tiff_comp on tiff data with RLE preceding
            || fastpfor_codec->name() == "SIMDGroupSimple_RingBuf+VariableByte" // Working in bench but not bench_tiff_comp on tiff data with RLE preceding
            || fastpfor_codec->name() == "VSEncoding") {
            // Broken.
            continue;
        }
        codecs.push_back(std::make_unique<FastPForCodec>(fastpfor_codec));
    }

    return codecs;
}


template <typename T> // T : dictionary type
std::vector<std::unique_ptr<StatefulIntegerCodec<int32_t>>> initCodecs
    (std::unordered_map<int32_t, T>& dict, std::vector<int32_t>& reverseDict, 
    bool nonCascaded, std::unique_ptr<StatefulIntegerCodec<int32_t>> cascadeCodec) 
{
    std::vector<std::unique_ptr<StatefulIntegerCodec<int32_t>>> codecs;

    std::vector<std::unique_ptr<StatefulIntegerCodec<int32_t>>> lCodecs = initLogicalCodecs(dict, reverseDict);
    std::vector<std::unique_ptr<StatefulIntegerCodec<int32_t>>> pCodecs = initPhysicalCodecs(dict, reverseDict);

    if (nonCascaded) {
        for (auto& codec : lCodecs) {
            codecs.push_back(std::unique_ptr<StatefulIntegerCodec<int32_t>>(codec->cloneFresh()));
        }
        for (auto& codec : pCodecs) {
            codecs.push_back(std::unique_ptr<StatefulIntegerCodec<int32_t>>(codec->cloneFresh()));
        }
    }

    if (cascadeCodec) {
        for (auto& pCodec : pCodecs) {
            auto cascadeCodecFresh = std::unique_ptr<StatefulIntegerCodec<int32_t>>(cascadeCodec->cloneFresh());
            auto pCodecFresh = std::unique_ptr<StatefulIntegerCodec<int32_t>>(pCodec->cloneFresh());
            auto compositeCodec = std::make_unique<CompositeStatefulIntegerCodec<int32_t>>(
                std::move(cascadeCodecFresh), std::move(pCodecFresh)
            );
            codecs.push_back(std::move(compositeCodec));
        }
    }

    return codecs;
}