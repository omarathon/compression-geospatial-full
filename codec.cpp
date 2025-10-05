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
// #include "codecs/custom_unvec_logic_codecs.h"
#include "codecs/custom_vec_logic_codecs_sse.h"
// #include "codecs/maskedvbyte_codecs.h"
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

#include "constants.h"

#include <optional>

namespace py = pybind11;

template <typename T> // T: type of data being stored
struct RawMemcpyBlockSequence {
    std::vector<std::unique_ptr<T[]>> _blocks;

    size_t _size_bytes;
    int _sw;
    int _sh;

    RawMemcpyBlockSequence(int sw, int sh) : _size_bytes{0}, _sw{sw}, _sh{sh}
    {}

    bool HasBlock(int id) {
        return id >= 0 && id < _blocks.size();
    }

    void WriteBlocks(py::array_t<T> arr, int bw, int bh) {
        int tiles_x = (bw + _sw - 1) / _sw;
        int tiles_y = (bh + _sh - 1) / _sh;
        int numTiles = tiles_x * tiles_y;
        _blocks.reserve(_blocks.size() + numTiles);

        py::buffer_info info = arr.request();
        if (info.ndim != 2)
            throw std::runtime_error("Input array must be 2D");

        auto *ptr = static_cast<T*>(info.ptr);
        int stride_x = info.strides[1] / sizeof(T); // 1
        int stride_y = info.strides[0] / sizeof(T); // bw

        assert(stride_x == 1 && stride_y == bw);

        for (int ty = 0; ty < tiles_y; ++ty) {
            for (int tx = 0; tx < tiles_x; ++tx) {
                int x0 = tx * _sw;
                int y0 = ty * _sh;
                int x1 = std::min(x0 + _sw, bw);
                int y1 = std::min(y0 + _sh, bh);
                int w = x1 - x0;
                int h = y1 - y0;

                std::unique_ptr<T[]> block(new T[w * h]);

                for (int yy = 0; yy < h; ++yy) {
                    const T* src_row = ptr + (y0 + yy) * stride_y + x0 * stride_x;
                    T* dst_row = block.get() + yy * w;
                    std::memcpy(dst_row, src_row, w * sizeof(T));
                }

                _size_bytes += w * h * sizeof(T);
                _blocks.push_back(std::move(block));
            }
        }
    }

    py::array_t<T> ReadBlock(int id, int sw, int sh) {
        if (!HasBlock(id))
            throw std::out_of_range("Block out of range");

        T* block = _blocks[id].get();

        py::capsule owner(block, [](void *) {});

        return py::array_t<T>(
            {sh, sw},
            {sizeof(T) * sw, sizeof(T)},
            block,
            owner    
        );
    }

    size_t SizeBytes() {
        return _size_bytes;
    }
};

template <typename T> // T: type of data being stored
struct RawNoCopyBlockSequence {
    std::vector<py::array_t<T>> _backing_arrays;

    size_t _size_bytes;
    int _sw;
    int _sh;

    std::optional<int> _numTilesInStrip;

    RawNoCopyBlockSequence(int sw, int sh) : _size_bytes{0}, _sw{sw}, _sh{sh}, _numTilesInStrip{std::nullopt}
    {}

    std::optional<int> StripIdFor(int globalTileId) {
        if (!_numTilesInStrip) return std::nullopt;
        return globalTileId / *_numTilesInStrip;
    }

    std::optional<int> TileIdWithinStripFor(int globalTileId) {
        if (!_numTilesInStrip) return std::nullopt;
        return globalTileId % *_numTilesInStrip;
    }

    bool HasBlock(int id) {
        if (id < 0) return false;
        auto stripId = StripIdFor(id);
        return stripId && (*stripId < _backing_arrays.size());
    }

    void WriteBlocks(py::array_t<T> arr, int bw, int bh) {
        _backing_arrays.push_back(arr);
        _size_bytes += bw * bh * sizeof(T); 
        if (!_numTilesInStrip) {
            int tiles_x = (bw + _sw - 1) / _sw;
            int tiles_y = (bh + _sh - 1) / _sh;
            _numTilesInStrip = tiles_x * tiles_y;
        }
    }

