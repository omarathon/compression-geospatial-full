#pragma once

#include <memory>
#include <vector>

#include "byte_codecs.h"
#include "charls_codecs.h"
#include "composite_codec.h"
#include "lerc_codecs.h"
#include "snappy_codecs.h"
#include "generic_codecs.h"
#include "openjpeg_codecs.h"
#include "png_codecs.h"
#include "direct_codec_uint16.h"
#include "simdcomp_fused_codec_uint16.h"
#include "fastpfor_fused_codec_uint16.h"

#include "custom_vec_logic_codecs.h"
#include "custom_unvec_logic_codecs_u16.h"  // includes predictive_codecs_u16.h
#include "turbopfor_codecs_u16.h"
// predictive_codecs_u16.h is transitively included above

inline std::vector<std::unique_ptr<StatefulIntegerCodec<uint16_t>>>
InitLogicalCodecsU16() {
  std::vector<std::unique_ptr<StatefulIntegerCodec<uint16_t>>> codecs;

  // Vectorized (SSE4.2) logical codecs
  // codecs.push_back(std::make_unique<DeltaCodecSSE42U16>());
  // codecs.push_back(std::make_unique<FORCodecSSE42U16>());
  // codecs.push_back(std::make_unique<RLECodecSSE42U16>());

  // Scalar logical codecs
  codecs.push_back(std::make_unique<DeltaCodecU16>());
  codecs.push_back(std::make_unique<DoubleDeltaCodecU16>());
  codecs.push_back(std::make_unique<FORCodecU16>());
  codecs.push_back(std::make_unique<RLECodecU16>());

  // Lossless JPEG predictors (2D, stride = sqrt(length))
  codecs.push_back(std::make_unique<JpegPred1CodecU16>());
  codecs.push_back(std::make_unique<JpegPred2CodecU16>());
  codecs.push_back(std::make_unique<JpegPred3CodecU16>());
  codecs.push_back(std::make_unique<JpegPred4CodecU16>());
  codecs.push_back(std::make_unique<JpegPred5CodecU16>());
  codecs.push_back(std::make_unique<JpegPred6CodecU16>());
  codecs.push_back(std::make_unique<JpegPred7CodecU16>());

  // JPEG-LS / edge-aware predictors
  codecs.push_back(std::make_unique<JpegLSMedCodecU16>());
  codecs.push_back(std::make_unique<PaethCodecU16>());

  return codecs;
}

inline std::vector<std::unique_ptr<StatefulIntegerCodec<uint16_t>>>
InitPhysicalCodecsU16() {
  std::vector<std::unique_ptr<StatefulIntegerCodec<uint16_t>>> codecs;
  codecs.push_back(std::make_unique<SimdCompFusedCodecU16>());
  codecs.push_back(std::make_unique<FastPForFusedCodecU16>());
  codecs.push_back(std::make_unique<TurboPForCodecU16>(3)); // turbopfor
  codecs.push_back(std::make_unique<TurboPForCodecU16>(7)); // turbopack

  return codecs;
}

inline std::vector<std::unique_ptr<StatefulIntegerCodec<uint16_t>>>
InitHeavyPhysicalCodecsU16() {
  std::vector<std::unique_ptr<StatefulIntegerCodec<uint16_t>>> codecs;

  codecs.push_back(std::make_unique<DeflateCodec<uint16_t>>());
  codecs.push_back(std::make_unique<LZ4Codec<uint16_t>>());
  codecs.push_back(std::make_unique<ZstdCodec<uint16_t>>(1));
  codecs.push_back(std::make_unique<ZstdCodec<uint16_t>>(3));
  codecs.push_back(std::make_unique<ZstdCodec<uint16_t>>(9));
  codecs.push_back(std::make_unique<LZMACodec<uint16_t>>());
  codecs.push_back(std::make_unique<PNGCodec<uint16_t>>());
  codecs.push_back(std::make_unique<OpenJPEGCodec<uint16_t>>());
#ifdef HAVE_CHARLS
  codecs.push_back(std::make_unique<CharLSCodec<uint16_t>>());
#endif
#ifdef HAVE_SNAPPY
  codecs.push_back(std::make_unique<SnappyCodec<uint16_t>>());
#endif
#ifdef HAVE_LERC
  codecs.push_back(std::make_unique<LERCCodec<uint16_t>>());
#endif

  return codecs;
}

inline std::vector<std::unique_ptr<StatefulIntegerCodec<uint16_t>>>
BuildAllCodecsU16() {
  std::vector<std::unique_ptr<StatefulIntegerCodec<uint16_t>>> codecs;

  auto lCodecs = InitLogicalCodecsU16();
  auto pCodecs = InitPhysicalCodecsU16();
  auto hCodecs = InitHeavyPhysicalCodecsU16();

  // Non-cascaded logical codecs
  for (auto& codec : lCodecs)
    codecs.push_back(
        std::unique_ptr<StatefulIntegerCodec<uint16_t>>(codec->CloneFresh()));

  // Non-cascaded physical codecs
  for (auto& codec : pCodecs)
    codecs.push_back(
        std::unique_ptr<StatefulIntegerCodec<uint16_t>>(codec->CloneFresh()));

  // Non-cascaded heavy codecs
  for (auto& codec : hCodecs)
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

  codecs.push_back(std::make_unique<DirectAccessCodecU16>());

  return codecs;
}
