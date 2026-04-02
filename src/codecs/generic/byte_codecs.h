#pragma once

#include <lz4.h>
#include <lzma.h>
#include <zlib.h>
#include <zstd.h>

#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

#include "generic_codecs.h"

// ── Deflate ─────────────────────────────────────────────────────────────────

template <typename T>
class DeflateCodec : public StatefulIntegerCodec<T> {
 public:
  std::vector<uint8_t> compressed;

  void AllocEncoded(const T*, size_t length) override {
    compressed.resize(compressBound(length * sizeof(T)));
  }

  void EncodeArray(const T* in, const size_t length) override {
    uLongf outSize = static_cast<uLongf>(compressed.size());
    if (compress2(compressed.data(), &outSize,
                  reinterpret_cast<const Bytef*>(in), length * sizeof(T),
                  Z_BEST_COMPRESSION) != Z_OK)
      throw std::runtime_error("Deflate: compression failed");
    compressed.resize(outSize);
  }

  void DecodeArray(T* out, const size_t length) override {
    uLongf outSize = static_cast<uLongf>(length * sizeof(T));
    if (uncompress(reinterpret_cast<Bytef*>(out), &outSize, compressed.data(),
                   compressed.size()) != Z_OK)
      throw std::runtime_error("Deflate: decompression failed");
    if (outSize != static_cast<uLongf>(length * sizeof(T)))
      throw std::runtime_error("Deflate: decompressed size mismatch");
  }

  std::size_t EncodedNumValues() override { return compressed.size(); }
  std::size_t EncodedSizeValue() override { return sizeof(uint8_t); }
  std::string name() const override { return "Heavy_Deflate"; }
  std::size_t GetOverflowSize(size_t) const override { return 0; }
  StatefulIntegerCodec<T>* CloneFresh() const override { return new DeflateCodec<T>(); }

  void clear() override { compressed.clear(); compressed.shrink_to_fit(); }
  std::vector<T>& GetEncoded() override {
    throw std::runtime_error("Deflate: GetEncoded not supported");
  }
};

// ── LZ4 ─────────────────────────────────────────────────────────────────────

template <typename T>
class LZ4Codec : public StatefulIntegerCodec<T> {
 public:
  std::vector<char> compressed;

  void AllocEncoded(const T*, size_t length) override {
    const size_t inBytes = length * sizeof(T);
    if (inBytes > static_cast<size_t>(std::numeric_limits<int>::max()))
      throw std::runtime_error("LZ4: input too large");
    compressed.resize(LZ4_compressBound(static_cast<int>(inBytes)));
  }

  void EncodeArray(const T* in, const size_t length) override {
    const size_t inBytes = length * sizeof(T);
    int n = LZ4_compress_default(reinterpret_cast<const char*>(in),
                                 compressed.data(),
                                 static_cast<int>(inBytes),
                                 static_cast<int>(compressed.size()));
    if (n <= 0) throw std::runtime_error("LZ4: compression failed");
    compressed.resize(n);
  }

  void DecodeArray(T* out, const size_t length) override {
    const size_t expected = length * sizeof(T);
    if (compressed.size() > static_cast<size_t>(std::numeric_limits<int>::max()) ||
        expected > static_cast<size_t>(std::numeric_limits<int>::max()))
      throw std::runtime_error("LZ4: buffer too large for int API");
    int n = LZ4_decompress_safe(compressed.data(),
                                reinterpret_cast<char*>(out),
                                static_cast<int>(compressed.size()),
                                static_cast<int>(expected));
    if (n < 0) throw std::runtime_error("LZ4: decompression failed");
    if (static_cast<size_t>(n) != expected)
      throw std::runtime_error("LZ4: decompressed size mismatch");
  }

  std::size_t EncodedNumValues() override { return compressed.size(); }
  std::size_t EncodedSizeValue() override { return sizeof(char); }
  std::string name() const override { return "Heavy_LZ4"; }
  std::size_t GetOverflowSize(size_t) const override { return 0; }
  StatefulIntegerCodec<T>* CloneFresh() const override { return new LZ4Codec<T>(); }

  void clear() override { compressed.clear(); compressed.shrink_to_fit(); }
  std::vector<T>& GetEncoded() override {
    throw std::runtime_error("LZ4: GetEncoded not supported");
  }
};

// ── Zstd ─────────────────────────────────────────────────────────────────────

template <typename T>
class ZstdCodec : public StatefulIntegerCodec<T> {
 public:
  int level;
  std::vector<char> compressed;

  explicit ZstdCodec(int level = 3) : level{level} {}

  void AllocEncoded(const T*, size_t length) override {
    compressed.resize(ZSTD_compressBound(length * sizeof(T)));
  }