    py::array_t<T> ReadBlock(int id, int sw, int sh) {
        if (!HasBlock(id))
            throw std::out_of_range("Block out of range");

        auto stripId = StripIdFor(id);
        auto tileIdWithinStrip = TileIdWithinStripFor(id);

        assert(stripId && tileIdWithinStrip);

        auto& arr = _backing_arrays[*stripId];

        py::buffer_info info = arr.request();
        if (info.ndim != 2)
            throw std::runtime_error("Input array must be 2D");

        auto *ptr = static_cast<T*>(info.ptr);
        int bh = static_cast<int>(info.shape[0]);  // number of rows
        int bw = static_cast<int>(info.shape[1]);  // number of columns

        if (bw == sw && bh == sh) {
            // No need to extract a tile. Return the whole stripe.
            assert(*tileIdWithinStrip == 0);
            return arr;
        }

        // return a view of the tile.
        int tilesPerRow = (bw + _sw - 1) / _sw;
        int rowId = *tileIdWithinStrip / tilesPerRow;
        int colId = *tileIdWithinStrip % tilesPerRow;

        int x0 = colId * _sw;
        int y0 = rowId * _sh;

        // Pointer to top-left of tile
        T* tile_ptr = ptr + y0 * bw + x0;

        // Construct a NumPy view (no copy)
        return py::array_t<T>(
            {sh, sw},                   // shape of subarray
            {sizeof(T) * bw, sizeof(T)},// strides: row = full array width * sizeof(T), col = sizeof(T)
            tile_ptr,                   // data pointer
            arr                         // keep base alive
        );
    }

    size_t SizeBytes() {
        return _size_bytes;
    }
};


// not benefit from morton: 0, 1

// 0: simdcomp
// 1: fastpfor_simdbinarypacking

// benefit from morton: 2-15, 100-103, 200, 201

// 2: fastpfor_simdpfor
// 3: rle
// 4: rle + simdcomp
// 5: rle + fastpfor_bitpack
// 6: TurboPFor256
// 7: TurboPFor_Delta+TurboPFor256
// 8: TurboPFor_Zigzag+TurboPFor256
// 9: TurboPFor_Zigzag+TurboPack256
// 10: TurboPFor_xor+TurboPack256
// 11: TurboPFor_zzag/delta+TurboPFor128
// 12: TurboPFor_Zigzag+TurboVSimple
// 13: TurboPFor_Zigzag/delta_bitio
// 14: TurboPFor_Zigzag_bitio

// 100-103: bads (entropy)

// 201: delta(zigzag)+fastpfor_bitpack
// 202: delta(zigzag)+simdcomp

