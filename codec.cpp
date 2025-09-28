#include <pybind11/pybind11.h>
#include <pybind11/numpy.h>
#include <pybind11/stl.h>
#include <memory>
#include <stdexcept>
#include <vector>
#include <cstring>

#include "codecs/generic_codecs.h"
#include "codecs/composite_codec.h"
#include "codecs/deflate_codecs.h"
#include "codecs/fastpfor_codecs.h"
#include "codecs/custom_unvec_logic_codecs.h"
#include "codecs/custom_vec_logic_codecs.h"
#include "codecs/maskedvbyte_codecs.h"
#include "codecs/streamvbyte_codecs.h"
#include "codecs/lz4_codecs.h"
#include "codecs/lzma_codecs.h"
#include "codecs/zstd_codecs.h"
#include "codecs/turbopfor_codecs.h"
#include "codecs/frameofreference_codecs.h"
#include "codecs/simdcomp_codecs.h"
#include "codecs/direct_codec.h"

#include <iostream>

#include <unordered_map>
#include "morton.h"

namespace py = pybind11;

struct MortonCache {
    int N;
    std::vector<size_t> to_morton;
    std::vector<size_t> from_morton;

    explicit MortonCache(int n) : N(n), to_morton(n*n), from_morton(n*n) {
        for (int y = 0; y < N; ++y) {
            for (int x = 0; x < N; ++x) {
                size_t idx = y * N + x;
                size_t m = libmorton::morton2D_32_encode(
                    static_cast<uint_fast16_t>(x),
                    static_cast<uint_fast16_t>(y)
                );
                to_morton[idx] = m;
                from_morton[m] = idx;
            }
        }
    }
};

static std::unordered_map<int, MortonCache> morton_caches;

MortonCache& getMortonCache(int N) {
    auto it = morton_caches.find(N);
    if (it == morton_caches.end()) {
        it = morton_caches.emplace(N, MortonCache(N)).first;
    }
    return it->second;
}

constexpr bool DO_MORTON = false; 

// ================================================================
// Factory for creating codec instances by kind
// ================================================================

// benefit from morton: 2, 3, 4, 6, 100-103, 7, 8, 10, 11, 12, 200-202
// 2: fastpfor_simdpfor
// 4: rle
// 4: rle + simdcomp
// 6: rle + fastpfor_bitpack
// 100-103: bad
// 7: turbopfor256
// 8: delta+turbopfor256
// 10: delta+turbopack256
// 11: turborle
// 12: turbopfor
// 200: delta
// 201: delta+fastpfor_bitpack
// 202: delta+simdcomp

std::unique_ptr<StatefulIntegerCodec<int32_t>> make_codec(uint8_t kind) {
    switch (kind) {
        case 0: return std::make_unique<SimdCompCodec>();
        case 1: return std::make_unique<FastPForCodec>(
                    CODECFactory{}.getFromName("simdbinarypacking"));
        case 2: return std::make_unique<FastPForCodec>( // benefits from morton
                    CODECFactory{}.getFromName("simdpfor"));
        case 3: return std::make_unique<RLECodecAVX512>();
        case 4: { // benefits from morton
            auto rle = std::make_unique<RLECodecAVX512>();
            auto simdcomp = std::make_unique<SimdCompCodec>();
            return std::make_unique<CompositeStatefulIntegerCodec<int32_t>>(
                std::move(rle), std::move(simdcomp));
        }
        case 5: return std::make_unique<TurboPForCodec>(12);
        case 6: {
            auto rle = std::make_unique<RLECodecAVX512>();
            auto binpack = std::make_unique<FastPForCodec>(
                CODECFactory{}.getFromName("simdbinarypacking"));
            return std::make_unique<CompositeStatefulIntegerCodec<int32_t>>(
                std::move(rle), std::move(binpack));
        }
        // exotic...
        case 7: return std::make_unique<TurboPForCodec>(3); // TurboPFor256
        case 8: return std::make_unique<TurboPForCodec>(4); // Delta+TurboPFor256
        case 9: return std::make_unique<TurboPForCodec>(7); // TurboPack256
        case 10: return std::make_unique<TurboPForCodec>(8); // Delta+TurboPack256
        case 11: return std::make_unique<TurboPForCodec>(18); // TurboRLE
        case 12: return std::make_unique<TurboPForCodec>(1); // TurboPFor
        case 98: {
            return std::make_unique<PointerDirectAccessCodec>();
        }
        case 99: {
            return std::make_unique<DirectAccessCodec>();
        }
        case 100: {
            return std::make_unique<LZ4Codec>();
        }
        case 101: {
            return std::make_unique<DeflateCodec>();
        }
        case 102: {
            return std::make_unique<ZstdCodec>(1);
        }
        case 103: {
            return std::make_unique<ZstdCodec>(3);
        }
        case 200: {
            return std::make_unique<DeltaCodecAVX512>();
        }
        case 201: {
            auto delta = std::make_unique<DeltaCodecAVX512>();
            auto binpack = std::make_unique<FastPForCodec>(
                CODECFactory{}.getFromName("simdbinarypacking"));
            return std::make_unique<CompositeStatefulIntegerCodec<int32_t>>(
                std::move(delta), std::move(binpack));
        }
        case 202: {
            auto delta = std::make_unique<DeltaCodecAVX512>();
            auto simdcomp = std::make_unique<SimdCompCodec>();
            return std::make_unique<CompositeStatefulIntegerCodec<int32_t>>(
                std::move(delta), std::move(simdcomp));
        }
        default: throw std::invalid_argument("Unknown codec kind");
    }
}

