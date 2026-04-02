#pragma once

#include <openjpeg.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <vector>

#include "generic_codecs.h"

// Lossless JPEG 2000 codec via OpenJPEG.
// uint16_t → 1 component, prec=16, unsigned.
// int32_t  → 2 components of uint16 (low/high 16 bits).
// A 2D image layout (w×h) is used so DWT has multiple levels to exploit.

namespace opj_codec_detail {

// Find w, h such that w*h == length and both <= 16383 (OPJ supports large dims,
// but 2D layout is needed so DWT can decompose more than 1 level).
// For perfect-square lengths returns (sqrt, sqrt).
inline std::pair<int, int> dims(size_t length) {
  if (length == 0) return {1, 1};  // degenerate; callers must guard length==0
  int sq = static_cast<int>(std::round(std::sqrt(static_cast<double>(length))));
  if (sq > 0 && static_cast<size_t>(sq) * sq == length && sq <= 16383)
    return {sq, sq};
  for (int w = std::min(static_cast<int>(length), 16383); w >= 1; w--) {
    if (length % w == 0) {
      int h = static_cast<int>(length / w);
      if (h <= 16383) return {w, h};
    }
  }
  // Fallback: 1 row (DWT disabled via numresolution=1)
  return {static_cast<int>(std::min(length, (size_t)16383)),
          static_cast<int>((length + 16382) / 16383)};
}

// ── Write stream ─────────────────────────────────────────────────────────────

struct WriteStream {
  std::vector<uint8_t>& buf;
  size_t pos = 0;
};

static OPJ_SIZE_T writeFn(void* src, OPJ_SIZE_T nb, void* data) {
  auto* s = static_cast<WriteStream*>(data);
  size_t end = s->pos + nb;
  if (end > s->buf.size()) s->buf.resize(end);
  std::memcpy(s->buf.data() + s->pos, src, nb);
  s->pos += nb;
  return nb;
}
static OPJ_OFF_T skipWriteFn(OPJ_OFF_T nb, void* data) {
  auto* s = static_cast<WriteStream*>(data);
  s->pos += static_cast<size_t>(nb);
  if (s->pos > s->buf.size()) s->buf.resize(s->pos);
  return nb;
}
static OPJ_BOOL seekWriteFn(OPJ_OFF_T nb, void* data) {
  static_cast<WriteStream*>(data)->pos = static_cast<size_t>(nb);
  return OPJ_TRUE;
}

// ── Read stream ──────────────────────────────────────────────────────────────

struct ReadStream {
  const uint8_t* data;
  size_t size;
  size_t pos = 0;
};

static OPJ_SIZE_T readFn(void* dst, OPJ_SIZE_T nb, void* data) {
  auto* s = static_cast<ReadStream*>(data);
  size_t avail = s->size - s->pos;
  size_t n = std::min(avail, static_cast<size_t>(nb));
  if (n == 0) return static_cast<OPJ_SIZE_T>(-1);
  std::memcpy(dst, s->data + s->pos, n);
  s->pos += n;
  return n;
}
static OPJ_OFF_T skipReadFn(OPJ_OFF_T nb, void* data) {
  auto* s = static_cast<ReadStream*>(data);
  OPJ_OFF_T avail = static_cast<OPJ_OFF_T>(s->size - s->pos);
  OPJ_OFF_T n = std::min(avail, nb);
  s->pos += static_cast<size_t>(n);
  return n > 0 ? n : static_cast<OPJ_OFF_T>(-1);
}
static OPJ_BOOL seekReadFn(OPJ_OFF_T nb, void* data) {
  auto* s = static_cast<ReadStream*>(data);
  if (static_cast<size_t>(nb) > s->size) return OPJ_FALSE;
  s->pos = static_cast<size_t>(nb);
  return OPJ_TRUE;
}

static void silentMsg(const char*, void*) {}

}  // namespace opj_codec_detail

template <typename T>
class OpenJPEGCodec : public StatefulIntegerCodec<T> {
  static constexpr int kNumComps = std::is_same_v<T, uint16_t> ? 1 : 2;

 public:
  std::vector<uint8_t> compressed;

