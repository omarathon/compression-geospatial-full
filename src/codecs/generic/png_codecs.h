#pragma once

#include <png.h>

#include <cstdint>
#include <cstring>
#include <csetjmp>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <vector>

#include "generic_codecs.h"

// Lossless PNG codec.
// uint16_t → 16-bit grayscale PNG (1 sample per pixel).
//            png_set_swap() swaps bytes only on little-endian hosts, which is
//            correct for x86/x86-64 targets. Not portable to big-endian.
// int32_t  → 8-bit RGBA PNG (4 bytes per pixel, explicit LE byte extraction,
//            fully portable).
//
// Error handling: we use libpng's setjmp/longjmp mechanism (png_jmpbuf).
// We pass nullptr for error_fn so libpng uses its built-in longjmp handler.
// All C++ objects with non-trivial destructors that appear after setjmp are
// either trivial or pre-allocated before the setjmp point.

namespace png_codec_detail {

// Suppress libpng warnings; errors use the built-in longjmp handler.
static void warnFn(png_structp, png_const_charp) {}

struct WriteState {
  std::vector<uint8_t>* buf;
};
static void writeFn(png_structp png, png_bytep data, png_size_t len) {
  auto* s = static_cast<WriteState*>(png_get_io_ptr(png));
  s->buf->insert(s->buf->end(), data, data + len);
}
static void flushFn(png_structp) {}

struct ReadState {
  const uint8_t* data;
  size_t size;
  size_t pos;
};
// readFn calls png_error (→ longjmp) on out-of-bounds access.
static void readFn(png_structp png, png_bytep data, png_size_t len) {
  auto* s = static_cast<ReadState*>(png_get_io_ptr(png));
  if (s->pos + len > s->size)
    png_error(png, "PNG: read past end of buffer");
  std::memcpy(data, s->data + s->pos, len);
  s->pos += len;
}

}  // namespace png_codec_detail

template <typename T>
class PNGCodec : public StatefulIntegerCodec<T> {
 public:
  std::vector<uint8_t> compressed;

  void AllocEncoded(const T*, size_t) override { compressed.clear(); }

  void EncodeArray(const T* in, const size_t length) override {
    compressed.clear();

    // Pre-build row buffer for int32 BEFORE setjmp so its destructor is
    // called on normal and longjmp-triggered paths.
    std::vector<uint8_t> row;
    if constexpr (!std::is_same_v<T, uint16_t>) {
      row.resize(length * 4);
      for (size_t i = 0; i < length; i++) {
        uint32_t v = static_cast<uint32_t>(in[i]);
        row[i * 4 + 0] = static_cast<uint8_t>(v & 0xFF);
        row[i * 4 + 1] = static_cast<uint8_t>((v >> 8) & 0xFF);
        row[i * 4 + 2] = static_cast<uint8_t>((v >> 16) & 0xFF);
        row[i * 4 + 3] = static_cast<uint8_t>((v >> 24) & 0xFF);
      }
    }

    png_structp png = png_create_write_struct(PNG_LIBPNG_VER_STRING, nullptr,
                                              nullptr, png_codec_detail::warnFn);
    if (!png) throw std::runtime_error("PNG: write struct alloc failed");
    png_infop info = png_create_info_struct(png);
    if (!info) {
      png_destroy_write_struct(&png, nullptr);
      throw std::runtime_error("PNG: info struct alloc failed");
    }

    // All libpng errors longjmp here; we clean up and re-throw.
    if (setjmp(png_jmpbuf(png))) {
      png_destroy_write_struct(&png, &info);
      throw std::runtime_error("PNG: encode failed");
    }

    // Only trivial locals (plain structs, raw pointers) below this point.
    png_codec_detail::WriteState ws{&compressed};
    png_set_write_fn(png, &ws, png_codec_detail::writeFn, png_codec_detail::flushFn);

    if constexpr (std::is_same_v<T, uint16_t>) {
      png_set_IHDR(png, info, static_cast<png_uint_32>(length), 1,
                   16, PNG_COLOR_TYPE_GRAY, PNG_INTERLACE_NONE,
                   PNG_COMPRESSION_TYPE_BASE, PNG_FILTER_TYPE_BASE);
      png_write_info(png, info);
      png_set_swap(png);  // host (LE) → big-endian
      png_write_row(png, const_cast<png_bytep>(reinterpret_cast<const uint8_t*>(in)));
    } else {
      png_set_IHDR(png, info, static_cast<png_uint_32>(length), 1,
                   8, PNG_COLOR_TYPE_RGBA, PNG_INTERLACE_NONE,
                   PNG_COMPRESSION_TYPE_BASE, PNG_FILTER_TYPE_BASE);
      png_write_info(png, info);
      png_write_row(png, row.data());
    }
    png_write_end(png, info);
    png_destroy_write_struct(&png, &info);
  }