struct CompressedBlockSequence {
    std::vector<std::unique_ptr<StatefulIntegerCodec<int32_t>>> _compressedBlocks;
    std::vector<int32_t> _scratchTile;
    std::vector<py::array_t<int32_t>> _backing_arrays;
    int _sw;
    int _sh;
    uint8_t _codec_kind;
    
    CompressedBlockSequence(int sw, int sh, uint8_t kind) : _sw{sw}, _sh{sh}, _codec_kind{kind}
    {
        if constexpr (DO_MORTON) {
            _scratchTile.resize(2 * sw * sh + make_codec(kind)->getOverflowSize(sw * sh));
        }
        else {
            _scratchTile.resize(sw * sh + make_codec(kind)->getOverflowSize(sw * sh));
        }
    }

    bool HasBlock(int id) {
        return id >= 0 && id < _compressedBlocks.size();
    }

    void WriteBlocks(py::array_t<int32_t> arr, int bw, int bh) {
        int tiles_x = (bw + _sw - 1) / _sw;
        int tiles_y = (bh + _sh - 1) / _sh;
        int numTiles = tiles_x * tiles_y;
        _compressedBlocks.reserve(_compressedBlocks.size() + numTiles);

        py::buffer_info info = arr.request();
        if (info.ndim != 2)
            throw std::runtime_error("Input array must be 2D");

        auto *ptr = static_cast<int32_t*>(info.ptr);
        int stride_x = info.strides[1] / sizeof(int32_t); // 1
        int stride_y = info.strides[0] / sizeof(int32_t); // bw

        // keep Python array alive if PointerDirectAccessCodec is used
        if (_codec_kind == 98) {
            _backing_arrays.push_back(arr);
        }

        for (int ty = 0; ty < tiles_y; ++ty) {
            for (int tx = 0; tx < tiles_x; ++tx) {
                int x0 = tx * _sw;
                int y0 = ty * _sh;
                int x1 = std::min(x0 + _sw, bw);
                int y1 = std::min(y0 + _sh, bh);
                int w = x1 - x0;
                int h = y1 - y0;

                auto codec = make_codec(_codec_kind);

                if (_codec_kind == 98) {
                    // --- PointerDirectAccessCodec path ---
                    auto* pcodec = dynamic_cast<PointerDirectAccessCodec*>(codec.get());
                    if (!pcodec) throw std::runtime_error("Codec kind 98 must be PointerDirectAccessCodec");

                    if (stride_x == 1 && stride_y == bw && w == _sw && h == _sh) {
                        // contiguous full tile → zero copy
                        int32_t* tile_ptr = ptr + y0 * stride_y + x0;
                        auto buf = std::shared_ptr<int32_t[]>(tile_ptr, [](int32_t*){}); // no-op deleter
                        pcodec->setOwnedBuffer(buf, w * h);
                    } else {
                        // non-contiguous → allocate new buffer, copy once
                        std::shared_ptr<int32_t[]> buf(new int32_t[w * h], std::default_delete<int32_t[]>());
                        for (int yy = 0; yy < h; ++yy) {
                            int32_t* src_row = ptr + (y0 + yy) * stride_y + x0 * stride_x;
                            int32_t* dst_row = buf.get() + yy * w;
                            if (stride_x == 1) {
                                std::memcpy(dst_row, src_row, w * sizeof(int32_t));
                            } else {
                                for (int xx = 0; xx < w; ++xx) {
                                    dst_row[xx] = src_row[xx * stride_x];
                                }
                            }
                        }
                        pcodec->setOwnedBuffer(std::move(buf), w * h);
                    }
                } else {
                    // --- All other codecs (existing logic) ---
                    if (h == 1) {
                        int32_t* tile_ptr = ptr + y0 * stride_y + x0;
                        codec->allocEncoded(tile_ptr, w * h);
                        codec->encodeArray(tile_ptr, w * h);
                    } else {
                        for (int yy = 0; yy < h; ++yy) {
                            int32_t* src_row = ptr + (y0 + yy) * stride_y + x0 * stride_x;
                            int32_t* dst_row = _scratchTile.data() + yy * w;
                            if (stride_x == 1) {
                                std::memcpy(dst_row, src_row, w * sizeof(int32_t));
                            } else {
                                for (int xx = 0; xx < w; ++xx) {
                                    dst_row[xx] = src_row[xx * stride_x];
                                }
                            }
                        }
                        if constexpr (DO_MORTON) {
                            if (w == h && _codec_kind != 99) {
                                // Square tile => reorder row-major scratchTile into Morton layout
                                MortonCache& mortonCodes = getMortonCache(w);
                                int32_t* mortonBuf = _scratchTile.data() + (w * h);
                                for (int i = 0; i < w * h; i++) {
                                    mortonBuf[mortonCodes.to_morton[i]] = _scratchTile[i];
                                }
                                codec->allocEncoded(mortonBuf, w * h);
                                codec->encodeArray(mortonBuf, w * h);
                            }
                            else {
                                codec->allocEncoded(_scratchTile.data(), w * h);
                                codec->encodeArray(_scratchTile.data(), w * h);
                            }
                        }
                        else {
                            codec->allocEncoded(_scratchTile.data(), w * h);
                            codec->encodeArray(_scratchTile.data(), w * h);
                        }
                    }
                }

                _compressedBlocks.push_back(std::move(codec));
            }
        }
    }