  void AllocEncoded(const T*, size_t) override { compressed.clear(); }

  void EncodeArray(const T* in, const size_t length) override {
    if (length == 0) throw std::runtime_error("OpenJPEG: zero-length input");
    compressed.clear();
    auto [w, h] = opj_codec_detail::dims(length);

    opj_image_cmptparm_t cmptparms[2] = {};
    for (int c = 0; c < kNumComps; c++) {
      cmptparms[c].prec = 16;
      cmptparms[c].sgnd = 0;
      cmptparms[c].dx   = 1;
      cmptparms[c].dy   = 1;
      cmptparms[c].w    = static_cast<OPJ_UINT32>(w);
      cmptparms[c].h    = static_cast<OPJ_UINT32>(h);
    }

    OPJ_COLOR_SPACE cs = (kNumComps == 1) ? OPJ_CLRSPC_GRAY : OPJ_CLRSPC_UNSPECIFIED;
    opj_image_t* image = opj_image_create(kNumComps, cmptparms, cs);
    if (!image) throw std::runtime_error("OpenJPEG: image create failed");
    image->x0 = 0; image->y0 = 0;
    image->x1 = static_cast<OPJ_UINT32>(w);
    image->y1 = static_cast<OPJ_UINT32>(h);

    const size_t total = static_cast<size_t>(w) * h;
    if constexpr (std::is_same_v<T, uint16_t>) {
      for (size_t i = 0; i < length; i++)
        image->comps[0].data[i] = static_cast<OPJ_INT32>(in[i]);
      // Zero-fill padding pixels if dims were rounded up (fallback case)
      for (size_t i = length; i < total; i++)
        image->comps[0].data[i] = 0;
    } else {
      for (size_t i = 0; i < length; i++) {
        uint32_t v = static_cast<uint32_t>(in[i]);
        image->comps[0].data[i] = static_cast<OPJ_INT32>(v & 0xFFFFu);
        image->comps[1].data[i] = static_cast<OPJ_INT32>((v >> 16) & 0xFFFFu);
      }
      // Zero-fill padding pixels if dims were rounded up (fallback case)
      for (size_t i = length; i < total; i++) {
        image->comps[0].data[i] = 0;
        image->comps[1].data[i] = 0;
      }
    }

    opj_cparameters_t params;
    opj_set_default_encoder_parameters(&params);
    params.tcp_numlayers  = 1;
    params.cp_disto_alloc = 1;
    params.tcp_rates[0]   = 0;  // lossless

    // Cap resolution levels to what the image dimensions support
    int minDim = std::min(w, h);
    int maxRes = 1;
    while ((1 << maxRes) < minDim) maxRes++;
    params.numresolution = std::min(params.numresolution, maxRes);

    opj_codec_t* codec = opj_create_compress(OPJ_CODEC_J2K);
    if (!codec) {
      opj_image_destroy(image);
      throw std::runtime_error("OpenJPEG: codec create failed");
    }
    opj_set_error_handler(codec, opj_codec_detail::silentMsg, nullptr);
    opj_set_warning_handler(codec, opj_codec_detail::silentMsg, nullptr);
    opj_set_info_handler(codec, opj_codec_detail::silentMsg, nullptr);

    if (!opj_setup_encoder(codec, &params, image)) {
      opj_destroy_codec(codec);
      opj_image_destroy(image);
      throw std::runtime_error("OpenJPEG: encoder setup failed");
    }

    opj_codec_detail::WriteStream ws{compressed};
    opj_stream_t* stream = opj_stream_create(1 << 16, OPJ_FALSE);
    if (!stream) {
      opj_destroy_codec(codec);
      opj_image_destroy(image);
      throw std::runtime_error("OpenJPEG: stream create failed");
    }
    opj_stream_set_write_function(stream, opj_codec_detail::writeFn);
    opj_stream_set_skip_function(stream, opj_codec_detail::skipWriteFn);
    opj_stream_set_seek_function(stream, opj_codec_detail::seekWriteFn);
    opj_stream_set_user_data(stream, &ws, nullptr);

    bool ok = opj_start_compress(codec, image, stream) &&
              opj_encode(codec, stream) &&
              opj_end_compress(codec, stream);

    opj_stream_destroy(stream);
    opj_destroy_codec(codec);
    opj_image_destroy(image);

    if (!ok) throw std::runtime_error("OpenJPEG: encode failed");
    compressed.resize(ws.pos);
  }

