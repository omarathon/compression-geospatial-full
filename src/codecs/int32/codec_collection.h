#include <vector>

#include "composite_codec.h"
#include "custom_unvec_logic_codecs.h"
#include "fastpfor_codecs.h"
#include "fastpfor_fused_codecs.h"
#include "generic_codecs.h"
#include "simdcomp_codecs.h"
#include "simdcomp_fused_codecs.h"

std::vector<std::unique_ptr<StatefulIntegerCodec<int32_t>>>
InitLogicalCodecs() {
  std::vector<std::unique_ptr<StatefulIntegerCodec<int32_t>>> codecs;

  codecs.push_back(std::make_unique<DeltaCodec>());
  codecs.push_back(std::make_unique<FORCodec>());
  codecs.push_back(std::make_unique<RLECodec>());

  return codecs;
}

std::vector<std::unique_ptr<StatefulIntegerCodec<int32_t>>>
InitPhysicalCodecs() {
  std::vector<std::unique_ptr<StatefulIntegerCodec<int32_t>>> codecs;

  codecs.push_back(std::make_unique<SimdCompCodec>());
  codecs.push_back(std::make_unique<SimdCompFusedCodec>());

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