std::unique_ptr<StatefulIntegerCodec<int32_t>> make_int32_codec(uint8_t kind) {
    switch (kind) {
        // case 0: return std::make_unique<SimdCompCodec>();
        case 0: return std::make_unique<SimdCompScratchCodec>();
        case 1: return std::make_unique<FastPForCodec>(
                    CODECFactory{}.getFromName("simdbinarypacking"));                               // <---- good for habitat
        case 2: return std::make_unique<FastPForCodec>( // benefits from morton
                    CODECFactory{}.getFromName("simdpfor"));
        case 3: return std::make_unique<RLECodecSSE42>();                                           // <---- very good for range map
        case 4: { // benefits from morton
            auto rle = std::make_unique<RLECodecSSE42>();
            auto simdcomp = std::make_unique<SimdCompCodec>();
            return std::make_unique<CompositeStatefulIntegerCodec<int32_t>>(
                std::move(rle), std::move(simdcomp));
        }
        case 5: {
            auto rle = std::make_unique<RLECodecSSE42>();
            auto binpack = std::make_unique<FastPForCodec>(
                CODECFactory{}.getFromName("simdbinarypacking"));
            return std::make_unique<FastCompositeStatefulIntegerCodec<int32_t>>(
                std::move(rle), std::move(binpack));
        }
        // exotic...
        case 6: return std::make_unique<TurboPForCodec>(3); // TurboPFor256
        case 7: return std::make_unique<TurboPForCodec>(4); // TurboPFor_Delta+TurboPFor256
        case 8: return std::make_unique<TurboPForCodec>(6); // TurboPFor_Zigzag+TurboPFor256
        case 9: return std::make_unique<TurboPForCodec>(10); // TurboPFor_Zigzag+TurboPack256           <---- good for habitat & elevation
        case 10: return std::make_unique<TurboPForCodec>(12); // TurboPFor_xor+TurboPack256             <---- good for habitat & el
        case 11: return std::make_unique<TurboPForCodec>(13); // TurboPFor_zzag/delta+TurboPFor128
        case 12: return std::make_unique<TurboPForCodec>(15); // TurboPFor_Zigzag+TurboVSimple
        case 13: return std::make_unique<TurboPForCodec>(16); // TurboPFor_Zigzag/delta_bitio
        case 14: return std::make_unique<TurboPForCodec>(17); // TurboPFor_Zigzag_bitio
        case 15: return std::make_unique<TurboPForCodec>(18); // TurboPFor_TurboRLE
        case 16: return std::make_unique<TurboPForCodec>(19); // TurboPFor_Xor+TurboRLE
        case 17: return std::make_unique<TurboPForCodec>(20); // TurboPFor_Zigzag+TurboRLE
        
        case 20: return std::make_unique<FastPForCodec>(
                    CODECFactory{}.getFromName("maskedvbyte"));                                            
        case 21: return std::make_unique<StreamVByteCodec>();
        case 22: return std::make_unique<FrameOfReferenceTurboCodec>();                             // <---- decent for elevation map

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
            auto delta = std::make_unique<DeltaCodecSSE42>();
            auto binpack = std::make_unique<FastPForCodec>(
                CODECFactory{}.getFromName("simdbinarypacking"));
            return std::make_unique<CompositeStatefulIntegerCodec<int32_t>>(
                std::move(delta), std::move(binpack));
        }
        case 201: {
            auto delta = std::make_unique<DeltaCodecSSE42>();
            auto simdcomp = std::make_unique<SimdCompCodec>();
            return std::make_unique<CompositeStatefulIntegerCodec<int32_t>>(
                std::move(delta), std::move(simdcomp));
        }
        default: throw std::invalid_argument("Unknown codec kind");
    }
}

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

static std::vector<int32_t> globalScratchCompression(TILE_WIDTH * TILE_HEIGHT * 2);

struct CompressedBlockSequence {
    std::vector<std::unique_ptr<StatefulIntegerCodec<int32_t>>> _compressedBlocks;

    std::vector<int32_t> _scratchDecompression;

    size_t _size_bytes;
    int _sw;
    int _sh;
    uint8_t _codec_kind;
    uint8_t _morton_mode;
    
    CompressedBlockSequence(int sw, int sh, uint8_t kind, uint8_t morton_mode) : _size_bytes{0}, _sw{sw}, _sh{sh}, _codec_kind{kind}, _morton_mode{morton_mode}, 
        _scratchDecompression(TILE_WIDTH * TILE_HEIGHT + make_int32_codec(kind)->getOverflowSize(TILE_WIDTH * TILE_HEIGHT))
    {}

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