  void DecodeArray(T* out, const size_t length) override {
    opj_codec_detail::ReadStream rs{compressed.data(), compressed.size(), 0};

    opj_codec_t* codec = opj_create_decompress(OPJ_CODEC_J2K);
    if (!codec) throw std::runtime_error("OpenJPEG: codec create failed");
    opj_set_error_handler(codec, opj_codec_detail::silentMsg, nullptr);
    opj_set_warning_handler(codec, opj_codec_detail::silentMsg, nullptr);
    opj_set_info_handler(codec, opj_codec_detail::silentMsg, nullptr);

    opj_dparameters_t params;
    opj_set_default_decoder_parameters(&params);
    if (!opj_setup_decoder(codec, &params)) {
      opj_destroy_codec(codec);
      throw std::runtime_error("OpenJPEG: decoder setup failed");
    }

    opj_stream_t* stream = opj_stream_create(1 << 16, OPJ_TRUE);
    if (!stream) {
      opj_destroy_codec(codec);
      throw std::runtime_error("OpenJPEG: stream create failed");
    }
    opj_stream_set_read_function(stream, opj_codec_detail::readFn);
    opj_stream_set_skip_function(stream, opj_codec_detail::skipReadFn);
    opj_stream_set_seek_function(stream, opj_codec_detail::seekReadFn);
    opj_stream_set_user_data(stream, &rs, nullptr);
    opj_stream_set_user_data_length(stream, compressed.size());

    opj_image_t* image = nullptr;
    bool ok = opj_read_header(stream, codec, &image) &&
              opj_decode(codec, stream, image) &&
              opj_end_decompress(codec, stream);

    opj_stream_destroy(stream);
    opj_destroy_codec(codec);

    if (!ok || !image) {
      if (image) opj_image_destroy(image);
      throw std::runtime_error("OpenJPEG: decode failed");
    }

    // Validate decoded image structure before accessing component data.
    auto imgGuard = [&]{ opj_image_destroy(image); };
    if (image->numcomps != static_cast<OPJ_UINT32>(kNumComps)) {
      imgGuard();
      throw std::runtime_error("OpenJPEG: unexpected component count");
    }
    for (int c = 0; c < kNumComps; c++) {
      if (image->comps[c].prec != 16 || image->comps[c].sgnd != 0) {
        imgGuard();
        throw std::runtime_error("OpenJPEG: unexpected component precision/signedness");
      }
      OPJ_UINT32 compPx = image->comps[c].w * image->comps[c].h;
      if (compPx < static_cast<OPJ_UINT32>(length)) {
        imgGuard();
        throw std::runtime_error("OpenJPEG: decoded component smaller than expected");
      }
    }

    if constexpr (std::is_same_v<T, uint16_t>) {
      for (size_t i = 0; i < length; i++)
        out[i] = static_cast<uint16_t>(image->comps[0].data[i]);
    } else {
      for (size_t i = 0; i < length; i++) {
        uint32_t lo = static_cast<uint32_t>(image->comps[0].data[i]) & 0xFFFFu;
        uint32_t hi = static_cast<uint32_t>(image->comps[1].data[i]) & 0xFFFFu;
        out[i] = static_cast<int32_t>(lo | (hi << 16));
      }
    }
    opj_image_destroy(image);
  }

  std::size_t EncodedNumValues() override { return compressed.size(); }
  std::size_t EncodedSizeValue() override { return sizeof(uint8_t); }
  std::string name() const override { return "Heavy_JPEG2000"; }
  std::size_t GetOverflowSize(size_t) const override { return 0; }
  StatefulIntegerCodec<T>* CloneFresh() const override { return new OpenJPEGCodec<T>(); }

  void clear() override { compressed.clear(); compressed.shrink_to_fit(); }
  std::vector<T>& GetEncoded() override {
    throw std::runtime_error("OpenJPEG: GetEncoded not supported");
  }
};
