#pragma once

#include <memory>
#include <vector>

#include "byte_codecs.h"
#include "charls_codecs.h"
#include "composite_codec.h"
#include "lerc_codecs.h"
#include "snappy_codecs.h"
#include "generic_codecs.h"
#include "direct_codec_uint16.h"
#include "openjpeg_codecs.h"
#include "png_codecs.h"
#include "simdcomp_fused_codec_uint16.h"
#include "simdcomp_fused_codec_uint16_w128.h"
#include "simdcomp_for_codec_uint16.h"
#include "simdcomp_for_codec_uint16_w128.h"
#include "simdcomp_for_codec_uint16_w256.h"
#include "fastpfor_fused_codec_uint16.h"

#include "custom_vec_logic_codecs.h"
#include "custom_unvec_logic_codecs_u16.h"  // includes predictive_codecs_u16.h
#include "turbopfor_codecs_u16.h"
#include "turbopfor_fused_codec_uint16.h"
#include "turbopfor_fused_256_codec_uint16.h"
#include "turbopfor_for_codec_uint16.h"
// predictive_codecs_u16.h is transitively included above

inline std::vector<std::unique_ptr<StatefulIntegerCodec<uint16_t>>>
InitLogicalCodecsU16() {
  std::vector<std::unique_ptr<StatefulIntegerCodec<uint16_t>>> codecs;

  // Vectorized (SSE4.2) logical codecs
  // codecs.push_back(std::make_unique<DeltaCodecSSE42U16>());
  // codecs.push_back(std::make_unique<FORCodecSSE42U16>());
  // codecs.push_back(std::make_unique<RLECodecSSE42U16>());

  // Scalar logical codecs
  // codecs.push_back(std::make_unique<DeltaCodecU16>());
  // codecs.push_back(std::make_unique<DoubleDeltaCodecU16>());
  // wfull outputs length+1 elements (65537 for a 256x256 block), which is not
  // divisible by kFusedSubBlockSize=256 and crashes fused second-stage codecs.
  // codecs.push_back(std::make_unique<FORCodecU16>());
  for (size_t w : {2u, 4u, 8u, 16u, 32u, 64u, 128u, 256u})
    for (bool sep : {false, true})
      codecs.push_back(std::make_unique<FORCodecU16>(w, sep));

  // FORHierarchicalCodecU16: variable output size → not safe with fused
  // physical codecs. Skip lw that don't divide gw.
  for (size_t gw : {128u, 256u})
    for (size_t lw : {2u, 4u, 8u, 16u, 32u, 64u, 128u, 256u})
      for (bool sep : {false, true})
        if (gw % lw == 0)
          codecs.push_back(std::make_unique<FORHierarchicalCodecU16>(gw, lw, sep));
  // RLE outputs 2*num_runs elements — arbitrary size, not guaranteed divisible
  // by kFusedSubBlockSize=256, crashes fused second-stage codecs.
  // codecs.push_back(std::make_unique<RLECodecU16>());

  // Lossless JPEG predictors (2D, stride = sqrt(length))
  // codecs.push_back(std::make_unique<JpegPred1CodecU16>());
  // codecs.push_back(std::make_unique<JpegPred2CodecU16>());
  // codecs.push_back(std::make_unique<JpegPred3CodecU16>());
  // codecs.push_back(std::make_unique<JpegPred4CodecU16>());
  // codecs.push_back(std::make_unique<JpegPred5CodecU16>());
  // codecs.push_back(std::make_unique<JpegPred6CodecU16>());
  // codecs.push_back(std::make_unique<JpegPred7CodecU16>());

  // JPEG-LS / edge-aware predictors
  // codecs.push_back(std::make_unique<JpegLSMedCodecU16>());
  // codecs.push_back(std::make_unique<PaethCodecU16>());

  return codecs;
}

// inline std::vector<std::unique_ptr<StatefulIntegerCodec<uint16_t>>>
// InitPhysicalCodecsU16Compressibility() {
//   std::vector<std::unique_ptr<StatefulIntegerCodec<uint16_t>>> codecs;
//   codecs.push_back(std::make_unique<SimdCompFusedCodecU16>());
//    codecs.push_back(std::make_unique<FastPForFusedCorrectedCodecU16>());        // global_b
//   codecs.push_back(std::make_unique<FastPForFusedCorrectedCodecU16>(false));   // adaptive_b
//   return codecs;
// }