        for (int ty = 0; ty < tiles_y; ++ty) {
            for (int tx = 0; tx < tiles_x; ++tx) {
                int x0 = tx * _sw;
                int y0 = ty * _sh;
                int x1 = std::min(x0 + _sw, bw);
                int y1 = std::min(y0 + _sh, bh);
                int w = x1 - x0;
                int h = y1 - y0; 

                auto codec = make_int32_codec(_codec_kind);

                // --- All other codecs (existing logic) ---
                if (h == 1) {
                    int32_t* tile_ptr = ptr + y0 * stride_y + x0;
                    codec->encodeArray(tile_ptr, w * h);
                } else {
                    for (int yy = 0; yy < h; ++yy) {
                        int32_t* src_row = ptr + (y0 + yy) * stride_y + x0 * stride_x;
                        int32_t* dst_row = globalScratchCompression.data() + yy * w;
                        if (stride_x == 1) {
                            std::memcpy(dst_row, src_row, w * sizeof(int32_t));
                        } else {
                            for (int xx = 0; xx < w; ++xx) {
                                dst_row[xx] = src_row[xx * stride_x];
                            }
                        }
                    }
                    if (_morton_mode > 0) {
                        if (w == h && w > 1) {
                            int32_t* mortonBuf = globalScratchCompression.data() + (w * h);
                            if (_morton_mode == 1) {
                                MortonCache& mortonCodes = getMortonCache(w);
                                for (int i = 0; i < w * h; i++) {
                                    mortonBuf[mortonCodes.to_morton[i]] = globalScratchCompression[i];
                                }
                            }
                            else {
                                for (int y = 0; y < h; y++) {
                                    for (int x = 0; x < w; x++) {
                                        size_t idx = y * w + x;
                                        uint32_t m = libmorton::morton2D_32_encode(
                                            static_cast<uint_fast16_t>(x),
                                            static_cast<uint_fast16_t>(y)
                                        );
                                        mortonBuf[m] = globalScratchCompression[idx];
                                    }
                                }
                            }
                            codec->encodeArray(mortonBuf, w * h);
                        }
                        else {
                            codec->encodeArray(globalScratchCompression.data(), w * h);
                        }
                    }
                    else {
                        codec->encodeArray(globalScratchCompression.data(), w * h);
                    }
                }
                _size_bytes += codec->encodedNumValues() * codec->encodedSizeValue();
                _compressedBlocks.push_back(std::move(codec));
            }
        }
    }

    py::array_t<int32_t> ReadBlock(int id, int sw, int sh) {
        if (!HasBlock(id))
            throw std::out_of_range("Block out of range");

        auto* codec = _compressedBlocks[id].get();

        // useScratch1 = !useScratch1; // toggle scratch to allow 2 decoded blocks in use at once

        // auto& globalScratch = useScratch1 ? globalScratch1 : globalScratch2;

        py::capsule owner(_scratchDecompression.data(), [](void *) {});
        // py::capsule owner(this, [](void *) {});

        // Normal path: decode into scratchTile
        size_t n = sw * sh;
        codec->decodeArray(_scratchDecompression.data(), n);

        if (_morton_mode > 0) {
            if (sw == sh && sw > 1) {
                // Undo Morton order
                int32_t* mortonBuf = _scratchDecompression.data() + (sw * sh);
                if (_morton_mode == 1) {
                    MortonCache& mortonCodes = getMortonCache(sw);
                    for (int i = 0; i < sw * sh; i++) {
                        mortonBuf[i] = _scratchDecompression[mortonCodes.from_morton[i]];
                    }
                }
                else {
                    for (int y = 0; y < sh; y++) {
                        for (int x = 0; x < sw; x++) {
                            size_t idx = y * sw + x;
                            uint32_t m = libmorton::morton2D_32_encode(
                                static_cast<uint_fast16_t>(x),
                                static_cast<uint_fast16_t>(y)
                            );
                            mortonBuf[idx] = _scratchDecompression[m];
                        }
                    }
                }
                return py::array_t<int32_t>(
                    {sh, sw},
                    {sizeof(int32_t) * sw, sizeof(int32_t)},
                    mortonBuf,
                    // py::cast(static_cast<CompressedBlockSequence*>(this))
                    owner
                );
            }
        }
        return py::array_t<int32_t>(
            {sh, sw},
            {sizeof(int32_t) * sw, sizeof(int32_t)},
            _scratchDecompression.data(),
            // py::cast(static_cast<CompressedBlockSequence*>(this))
            owner
        );
    }

    size_t SizeBytes() {
        return _size_bytes;
    }
};




// TODO: kill if Memcpy/NoCopy are better.

template <typename T> // T: type of data being compressed
std::unique_ptr<StatefulIntegerCodec<T>> make_arbitrary_codec(uint8_t kind) {
    switch (kind) {
        case 98: {
            return std::make_unique<PointerDirectAccessCodec<T>>();
        }
        case 99: {
            return std::make_unique<DirectAccessCodec<T>>();
        }
        default: throw std::invalid_argument("Unknown codec kind");
    }
}

template <typename T> // T: type of data being stored
struct RawBlockSequence {
    std::vector<std::unique_ptr<StatefulIntegerCodec<T>>> _blocks;
    std::vector<py::array_t<T>> _backing_arrays;
    std::vector<T> _scratchTile;

    size_t _size_bytes;
    int _sw;
    int _sh;
    uint8_t _codec_kind;

