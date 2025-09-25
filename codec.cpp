#include <pybind11/pybind11.h>
#include <pybind11/numpy.h>
#include <pybind11/stl.h>
#include <memory>
#include <stdexcept>
#include <vector>

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

namespace py = pybind11;

// ================================================================
// Factory for creating codec instances by kind
// ================================================================
std::unique_ptr<StatefulIntegerCodec<int32_t>> make_codec(uint8_t kind) {
    switch (kind) {
        case 0: return std::make_unique<SimdCompCodec>();
        case 1: return std::make_unique<FastPForCodec>(
                    CODECFactory{}.getFromName("simdbinarypacking"));
        case 2: return std::make_unique<FastPForCodec>(
                    CODECFactory{}.getFromName("simdpfor"));
        case 3: return std::make_unique<RLECodecAVX512>();
        case 4: {
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
        case 99: {
            return std::make_unique<DirectAccessCodec>();
        }
        default: throw std::invalid_argument("Unknown codec kind");
    }
}

// ================================================================
// Simple array encode/decode interface
// ================================================================
struct CodecHandle {
    std::unique_ptr<StatefulIntegerCodec<int32_t>> codec;
    uint8_t kind;

    CodecHandle(uint8_t kind_) : kind(kind_) {
        codec = make_codec(kind_);
    }
};

void encode_array(CodecHandle &h, py::array_t<int32_t> arr) {
    py::buffer_info info = arr.request();
    auto *ptr = static_cast<int32_t*>(info.ptr);
    size_t n = info.size;
    h.codec->clear();
    h.codec->allocEncoded(ptr, n);
    h.codec->encodeArray(ptr, n);
}

py::array_t<int32_t> decode_array(CodecHandle &h, size_t out_len) {
    py::array_t<int32_t> out(out_len + h.codec->getOverflowSize(out_len));
    py::buffer_info info = out.request();
    auto *ptr = static_cast<int32_t*>(info.ptr);
    h.codec->decodeArray(ptr, out_len);
    return out;
}

// ================================================================
// CachedBlock: tile-based compressed storage
// ================================================================
struct CachedBlock {
    int block_w, block_h;
    int sub_w, sub_h;
    int tiles_x, tiles_y;
    int origin_x, origin_y;
    uint8_t codec_kind;

    std::vector<std::unique_ptr<StatefulIntegerCodec<int32_t>>> codecs;

    CachedBlock(py::array_t<int32_t> arr,
                int bw, int bh,
                int sw, int sh,
                uint8_t kind,
                int ox = 0, int oy = 0)
        : block_w(bw), block_h(bh),
          sub_w(sw), sub_h(sh),
          origin_x(ox), origin_y(oy),
          codec_kind(kind)
    {
        tiles_x = (block_w + sub_w - 1) / sub_w;
        tiles_y = (block_h + sub_h - 1) / sub_h;
        codecs.resize(tiles_x * tiles_y);

        py::buffer_info info = arr.request();
        if (info.ndim != 2)
            throw std::runtime_error("Input array must be 2D");

        auto *ptr = static_cast<int32_t*>(info.ptr);
        int stride_x = info.strides[1] / sizeof(int32_t);
        int stride_y = info.strides[0] / sizeof(int32_t);

        for (int ty = 0; ty < tiles_y; ++ty) {
            for (int tx = 0; tx < tiles_x; ++tx) {
                int x0 = tx * sub_w;
                int y0 = ty * sub_h;
                int x1 = std::min(x0 + sub_w, block_w);
                int y1 = std::min(y0 + sub_h, block_h);
                int w = x1 - x0;
                int h = y1 - y0;

                std::vector<int32_t> tile;
                tile.reserve(w * h);
                for (int yy = 0; yy < h; ++yy) {
                    int32_t* row = ptr + (y0 + yy) * stride_y + x0 * stride_x;
                    for (int xx = 0; xx < w; ++xx) {
                        tile.push_back(row[xx * stride_x]);
                    }
                }

                auto codec = make_codec(kind);
                codec->clear();
                codec->allocEncoded(tile.data(), tile.size());
                codec->encodeArray(tile.data(), tile.size());
                codecs[ty * tiles_x + tx] = std::move(codec);
            }
        }
    }

    py::array_t<int32_t> decode_tile(int tx, int ty) {
        if (tx < 0 || tx >= tiles_x || ty < 0 || ty >= tiles_y)
            throw std::out_of_range("Tile indices out of range");
        int idx = ty * tiles_x + tx;
        auto* codec = codecs[idx].get();

        int w = std::min(sub_w, block_w - tx * sub_w);
        int h = std::min(sub_h, block_h - ty * sub_h);
        size_t n = w * h;

        // py::array_t<int32_t> out(n);
        // py::buffer_info info = out.request();
        // auto* ptr = static_cast<int32_t*>(info.ptr);
        // codec->decodeArray(ptr, n);
        // return out.reshape({h, w});

        py::array_t<int32_t> out({h, w});
        py::buffer_info info = out.request();
        auto* ptr = static_cast<int32_t*>(info.ptr);
        codec->decodeArray(ptr, n);
        return out;
    }
};

// ================================================================
// Pybind11 bindings
// ================================================================
PYBIND11_MODULE(codec, m) {
    py::class_<CodecHandle>(m, "CodecHandle")
        .def(py::init<uint8_t>())
        .def_readonly("kind", &CodecHandle::kind);

    m.def("make_codec", [](uint8_t kind) { return std::make_unique<CodecHandle>(kind); });
    m.def("encode_array", &encode_array);
    m.def("decode_array", &decode_array);

    py::class_<CachedBlock>(m, "CachedBlock")
        .def(py::init<py::array_t<int32_t>, int, int, int, int, uint8_t, int, int>(),
             py::arg("array"),
             py::arg("block_w"),
             py::arg("block_h"),
             py::arg("sub_w"),
             py::arg("sub_h"),
             py::arg("codec_kind"),
             py::arg("origin_x") = 0,
             py::arg("origin_y") = 0)
        .def("decode_tile", &CachedBlock::decode_tile)
        .def_readonly("origin_x", &CachedBlock::origin_x)
        .def_readonly("origin_y", &CachedBlock::origin_y);
}