  void EncodeArray(const T* in, const size_t length) override {
    size_t n = ZSTD_compress(compressed.data(), compressed.size(),
                             in, length * sizeof(T), level);
    if (ZSTD_isError(n))
      throw std::runtime_error(std::string("Zstd: ") + ZSTD_getErrorName(n));
    compressed.resize(n);
  }

  void DecodeArray(T* out, const size_t length) override {
    size_t n = ZSTD_decompress(out, length * sizeof(T),
                               compressed.data(), compressed.size());
    if (ZSTD_isError(n))
      throw std::runtime_error(std::string("Zstd: ") + ZSTD_getErrorName(n));
    if (n != length * sizeof(T))
      throw std::runtime_error("Zstd: decompressed size mismatch");
  }

  std::size_t EncodedNumValues() override { return compressed.size(); }
  std::size_t EncodedSizeValue() override { return sizeof(char); }
  std::string name() const override { return "Heavy_Zstd_" + std::to_string(level); }
  std::size_t GetOverflowSize(size_t) const override { return 0; }
  StatefulIntegerCodec<T>* CloneFresh() const override { return new ZstdCodec<T>(level); }

  void clear() override { compressed.clear(); compressed.shrink_to_fit(); }
  std::vector<T>& GetEncoded() override {
    throw std::runtime_error("Zstd: GetEncoded not supported");
  }
};

// ── LZMA ─────────────────────────────────────────────────────────────────────

template <typename T>
class LZMACodec : public StatefulIntegerCodec<T> {
 public:
  std::vector<uint8_t> compressed;

  void AllocEncoded(const T*, size_t) override {}

  void EncodeArray(const T* in, const size_t length) override {
    lzma_stream strm = LZMA_STREAM_INIT;
    if (lzma_easy_encoder(&strm, LZMA_PRESET_DEFAULT, LZMA_CHECK_CRC64) != LZMA_OK)
      throw std::runtime_error("LZMA: encoder init failed");

    const size_t inBytes = length * sizeof(T);
    // lzma_stream_buffer_bound gives a guaranteed upper bound for the xz-stream
    // format. We still grow dynamically in the loop below just in case.
    size_t outBound = lzma_stream_buffer_bound(inBytes);
    compressed.resize(outBound);

    strm.next_in   = reinterpret_cast<const uint8_t*>(in);
    strm.avail_in  = inBytes;
    strm.next_out  = compressed.data();
    strm.avail_out = outBound;

    lzma_ret ret;
    do {
      ret = lzma_code(&strm, LZMA_FINISH);
      if (ret == LZMA_OK && strm.avail_out == 0) {
        // Output buffer full but stream not done — grow and continue
        size_t written = strm.total_out;
        compressed.resize(compressed.size() * 2);
        strm.next_out  = compressed.data() + written;
        strm.avail_out = compressed.size() - written;
      }
    } while (ret == LZMA_OK);

    if (ret != LZMA_STREAM_END) {
      lzma_end(&strm);
      throw std::runtime_error("LZMA: compression failed");
    }
    compressed.resize(strm.total_out);
    lzma_end(&strm);
  }

  void DecodeArray(T* out, const size_t length) override {
    lzma_stream strm = LZMA_STREAM_INIT;
    if (lzma_stream_decoder(&strm, UINT64_MAX, 0) != LZMA_OK)
      throw std::runtime_error("LZMA: decoder init failed");

    const size_t expected = length * sizeof(T);
    strm.next_in   = compressed.data();
    strm.avail_in  = compressed.size();
    strm.next_out  = reinterpret_cast<uint8_t*>(out);
    strm.avail_out = expected;

    lzma_ret ret;
    do {
      ret = lzma_code(&strm, LZMA_FINISH);
    } while (ret == LZMA_OK);

    if (ret != LZMA_STREAM_END) {
      lzma_end(&strm);
      throw std::runtime_error("LZMA: decompression failed");
    }
    lzma_end(&strm);

    if (strm.total_out != expected)
      throw std::runtime_error("LZMA: decompressed size mismatch");
  }

  std::size_t EncodedNumValues() override { return compressed.size(); }
  std::size_t EncodedSizeValue() override { return sizeof(uint8_t); }
  std::string name() const override { return "Heavy_LZMA"; }
  std::size_t GetOverflowSize(size_t) const override { return 0; }
  StatefulIntegerCodec<T>* CloneFresh() const override { return new LZMACodec<T>(); }

  void clear() override { compressed.clear(); compressed.shrink_to_fit(); }
  std::vector<T>& GetEncoded() override {
    throw std::runtime_error("LZMA: GetEncoded not supported");
  }
};