inline std::vector<std::unique_ptr<StatefulIntegerCodec<uint16_t>>>
InitPhysicalCodecsU16() {
  std::vector<std::unique_ptr<StatefulIntegerCodec<uint16_t>>> codecs;
  codecs.push_back(std::make_unique<SimdCompFusedCodecU16>());       // 256-bit
  codecs.push_back(std::make_unique<SimdCompFusedCodecU16_128>());   // 128-bit
  // Both aggregate-sum implementations (unpack-widen vs madd) are registered for
  // the 128- and 256-bit FoR codecs so the bench sweep can compare them.
  for (FusedAggImpl agg : {FusedAggImpl::kUnpack, FusedAggImpl::kMadd}) {
    // ── 128-bit fused FoR (regular: window × {raw,packed-anchor}) ──
    for (size_t w : {4u, 8u, 16u, 32u, 64u, 128u, 256u})
      for (bool sep : {false, true})
        codecs.push_back(
            std::make_unique<SimdCompFusedForCodecU16_128>(w, sep, agg));
    // shuffle correction for all windows (vmovq+pshufb instead of vpbroadcastw)
    for (size_t w : {4u, 8u, 16u, 32u, 64u, 128u, 256u})
      for (bool sep : {false, true})
        codecs.push_back(
            std::make_unique<SimdCompFusedForCodecU16_128>(w, sep, agg, /*shuf=*/true));
    // nobc: scalar anchor accumulation, no SIMD correction overhead
    for (size_t w : {4u, 8u, 16u, 32u, 64u, 128u, 256u})
      for (bool sep : {false, true})
        codecs.push_back(
            std::make_unique<SimdCompFusedForCodecU16_128>(w, sep, agg, /*shuf=*/false, /*nobc=*/true));
    // ── 128-bit fused hierarchical FoR (outer ∈ {128,256}, inner | outer) ──
    for (size_t gw : {128u, 256u})
      for (size_t lw : {4u, 8u, 16u, 32u, 64u, 128u, 256u})
        if (lw <= gw && gw % lw == 0)
          codecs.push_back(
              std::make_unique<SimdCompFusedForHierarchicalCodecU16_128>(gw, lw,
                                                                          agg));
    // ── 256-bit fused FoR (regular: window × {raw,packed-anchor} × {shuf}) ──
    for (size_t w : {4u, 8u, 16u, 32u, 64u, 128u, 256u})
      for (bool sep : {false, true})
        for (bool shuf : {false, true})
          codecs.push_back(
              std::make_unique<SimdCompFusedForCodecU16_256>(w, sep, agg, shuf));
    // nobc: scalar anchor accumulation, no SIMD correction overhead
    for (size_t w : {4u, 8u, 16u, 32u, 64u, 128u, 256u})
      for (bool sep : {false, true})
        codecs.push_back(
            std::make_unique<SimdCompFusedForCodecU16_256>(w, sep, agg, /*shuf=*/false, /*nobc=*/true));
    // ── 256-bit fused hierarchical FoR (outer ∈ {128,256}, inner | outer) ──
    for (size_t gw : {128u, 256u})
      for (size_t lw : {4u, 8u, 16u, 32u, 64u, 128u, 256u})
        if (lw <= gw && gw % lw == 0)
          codecs.push_back(
              std::make_unique<SimdCompFusedForHierarchicalCodecU16_256>(gw, lw,
                                                                          agg));
  }
  // codecs.push_back(std::make_unique<SimdCompFusedDeltaLocalCodecU16>());
  // codecs.push_back(std::make_unique<SimdCompFusedDeltaCarryCodecU16>());
  // codecs.push_back(std::make_unique<SimdCompFusedForGlobalCodecU16>());        // w256
  // codecs.push_back(std::make_unique<SimdCompFusedForGlobalCodecU16>(128));    // w128
  // codecs.push_back(std::make_unique<SimdCompFusedForGlobalCodecU16>(64));     // w64
  // codecs.push_back(std::make_unique<SimdCompFusedForGlobalCodecU16>(32));     // w32
  // codecs.push_back(std::make_unique<SimdCompFusedForLocalCodecU16>());
  // codecs.push_back(std::make_unique<SimdCompFusedForHierarchicalCodecU16>());
  // codecs.push_back(std::make_unique<FastPForFusedCodecU16>());
  // codecs.push_back(std::make_unique<FastPForFusedCorrectedCodecU16>());            // global_b
  // codecs.push_back(std::make_unique<FastPForFusedCorrectedCodecU16>(false));       // adaptive_b, w256
  // codecs.push_back(std::make_unique<FastPForFusedCorrectedCodecU16>(false, 128));  // adaptive_b, w128
  // codecs.push_back(std::make_unique<FastPForFusedCorrectedCodecU16>(false, 64));   // adaptive_b, w64
  // codecs.push_back(std::make_unique<FastPForFusedCorrectedDeltaLocalCodecU16>());
  // codecs.push_back(std::make_unique<FastPForFusedCorrectedDeltaCarryCodecU16>());
  // codecs.push_back(std::make_unique<FastPForFusedCorrectedForGlobalCodecU16>());               // global_b
  // codecs.push_back(std::make_unique<FastPForFusedCorrectedForGlobalCodecU16>(false));          // adaptive_b, w256, p16
  // codecs.push_back(std::make_unique<FastPForFusedCorrectedForGlobalCodecU16>(false, 16.0, 128)); // adaptive_b, w128
  // codecs.push_back(std::make_unique<FastPForFusedCorrectedForGlobalCodecU16>(false, 16.0, 64));  // adaptive_b, w64
  // codecs.push_back(std::make_unique<FastPForFusedCorrectedForGlobalCodecU16>(false, 16.0, 32));  // adaptive_b, w32
  // codecs.push_back(std::make_unique<FastPForFusedCorrectedForGlobalCodecU16>(false, 32.0));
  // codecs.push_back(std::make_unique<FastPForFusedCorrectedForGlobalCodecU16>(false, 64.0));
  // codecs.push_back(std::make_unique<FastPForFusedCorrectedForGlobalCodecU16>(false, 128.0));
  // codecs.push_back(std::make_unique<FastPForFusedCorrectedForGlobalCodecU16>(false, 256.0));
  // codecs.push_back(std::make_unique<FastPForFusedCorrectedForGlobalCodecU16>(false, 512.0));
  // codecs.push_back(std::make_unique<FastPForFusedCorrectedForGlobalCodecU16>(false, 1024.0));
  // codecs.push_back(std::make_unique<FastPForFusedCorrectedForGlobalCodecU16>(false, 2048.0));
  // codecs.push_back(std::make_unique<FastPForFusedCorrectedForGlobalCodecU16>(false, 4096.0));
  // codecs.push_back(std::make_unique<FastPForFusedCorrectedForGlobalCodecU16>(false, 8192.0));
  codecs.push_back(std::make_unique<TurboPForCodecU16>(3)); // turbopfor
  codecs.push_back(std::make_unique<TurboPForCodecU16>(7)); // turbopack
  codecs.push_back(std::make_unique<TurboPForFusedCodecU16>()); // fused-sum 128v16
  codecs.push_back(std::make_unique<TurboPForFused256CodecU16>()); // fused-sum 256v16

  // ── 256-bit fused FoR TurboPFor (PFor residuals + per-window anchor), both
  //    aggregate impls — regular (window × {raw,packed-anchor}) + hierarchical ──
  for (FusedAggImpl agg : {FusedAggImpl::kUnpack, FusedAggImpl::kMadd}) {
    for (size_t w : {4u, 8u, 16u, 32u, 64u, 128u, 256u})
      for (bool sep : {false, true})
        codecs.push_back(std::make_unique<TurboPForFusedForCodecU16>(w, sep, agg));
    for (size_t gw : {128u, 256u})
      for (size_t lw : {4u, 8u, 16u, 32u, 64u, 128u, 256u})
        if (lw <= gw && gw % lw == 0)
          codecs.push_back(
              std::make_unique<TurboPForFusedForHierarchicalCodecU16>(gw, lw, agg));
  }

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
  // for (auto& codec : lCodecs)
    // codecs.push_back(
        // std::unique_ptr<StatefulIntegerCodec<uint16_t>>(codec->CloneFresh()));
  // codecs.push_back(std::make_unique<RLECodecU16>());

  // Non-cascaded physical codecs
  for (auto& codec : pCodecs)
    codecs.push_back(
        std::unique_ptr<StatefulIntegerCodec<uint16_t>>(codec->CloneFresh()));

  // Non-cascaded heavy codecs
  // for (auto& codec : hCodecs)
  //   codecs.push_back(
  //       std::unique_ptr<StatefulIntegerCodec<uint16_t>>(codec->CloneFresh()));

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

// Focused codec set for local delta-fusion testing.
// Includes: the 4 new fused-delta codecs, plain simdcomp/PFor (fused),
// and DeltaCodecU16 cascaded with each of simdcomp and PFor for comparison.
// inline std::vector<std::unique_ptr<StatefulIntegerCodec<uint16_t>>>
// BuildAllCodecsU16() {
//   std::vector<std::unique_ptr<StatefulIntegerCodec<uint16_t>>> codecs;

//   // Plain physical (no external delta pre-pass)
//   codecs.push_back(std::make_unique<SimdCompFusedCodecU16>());
//   codecs.push_back(std::make_unique<FastPForFusedCorrectedCodecU16>());       // global_b
//   codecs.push_back(std::make_unique<FastPForFusedCorrectedCodecU16>(false));  // adaptive_b

//   // Fused delta variants
//   codecs.push_back(std::make_unique<SimdCompFusedDeltaLocalCodecU16>());
//   codecs.push_back(std::make_unique<SimdCompFusedDeltaCarryCodecU16>());
//   codecs.push_back(std::make_unique<FastPForFusedCorrectedDeltaLocalCodecU16>());
//   codecs.push_back(std::make_unique<FastPForFusedCorrectedDeltaCarryCodecU16>());

//   // Fused FoR variants (SimdComp)
//   codecs.push_back(std::make_unique<SimdCompFusedForGlobalCodecU16>());        // w256
//   codecs.push_back(std::make_unique<SimdCompFusedForGlobalCodecU16>(128));    // w128
//   codecs.push_back(std::make_unique<SimdCompFusedForGlobalCodecU16>(64));     // w64
//   codecs.push_back(std::make_unique<SimdCompFusedForGlobalCodecU16>(32));     // w32
//   codecs.push_back(std::make_unique<SimdCompFusedForLocalCodecU16>());
//   codecs.push_back(std::make_unique<SimdCompFusedForHierarchicalCodecU16>());

//   // Fused FoR variants (FastPFor)
//   codecs.push_back(std::make_unique<FastPForFusedCorrectedForGlobalCodecU16>());               // global_b
//   codecs.push_back(std::make_unique<FastPForFusedCorrectedForGlobalCodecU16>(false));          // adaptive_b, w256, p16
//   codecs.push_back(std::make_unique<FastPForFusedCorrectedForGlobalCodecU16>(false, 16.0, 128)); // adaptive_b, w128
//   codecs.push_back(std::make_unique<FastPForFusedCorrectedForGlobalCodecU16>(false, 16.0, 64));  // adaptive_b, w64
//   codecs.push_back(std::make_unique<FastPForFusedCorrectedForGlobalCodecU16>(false, 16.0, 32));  // adaptive_b, w32
//   codecs.push_back(std::make_unique<FastPForFusedCorrectedForGlobalCodecU16>(false, 32.0));
//   codecs.push_back(std::make_unique<FastPForFusedCorrectedForGlobalCodecU16>(false, 64.0));
//   codecs.push_back(std::make_unique<FastPForFusedCorrectedForGlobalCodecU16>(false, 128.0));
//   codecs.push_back(std::make_unique<FastPForFusedCorrectedForGlobalCodecU16>(false, 256.0));
//   codecs.push_back(std::make_unique<FastPForFusedCorrectedForGlobalCodecU16>(false, 512.0));
//   codecs.push_back(std::make_unique<FastPForFusedCorrectedForGlobalCodecU16>(false, 1024.0));
//   codecs.push_back(std::make_unique<FastPForFusedCorrectedForGlobalCodecU16>(false, 2048.0));
//   codecs.push_back(std::make_unique<FastPForFusedCorrectedForGlobalCodecU16>(false, 4096.0));
//   codecs.push_back(std::make_unique<FastPForFusedCorrectedForGlobalCodecU16>(false, 8192.0));

//   // Cascaded: DeltaCodecU16 -> simdcomp
//   codecs.push_back(
//       std::make_unique<CompositeStatefulIntegerCodec<uint16_t>>(
//           std::make_unique<DeltaCodecU16>(),
//           std::make_unique<SimdCompFusedCodecU16>()));

//   // Cascaded: DeltaCodecU16 -> PFor
//   codecs.push_back(
//       std::make_unique<CompositeStatefulIntegerCodec<uint16_t>>(
//           std::make_unique<DeltaCodecU16>(),
//           std::make_unique<FastPForFusedCorrectedCodecU16>()));

//   codecs.push_back(std::make_unique<DirectAccessCodecU16>());

//   codecs.push_back(std::make_unique<TurboPForCodecU16>(7)); // turbopack
//   // Cascaded: DeltaCodecU16 -> turbopack
//   codecs.push_back(
//       std::make_unique<CompositeStatefulIntegerCodec<uint16_t>>(
//           std::make_unique<DeltaCodecU16>(),
//           std::make_unique<TurboPForCodecU16>(7)));


//   return codecs;
// }