    RawBlockSequence(int sw, int sh, uint8_t kind) : _size_bytes{0}, _sw{sw}, _sh{sh}, _codec_kind{kind}
    {
        if (_codec_kind != 98) { // Pointer codec doesn't use the scratch tile
            _scratchTile.resize(sw * sh + make_arbitrary_codec<T>(kind)->getOverflowSize(sw * sh));
        }
    }

    bool HasBlock(int id) {
        return id >= 0 && id < _blocks.size();
    }

    void WriteBlocks(py::array_t<T> arr, int bw, int bh) {
        int tiles_x = (bw + _sw - 1) / _sw;
        int tiles_y = (bh + _sh - 1) / _sh;
        int numTiles = tiles_x * tiles_y;
        _blocks.reserve(_blocks.size() + numTiles);

        py::buffer_info info = arr.request();
        if (info.ndim != 2)
            throw std::runtime_error("Input array must be 2D");

        auto *ptr = static_cast<T*>(info.ptr);
        int stride_x = info.strides[1] / sizeof(T); // 1
        int stride_y = info.strides[0] / sizeof(T); // bw

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

                auto codec = make_arbitrary_codec<T>(_codec_kind);

                if (_codec_kind == 98) {
                    // --- PointerDirectAccessCodec path ---
                    auto* pcodec = dynamic_cast<PointerDirectAccessCodec<T>*>(codec.get());
                    if (!pcodec) throw std::runtime_error("Codec kind 98 must be PointerDirectAccessCodec");

                    if (h == 1) {
                        // contiguous full tile → zero copy
                        T* tile_ptr = ptr + y0 * stride_y + x0;
                        auto buf = std::shared_ptr<T[]>(tile_ptr, [](T*){}); // no-op deleter
                        pcodec->setOwnedBuffer(buf, w * h);
                    } else {
                        // non-contiguous → allocate new buffer, copy once
                        std::shared_ptr<T[]> buf(new T[w * h], std::default_delete<T[]>());
                        for (int yy = 0; yy < h; ++yy) {
                            T* src_row = ptr + (y0 + yy) * stride_y + x0 * stride_x;
                            T* dst_row = buf.get() + yy * w;
                            if (stride_x == 1) {
                                std::memcpy(dst_row, src_row, w * sizeof(T));
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
                        T* tile_ptr = ptr + y0 * stride_y + x0;
                        codec->allocEncoded(tile_ptr, w * h);
                        codec->encodeArray(tile_ptr, w * h);
                    } else {
                        for (int yy = 0; yy < h; ++yy) {
                            T* src_row = ptr + (y0 + yy) * stride_y + x0 * stride_x;
                            T* dst_row = _scratchTile.data() + yy * w;
                            if (stride_x == 1) {
                                std::memcpy(dst_row, src_row, w * sizeof(T));
                            } else {
                                for (int xx = 0; xx < w; ++xx) {
                                    dst_row[xx] = src_row[xx * stride_x];
                                }
                            }
                        }
                        codec->allocEncoded(_scratchTile.data(), w * h);
                        codec->encodeArray(_scratchTile.data(), w * h);
                    }
                }
                _size_bytes += codec->encodedNumValues() * codec->encodedSizeValue();
                _blocks.push_back(std::move(codec));
            }
        }
    }

    py::array_t<T> ReadBlock(int id, int sw, int sh) {
        if (!HasBlock(id))
            throw std::out_of_range("Block out of range");

        auto* codec = _blocks[id].get();

        if (_codec_kind == 99) {
            // Special case: codec already owns the decoded/encoded data
            std::vector<T>& encoded = codec->getEncoded();

            if (encoded.size() < sw * sh) {
                throw std::runtime_error("Encoded block smaller than requested shape");
            }

            return py::array_t<T>(
                {sh, sw},
                {sizeof(T) * sw, sizeof(T)},
                encoded.data()
            );
        } else { // 98
             // PointerDirectAccessCodec
            auto* pcodec = dynamic_cast<PointerDirectAccessCodec<T>*>(codec);
            if (!pcodec || !pcodec->getPointer())
                throw std::runtime_error("PointerDirectAccessCodec has no data");

            if (pcodec->encodedNumValues() < (size_t)(sw * sh))
                throw std::runtime_error("Encoded block smaller than requested shape");

            return py::array_t<T>(
                {sh, sw},
                {sizeof(T) * sw, sizeof(T)},
                pcodec->getPointer(),
                _backing_arrays.back()
            );
        }
    }

