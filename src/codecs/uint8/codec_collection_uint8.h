#pragma once

#include <memory>
#include <vector>

#include "custom_unvec_logic_codecs_u8.h"
#include "generic_codecs.h"

inline std::vector<std::unique_ptr<StatefulIntegerCodec<uint8_t>>>
BuildAllCodecsU8() {
  std::vector<std::unique_ptr<StatefulIntegerCodec<uint8_t>>> codecs;
  codecs.push_back(std::make_unique<RLECodecU8>());
  return codecs;
}
