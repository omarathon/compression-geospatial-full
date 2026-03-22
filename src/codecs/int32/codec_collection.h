#include <vector>

#include "composite_codec.h"
#include "custom_unvec_logic_codecs.h"
#include "custom_vec_logic_codecs.h"
#include "deflate_codecs.h"
#include "fastpfor_codecs.h"
#include "fastpfor_fused_codecs.h"
#include "frameofreference_codecs.h"
#include "generic_codecs.h"
#include "lz4_codecs.h"
#include "lzma_codecs.h"
#include "maskedvbyte_codecs.h"
#include "simdcomp_codecs.h"
#include "simdcomp_fused_codecs.h"
#include "streamvbyte_codecs.h"
#include "turbopfor_codecs.h"
#include "zstd_codecs.h"

std::vector<std::unique_ptr<StatefulIntegerCodec<int32_t>>>
InitLogicalCodecs() {
  std::vector<std::unique_ptr<StatefulIntegerCodec<int32_t>>> codecs;

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

std::vector<std::unique_ptr<StatefulIntegerCodec<int32_t>>>
InitPhysicalCodecs() {
  std::vector<std::unique_ptr<StatefulIntegerCodec<int32_t>>> codecs;

  codecs.push_back(std::make_unique<DeflateCodec>());
  codecs.push_back(std::make_unique<MaskedVByteCodec>());
  codecs.push_back(std::make_unique<MaskedVByteDeltaCodec>());
  codecs.push_back(std::make_unique<StreamVByteCodec>());
  codecs.push_back(std::make_unique<FrameOfReferenceTurboCodec>());
  codecs.push_back(std::make_unique<SimdCompCodec>());
  codecs.push_back(std::make_unique<SimdCompFusedCodec>());
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
    if (fastpfor_codec->name() == "Simple8b_RLE" ||
        fastpfor_codec->name() == "Simple9_RLE" ||
        fastpfor_codec->name() == "SimplePFor+VariableByte" ||
        fastpfor_codec->name() == "SIMDGroupSimple+VariableByte" ||
        fastpfor_codec->name() == "SIMDGroupSimple_RingBuf+VariableByte" ||
        fastpfor_codec->name() == "VSEncoding") {
      continue;
    }
    codecs.push_back(std::make_unique<FastPForCodec>(fastpfor_codec));
    codecs.push_back(std::make_unique<FastPForFusedCodec>(fastpfor_codec));
  }

  return codecs;
}

std::vector<std::unique_ptr<StatefulIntegerCodec<int32_t>>> InitCodecs(
    bool nonCascaded,
    std::unique_ptr<StatefulIntegerCodec<int32_t>> cascadeCodec) {
  std::vector<std::unique_ptr<StatefulIntegerCodec<int32_t>>> codecs;

  std::vector<std::unique_ptr<StatefulIntegerCodec<int32_t>>> lCodecs =
      InitLogicalCodecs();
  std::vector<std::unique_ptr<StatefulIntegerCodec<int32_t>>> pCodecs =
      InitPhysicalCodecs();

  if (nonCascaded) {
    for (auto& codec : lCodecs) {
      codecs.push_back(
          std::unique_ptr<StatefulIntegerCodec<int32_t>>(codec->CloneFresh()));
    }
    for (auto& codec : pCodecs) {
      codecs.push_back(
          std::unique_ptr<StatefulIntegerCodec<int32_t>>(codec->CloneFresh()));
    }
  }

  if (cascadeCodec) {
    for (auto& pCodec : pCodecs) {
      auto cascadeCodecFresh = std::unique_ptr<StatefulIntegerCodec<int32_t>>(
          cascadeCodec->CloneFresh());
      auto pCodecFresh =
          std::unique_ptr<StatefulIntegerCodec<int32_t>>(pCodec->CloneFresh());
      auto compositeCodec =
          std::make_unique<CompositeStatefulIntegerCodec<int32_t>>(
              std::move(cascadeCodecFresh), std::move(pCodecFresh));
      codecs.push_back(std::move(compositeCodec));
    }
  }

  return codecs;
}
