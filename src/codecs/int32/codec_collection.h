#include <vector>

#include "byte_codecs.h"
#include "charls_codecs.h"
#include "composite_codec.h"
#include "lerc_codecs.h"
#include "snappy_codecs.h"
#include "custom_unvec_logic_codecs.h"
// #include "fastpfor_codecs.h"
// #include "fastpfor_fused_codecs.h"
#include "generic_codecs.h"
#include "openjpeg_codecs.h"
#include "png_codecs.h"
#include "predictive_codecs.h"
#include "turbopfor_codecs.h"
// #include "simdcomp_codecs.h"
// #include "simdcomp_fused_codecs.h"

std::vector<std::unique_ptr<StatefulIntegerCodec<int32_t>>>
InitLogicalCodecs() {
  std::vector<std::unique_ptr<StatefulIntegerCodec<int32_t>>> codecs;

  codecs.push_back(std::make_unique<DeltaCodec>());
  codecs.push_back(std::make_unique<DoubleDeltaCodec>());
  codecs.push_back(std::make_unique<FORCodec>());
  codecs.push_back(std::make_unique<RLECodec>());


  // Lossless JPEG predictors (2D, stride = sqrt(length))
  codecs.push_back(std::make_unique<JpegPred1Codec>());
  codecs.push_back(std::make_unique<JpegPred2Codec>());
  codecs.push_back(std::make_unique<JpegPred3Codec>());
  codecs.push_back(std::make_unique<JpegPred4Codec>());
  codecs.push_back(std::make_unique<JpegPred5Codec>());
  codecs.push_back(std::make_unique<JpegPred6Codec>());
  codecs.push_back(std::make_unique<JpegPred7Codec>());

  // JPEG-LS / edge-aware predictors
  codecs.push_back(std::make_unique<JpegLSMedCodec>());
  codecs.push_back(std::make_unique<PaethCodec>());

  return codecs;
}

std::vector<std::unique_ptr<StatefulIntegerCodec<int32_t>>>
InitPhysicalCodecs() {
  std::vector<std::unique_ptr<StatefulIntegerCodec<int32_t>>> codecs;

  codecs.push_back(std::make_unique<TurboPForCodec>(3)); // turbopfor
  codecs.push_back(std::make_unique<TurboPForCodec>(7)); // turbopack

  // codecs.push_back(std::make_unique<SimdCompCodec>());
  // codecs.push_back(std::make_unique<SimdCompFusedCodec>());

  // FastPFor Codecs
  // CODECFactory fastpfor_codecfactory;
  // for (auto& fastpfor_codec : fastpfor_codecfactory.allSchemes()) {
  //   if (fastpfor_codec->name() == "Simple8b_RLE" ||
  //       fastpfor_codec->name() == "Simple9_RLE" ||
  //       fastpfor_codec->name() == "SimplePFor+VariableByte" ||
  //       fastpfor_codec->name() == "SIMDGroupSimple+VariableByte" ||
  //       fastpfor_codec->name() == "SIMDGroupSimple_RingBuf+VariableByte" ||
  //       fastpfor_codec->name() == "VSEncoding") {
  //     continue;
  //   }
  //   codecs.push_back(std::make_unique<FastPForCodec>(fastpfor_codec));
  //   codecs.push_back(std::make_unique<FastPForFusedCodec>(fastpfor_codec));
  // }

  return codecs;
}

std::vector<std::unique_ptr<StatefulIntegerCodec<int32_t>>>
InitHeavyPhysicalCodecs() {
  std::vector<std::unique_ptr<StatefulIntegerCodec<int32_t>>> codecs;
  
  codecs.push_back(std::make_unique<DeflateCodec<int32_t>>());
  codecs.push_back(std::make_unique<LZ4Codec<int32_t>>());
  codecs.push_back(std::make_unique<ZstdCodec<int32_t>>(1));
  codecs.push_back(std::make_unique<ZstdCodec<int32_t>>(3));
  codecs.push_back(std::make_unique<ZstdCodec<int32_t>>(9));
  codecs.push_back(std::make_unique<LZMACodec<int32_t>>());
  codecs.push_back(std::make_unique<PNGCodec<int32_t>>());
  codecs.push_back(std::make_unique<OpenJPEGCodec<int32_t>>());
#ifdef HAVE_CHARLS
  codecs.push_back(std::make_unique<CharLSCodec<int32_t>>());
#endif
#ifdef HAVE_SNAPPY
  codecs.push_back(std::make_unique<SnappyCodec<int32_t>>());
#endif
#ifdef HAVE_LERC
  codecs.push_back(std::make_unique<LERCCodec<int32_t>>());
#endif

  return codecs;
}

inline std::vector<std::unique_ptr<StatefulIntegerCodec<int32_t>>>
BuildAllCodecs() {
  std::vector<std::unique_ptr<StatefulIntegerCodec<int32_t>>> codecs;

  auto lCodecs = InitLogicalCodecs();
  auto pCodecs = InitPhysicalCodecs();
  auto hCodecs = InitHeavyPhysicalCodecs();

  // Non-cascaded logical codecs
  for (auto& codec : lCodecs)
    codecs.push_back(
        std::unique_ptr<StatefulIntegerCodec<int32_t>>(codec->CloneFresh()));

  // Non-cascaded physical codecs
  for (auto& codec : pCodecs)
    codecs.push_back(
        std::unique_ptr<StatefulIntegerCodec<int32_t>>(codec->CloneFresh()));

  // Non-cascaded heavy codecs
  for (auto& codec : hCodecs)
    codecs.push_back(
        std::unique_ptr<StatefulIntegerCodec<int32_t>>(codec->CloneFresh())); 

  // Cascaded: each logical + each physical
  for (auto& lCodec : lCodecs) {
    for (auto& pCodec : pCodecs) {
      auto lFresh = std::unique_ptr<StatefulIntegerCodec<int32_t>>(
          lCodec->CloneFresh());
      auto pFresh = std::unique_ptr<StatefulIntegerCodec<int32_t>>(
          pCodec->CloneFresh());
      codecs.push_back(
          std::make_unique<CompositeStatefulIntegerCodec<int32_t>>(
              std::move(lFresh), std::move(pFresh)));
    }
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
