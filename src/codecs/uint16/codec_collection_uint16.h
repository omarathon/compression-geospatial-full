#pragma once

#include <memory>
#include <vector>

#include "composite_codec.h"
#include "generic_codecs.h"
#include "direct_codec_uint16.h"
#include "simdcomp_fused_codec_uint16.h"
#include "fastpfor_fused_codec_uint16.h"
#include "custom_vec_logic_codecs.h"

inline std::vector<std::unique_ptr<StatefulIntegerCodec<uint16_t>>>
InitLogicalCodecsU16() {
  std::vector<std::unique_ptr<StatefulIntegerCodec<uint16_t>>> codecs;
  codecs.push_back(std::make_unique<DeltaCodecSSE42U16>());
  codecs.push_back(std::make_unique<FORCodecSSE42U16>());
  codecs.push_back(std::make_unique<RLECodecSSE42U16>());
  return codecs;
}

inline std::vector<std::unique_ptr<StatefulIntegerCodec<uint16_t>>>
InitPhysicalCodecsU16() {
  std::vector<std::unique_ptr<StatefulIntegerCodec<uint16_t>>> codecs;
  codecs.push_back(std::make_unique<DirectAccessCodecU16>());
  codecs.push_back(std::make_unique<SimdCompFusedCodecU16>());
  codecs.push_back(std::make_unique<FastPForFusedCodecU16>());
  return codecs;
}

inline std::vector<std::unique_ptr<StatefulIntegerCodec<uint16_t>>>
BuildAllCodecsU16() {
  std::vector<std::unique_ptr<StatefulIntegerCodec<uint16_t>>> codecs;

  auto lCodecs = InitLogicalCodecsU16();
  auto pCodecs = InitPhysicalCodecsU16();

  // Non-cascaded logical codecs
  for (auto& codec : lCodecs)
    codecs.push_back(
        std::unique_ptr<StatefulIntegerCodec<uint16_t>>(codec->CloneFresh()));

  // Non-cascaded physical codecs
  for (auto& codec : pCodecs)
    codecs.push_back(
        std::unique_ptr<StatefulIntegerCodec<uint16_t>>(codec->CloneFresh()));

  // Cascaded: each logical + each physical
  for (auto& lCodec : lCodecs) {
    for (auto& pCodec : pCodecs) {
      auto lFresh = std::unique_ptr<StatefulIntegerCodec<uint16_t>>(
          lCodec->CloneFresh());
      auto pFresh = std::unique_ptr<StatefulIntegerCodec<uint16_t>>(
          pCodec->CloneFresh());
      codecs.push_back(
          std::make_unique<CompositeStatefulIntegerCodec<uint16_t>>(
              std::move(lFresh), std::move(pFresh)));
    }
  }

  return codecs;
}
