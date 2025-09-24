#include <pybind11/pybind11.h>
#include <pybind11/numpy.h>
#include <memory>
#include <stdexcept>

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
#include <vector>
#include <unordered_map>

namespace py = pybind11;

struct CodecHandle {
    std::unique_ptr<StatefulIntegerCodec<int32_t>> codec;
    uint8_t kind;
};

std::unique_ptr<CodecHandle> make_codec(uint8_t kind) {
    auto handle = std::make_unique<CodecHandle>();
    handle->kind = kind;
    switch (kind) {
        case 0:
            // simdcomp
            handle->codec = std::make_unique<SimdCompCodec>();
            break;
        case 1:
            // FastPFor SIMDBinaryPacking+VariableByte
            handle->codec = std::make_unique<FastPForCodec>(
                CODECFactory{}.getFromName("simdbinarypacking")
            );
            break;
        case 2:
            // FastPFor SIMDPFor+VariableByte
            handle->codec = std::make_unique<FastPForCodec>(
                CODECFactory{}.getFromName("simdpfor")
            );
            break;
        case 3:
            // custom rle vecavx512
            handle->codec = std::make_unique<RLECodecAVX512>();
            break;
        case 4: {
            // [+] custom rle vecavx512+simdcomp
            auto rle = std::make_unique<RLECodecAVX512>();
            auto simdcomp = std::make_unique<SimdCompCodec>();
            handle->codec = std::make_unique<CompositeStatefulIntegerCodec<int32_t>>(
                std::move(rle), std::move(simdcomp)
            );
            break;
        }
        case 5:
            // TurboPFor xor+TurboPack256
            handle->codec = std::make_unique<TurboPForCodec>(12);
            break;
        case 6: {
            // [+] custom rle vecavx512+FastPForSIMDBinaryPacking+VariableByte
            auto rle = std::make_unique<RLECodecAVX512>();
            auto binpack = std::make_unique<FastPForCodec>(
                CODECFactory{}.getFromName("simdbinarypacking")
            );
            handle->codec = std::make_unique<CompositeStatefulIntegerCodec<int32_t>>(
                std::move(rle), std::move(binpack)
            );
            break;
        }
        default:
            throw std::invalid_argument("Unknown codec kind");
    }
    return handle;
}

void encode_array(CodecHandle &h, py::array_t<int32_t> arr) {
    py::buffer_info info = arr.request();
    auto *ptr = static_cast<int32_t*>(info.ptr);
    size_t n = info.size;
    h.codec->clear();                // reset if reused
    h.codec->encodeArray(ptr, n);    // compress into internal state
}

py::array_t<int32_t> decode_array(CodecHandle &h, size_t out_len) {
    py::array_t<int32_t> out(out_len + h.codec->getOverflowSize(out_len));
    py::buffer_info info = out.request();
    auto *ptr = static_cast<int32_t*>(info.ptr);
    h.codec->decodeArray(ptr, out_len);  // fill buffer
    return out;
}

// Pybind11 module
PYBIND11_MODULE(codec, m) {
    py::class_<CodecHandle>(m, "CodecHandle");

    m.def("make_codec", &make_codec, py::arg("kind"));
    m.def("encode_array", &encode_array, py::arg("handle"), py::arg("arr"));
    m.def("decode_array", &decode_array, py::arg("handle"), py::arg("out_len"));
}