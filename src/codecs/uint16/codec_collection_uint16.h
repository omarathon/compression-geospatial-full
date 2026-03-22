#pragma once

#include <memory>
#include <vector>

#include "generic_codecs.h"
#include "direct_codec_uint16.h"
#include "simdcomp_fused_codec_uint16.h"
#include "fastpfor_fused_codec_uint16.h"

inline std::vector<std::unique_ptr<StatefulIntegerCodec<uint16_t>>>
BuildAllCodecsU16() {
  std::vector<std::unique_ptr<StatefulIntegerCodec<uint16_t>>> pool;
  pool.push_back(std::make_unique<DirectAccessCodecU16>());
  pool.push_back(std::make_unique<SimdCompFusedCodecU16>());
  pool.push_back(std::make_unique<FastPForFusedCodecU16>());
  return pool;
}