    size_t SizeBytes() {
        return _size_bytes;
    }
};



PYBIND11_MODULE(codec, m) {
    m.doc() = "Codec bindings with block sequence support";
    
    // CompressedBlockSequence

    py::class_<CompressedBlockSequence>(m, "CompressedBlockSequence")
        .def(py::init<int,int,uint8_t,uint8_t>(),
             py::arg("sw"),
             py::arg("sh"),
             py::arg("codec_kind"),
             py::arg("morton_mode"))
        .def("write_blocks", &CompressedBlockSequence::WriteBlocks,
             py::arg("array"),
             py::arg("bw"),
             py::arg("bh"),
             "")
        .def("read_block", &CompressedBlockSequence::ReadBlock,
             py::arg("id"),
             py::arg("sw"),
             py::arg("sh"),
             "")
        .def("has_block", &CompressedBlockSequence::HasBlock,
             py::arg("id"),
             "")
        .def("size_bytes", &CompressedBlockSequence::SizeBytes,
             "");

    // RawMemcpyBlockSequence

    py::class_<RawMemcpyBlockSequence<int32_t>>(m, "RawMemcpyBlockSequenceInt32")
        .def(py::init<int,int>(),
             py::arg("sw"),
             py::arg("sh"))
        .def("write_blocks", &RawMemcpyBlockSequence<int32_t>::WriteBlocks)
        .def("read_block", &RawMemcpyBlockSequence<int32_t>::ReadBlock)
        .def("has_block", &RawMemcpyBlockSequence<int32_t>::HasBlock)
        .def("size_bytes", &RawMemcpyBlockSequence<int32_t>::SizeBytes);

    py::class_<RawMemcpyBlockSequence<int16_t>>(m, "RawMemcpyBlockSequenceInt16")
        .def(py::init<int,int>(),
             py::arg("sw"),
             py::arg("sh"))
        .def("write_blocks", &RawMemcpyBlockSequence<int16_t>::WriteBlocks)
        .def("read_block", &RawMemcpyBlockSequence<int16_t>::ReadBlock)
        .def("has_block", &RawMemcpyBlockSequence<int16_t>::HasBlock)
        .def("size_bytes", &RawMemcpyBlockSequence<int16_t>::SizeBytes);

    py::class_<RawMemcpyBlockSequence<uint8_t>>(m, "RawMemcpyBlockSequenceByte")
        .def(py::init<int,int>(),
             py::arg("sw"),
             py::arg("sh"))
        .def("write_blocks", &RawMemcpyBlockSequence<uint8_t>::WriteBlocks)
        .def("read_block", &RawMemcpyBlockSequence<uint8_t>::ReadBlock)
        .def("has_block", &RawMemcpyBlockSequence<uint8_t>::HasBlock)
        .def("size_bytes", &RawMemcpyBlockSequence<uint8_t>::SizeBytes);

    py::class_<RawMemcpyBlockSequence<float>>(m, "RawMemcpyBlockSequenceFloat")
        .def(py::init<int,int>(),
             py::arg("sw"),
             py::arg("sh"))
        .def("write_blocks", &RawMemcpyBlockSequence<float>::WriteBlocks)
        .def("read_block", &RawMemcpyBlockSequence<float>::ReadBlock)
        .def("has_block", &RawMemcpyBlockSequence<float>::HasBlock)
        .def("size_bytes", &RawMemcpyBlockSequence<float>::SizeBytes);

    // RawNoCopyBlockSequence

    py::class_<RawNoCopyBlockSequence<int32_t>>(m, "RawNoCopyBlockSequenceInt32")
        .def(py::init<int,int>(),
             py::arg("sw"),
             py::arg("sh"))
        .def("write_blocks", &RawNoCopyBlockSequence<int32_t>::WriteBlocks)
        .def("read_block", &RawNoCopyBlockSequence<int32_t>::ReadBlock)
        .def("has_block", &RawNoCopyBlockSequence<int32_t>::HasBlock)
        .def("size_bytes", &RawNoCopyBlockSequence<int32_t>::SizeBytes);

    py::class_<RawNoCopyBlockSequence<int16_t>>(m, "RawNoCopyBlockSequenceInt16")
        .def(py::init<int,int>(),
             py::arg("sw"),
             py::arg("sh"))
        .def("write_blocks", &RawNoCopyBlockSequence<int16_t>::WriteBlocks)
        .def("read_block", &RawNoCopyBlockSequence<int16_t>::ReadBlock)
        .def("has_block", &RawNoCopyBlockSequence<int16_t>::HasBlock)
        .def("size_bytes", &RawNoCopyBlockSequence<int16_t>::SizeBytes);

    py::class_<RawNoCopyBlockSequence<uint8_t>>(m, "RawNoCopyBlockSequenceByte")
        .def(py::init<int,int>(),
             py::arg("sw"),
             py::arg("sh"))
        .def("write_blocks", &RawNoCopyBlockSequence<uint8_t>::WriteBlocks)
        .def("read_block", &RawNoCopyBlockSequence<uint8_t>::ReadBlock)
        .def("has_block", &RawNoCopyBlockSequence<uint8_t>::HasBlock)
        .def("size_bytes", &RawNoCopyBlockSequence<uint8_t>::SizeBytes);

    py::class_<RawNoCopyBlockSequence<float>>(m, "RawNoCopyBlockSequenceFloat")
        .def(py::init<int,int>(),
             py::arg("sw"),
             py::arg("sh"))
        .def("write_blocks", &RawNoCopyBlockSequence<float>::WriteBlocks)
        .def("read_block", &RawNoCopyBlockSequence<float>::ReadBlock)
        .def("has_block", &RawNoCopyBlockSequence<float>::HasBlock)
        .def("size_bytes", &RawNoCopyBlockSequence<float>::SizeBytes);

    // RawBlockSequence - TODO delete

    py::class_<RawBlockSequence<int32_t>>(m, "RawBlockSequenceInt32")
        .def(py::init<int,int,uint8_t>(),
             py::arg("sw"),
             py::arg("sh"),
             py::arg("codec_kind"))
        .def("write_blocks", &RawBlockSequence<int32_t>::WriteBlocks)
        .def("read_block", &RawBlockSequence<int32_t>::ReadBlock)
        .def("has_block", &RawBlockSequence<int32_t>::HasBlock)
        .def("size_bytes", &RawBlockSequence<int32_t>::SizeBytes);

    py::class_<RawBlockSequence<int16_t>>(m, "RawBlockSequenceInt16")
        .def(py::init<int,int,uint8_t>(),
             py::arg("sw"),
             py::arg("sh"),
             py::arg("codec_kind"))
        .def("write_blocks", &RawBlockSequence<int16_t>::WriteBlocks)
        .def("read_block", &RawBlockSequence<int16_t>::ReadBlock)
        .def("has_block", &RawBlockSequence<int16_t>::HasBlock)
        .def("size_bytes", &RawBlockSequence<int16_t>::SizeBytes);

    py::class_<RawBlockSequence<uint8_t>>(m, "RawBlockSequenceByte")
        .def(py::init<int,int,uint8_t>(),
             py::arg("sw"),
             py::arg("sh"),
             py::arg("codec_kind"))
        .def("write_blocks", &RawBlockSequence<uint8_t>::WriteBlocks)
        .def("read_block", &RawBlockSequence<uint8_t>::ReadBlock)
        .def("has_block", &RawBlockSequence<uint8_t>::HasBlock)
        .def("size_bytes", &RawBlockSequence<uint8_t>::SizeBytes);

    py::class_<RawBlockSequence<float>>(m, "RawBlockSequenceFloat")
        .def(py::init<int,int,uint8_t>(),
             py::arg("sw"),
             py::arg("sh"),
             py::arg("codec_kind"))
        .def("write_blocks", &RawBlockSequence<float>::WriteBlocks)
        .def("read_block", &RawBlockSequence<float>::ReadBlock)
        .def("has_block", &RawBlockSequence<float>::HasBlock)
        .def("size_bytes", &RawBlockSequence<float>::SizeBytes);
}