  void DecodeArray(T* out, const size_t length) override {
    png_codec_detail::ReadState rs{compressed.data(), compressed.size(), 0};

    // Pre-allocate row buffer for int32 BEFORE setjmp.
    std::vector<uint8_t> row;
    if constexpr (!std::is_same_v<T, uint16_t>) {
      row.resize(length * 4);
    }

    png_structp png = png_create_read_struct(PNG_LIBPNG_VER_STRING, nullptr,
                                             nullptr, png_codec_detail::warnFn);
    if (!png) throw std::runtime_error("PNG: read struct alloc failed");
    png_infop info = png_create_info_struct(png);
    if (!info) {
      png_destroy_read_struct(&png, nullptr, nullptr);
      throw std::runtime_error("PNG: info struct alloc failed");
    }

    // All libpng errors (including png_error() calls from readFn and IHDR
    // validation below) longjmp here.
    if (setjmp(png_jmpbuf(png))) {
      png_destroy_read_struct(&png, &info, nullptr);
      throw std::runtime_error("PNG: decode failed");
    }

    png_set_read_fn(png, &rs, png_codec_detail::readFn);
    png_read_info(png, info);

    // Validate IHDR — use png_error so failures route through the longjmp.
    png_uint_32 w         = png_get_image_width(png, info);
    png_uint_32 h         = png_get_image_height(png, info);
    int         bit_depth = png_get_bit_depth(png, info);
    int         color_type= png_get_color_type(png, info);

    if (w != static_cast<png_uint_32>(length) || h != 1)
      png_error(png, "PNG: IHDR dimensions mismatch");

    if constexpr (std::is_same_v<T, uint16_t>) {
      if (bit_depth != 16 || color_type != PNG_COLOR_TYPE_GRAY)
        png_error(png, "PNG: IHDR format mismatch for uint16");
      png_set_swap(png);  // big-endian → host (LE)
      png_read_row(png, reinterpret_cast<png_bytep>(out), nullptr);
    } else {
      if (bit_depth != 8 || color_type != PNG_COLOR_TYPE_RGBA)
        png_error(png, "PNG: IHDR format mismatch for int32");
      png_read_row(png, row.data(), nullptr);
      for (size_t i = 0; i < length; i++) {
        out[i] = static_cast<int32_t>(
            static_cast<uint32_t>(row[i * 4 + 0]) |
            (static_cast<uint32_t>(row[i * 4 + 1]) << 8) |
            (static_cast<uint32_t>(row[i * 4 + 2]) << 16) |
            (static_cast<uint32_t>(row[i * 4 + 3]) << 24));
      }
    }
    png_destroy_read_struct(&png, &info, nullptr);
  }

  std::size_t EncodedNumValues() override { return compressed.size(); }
  std::size_t EncodedSizeValue() override { return sizeof(uint8_t); }
  std::string name() const override { return "Heavy_PNG"; }
  std::size_t GetOverflowSize(size_t) const override { return 0; }
  StatefulIntegerCodec<T>* CloneFresh() const override { return new PNGCodec<T>(); }

  void clear() override { compressed.clear(); compressed.shrink_to_fit(); }
  std::vector<T>& GetEncoded() override {
    throw std::runtime_error("PNG: GetEncoded not supported");
  }
};