    py::array_t<int32_t> ReadBlock(int id, int sw, int sh) {
        if (!HasBlock(id))
            throw std::out_of_range("Block out of range");

        auto* codec = _compressedBlocks[id].get();

        if (_codec_kind == 99) {
            // Special case: codec already owns the decoded/encoded data
            std::vector<int32_t>& encoded = codec->getEncoded();

            if (encoded.size() < sw * sh) {
                throw std::runtime_error("Encoded block smaller than requested shape");
            }

            return py::array_t<int32_t>(
                {sh, sw},
                {sizeof(int32_t) * sw, sizeof(int32_t)},
                encoded.data()
            );
        } else if (_codec_kind == 98) {
             // PointerDirectAccessCodec
            auto* pcodec = dynamic_cast<PointerDirectAccessCodec*>(codec);
            if (!pcodec || !pcodec->getPointer())
                throw std::runtime_error("PointerDirectAccessCodec has no data");

            if (pcodec->encodedNumValues() < (size_t)(sw * sh))
                throw std::runtime_error("Encoded block smaller than requested shape");

            return py::array_t<int32_t>(
                {sh, sw},
                {sizeof(int32_t) * sw, sizeof(int32_t)},
                pcodec->getPointer(),
                _backing_arrays.back()
            );
        } else {
            // Normal path: decode into scratchTile
            size_t n = sw * sh;
            codec->decodeArray(_scratchTile.data(), n);

            if constexpr (DO_MORTON) {
                if (sw == sh && sw > 1) {
                    // Undo Morton order
                    MortonCache& mortonCodes = getMortonCache(sw);
                    int32_t* mortonBuf = _scratchTile.data() + (sw * sh);
                    for (int i = 0; i < sw * sh; i++) {
                        mortonBuf[i] = _scratchTile[mortonCodes.from_morton[i]];
                    }
                    return py::array_t<int32_t>(
                        {sh, sw},
                        {sizeof(int32_t) * sw, sizeof(int32_t)},
                        mortonBuf
                    );
                }
            }
            
            return py::array_t<int32_t>(
                {sh, sw},
                {sizeof(int32_t) * sw, sizeof(int32_t)},
                _scratchTile.data()
            );
        }
    }
};

PYBIND11_MODULE(codec, m) {
    m.doc() = "Codec bindings with block sequence support";

    py::class_<CompressedBlockSequence>(m, "CompressedBlockSequence")
        .def(py::init<int,int,uint8_t>(),
             py::arg("sw"),
             py::arg("sh"),
             py::arg("codec_kind"))
        .def("write_blocks", &CompressedBlockSequence::WriteBlocks,
             py::arg("array"),
             py::arg("bw"),
             py::arg("bh"),
             "Partition a 2D int32 numpy array into tiles and compress each block.")
        .def("read_block", &CompressedBlockSequence::ReadBlock,
             py::arg("id"),
             py::arg("sw"),
             py::arg("sh"),
             "Decode (or reference) a block by id into a numpy array view.")
        .def("has_block", &CompressedBlockSequence::HasBlock,
             py::arg("id"),
             "Check if a block exists at the given id.");
}