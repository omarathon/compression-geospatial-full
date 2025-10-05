
// static std::unique_ptr<Arena> globalArena = nullptr;

// struct RasterMetadata {
//     size_t w;
//     size_t h;
//     uint32_t bitMax;
// };
// void initArena(std::vector<RasterMetadata> metadatas, int sw, int sh) {
//     size_t max_tile_elems = sw * sh;
//     size_t maxSizeBytes = 0;
//     for (const auto& metadata : metadatas) {
//         int tiles_x = (metadata.w + sw - 1) / sw;
//         int tiles_y = (metadata.h + sh - 1) / sh;
//         int numTiles = tiles_x * tiles_y;
//         size_t max_tile_bytes = simdpack_compressedbytes(max_tile_elems, metadata.bitMax);
//         maxSizeBytes += numTiles * max_tile_bytes;
//     }
//     globalArena = std::make_unique<Arena>(/* size */ maxSizeBytes);
// }

// void finalizeArena() {
//     if (!globalArena) return;
//     globalArena->shrink_to_fit();
// }

// struct ArenaCompressedBlockSequence {
//     // Arena _arena;
//     std::vector<std::unique_ptr<StatefulIntegerCodec<int32_t>>> _compressedBlocks;
//     // std::vector<int32_t> _scratchTile;

//     size_t _size_bytes;
//     int _sw;
//     int _sh;
//     uint8_t _codec_kind;
//     uint8_t _morton_mode;

//     ArenaCompressedBlockSequence(int sw, int sh, uint8_t kind, uint8_t morton_mode)
//         : _size_bytes{0}, _sw{sw}, _sh{sh},
//           _codec_kind{kind}, _morton_mode{morton_mode}
//     {
//         // scratch tile includes morton buffer + overflow
//         // if (_morton_mode > 0) {
//         //     _scratchTile.resize(2 * sw * sh + make_int32_codec(kind)->getOverflowSize(sw * sh));
//         // } else {
//         //     _scratchTile.resize(sw * sh + make_int32_codec(kind)->getOverflowSize(sw * sh));
//         // }
//     }

//     bool HasBlock(int id) {
//         return id >= 0 && id < (int)_compressedBlocks.size();
//     }

//     // void AllocArena(int w, int h, uint32_t bitsMax) {
//     //     int tiles_x = (w + _sw - 1) / _sw;
//     //     int tiles_y = (h + _sh - 1) / _sh;
//     //     int numTiles = tiles_x * tiles_y;

//     //     size_t max_tile_elems = _sw * _sh;
//     //     size_t max_tile_bytes = simdpack_compressedbytes(max_tile_elems, bitsMax);

//     //     _arena.reserve(numTiles * max_tile_bytes);
//     // }

//     void WriteBlocks(py::array_t<int32_t> arr, int bw, int bh) {
//         int tiles_x = (bw + _sw - 1) / _sw;
//         int tiles_y = (bh + _sh - 1) / _sh;
//         int numTiles = tiles_x * tiles_y;
//         _compressedBlocks.reserve(_compressedBlocks.size() + numTiles);

//         //  // --- NEW: pre-reserve arena capacity ---
//         // size_t max_tile_elems = _sw * _sh;
//         // // worst case: 32 bits per int32_t (uncompressed size)
//         // size_t max_tile_bytes = simdpack_compressedbytes(max_tile_elems, 16);
//         // _arena.reserve(numTiles * max_tile_bytes);

//         py::buffer_info info = arr.request();
//         if (info.ndim != 2)
//             throw std::runtime_error("Input array must be 2D");

//         auto *ptr = static_cast<int32_t*>(info.ptr);
//         int stride_x = info.strides[1] / sizeof(int32_t);
//         int stride_y = info.strides[0] / sizeof(int32_t);

//         for (int ty = 0; ty < tiles_y; ++ty) {
//             for (int tx = 0; tx < tiles_x; ++tx) {
//                 int x0 = tx * _sw;
//                 int y0 = ty * _sh;
//                 int x1 = std::min(x0 + _sw, bw);
//                 int y1 = std::min(y0 + _sh, bh);
//                 int w = x1 - x0;
//                 int h = y1 - y0;

//                 auto codec = std::make_unique<ArenaSimdCompCodec>(globalArena.get());

//                 if (h == 1) {
//                     int32_t* tile_ptr = ptr + y0 * stride_y + x0;
//                     codec->allocEncoded(tile_ptr, w * h);
//                     codec->encodeArray(tile_ptr, w * h);
//                 } else {
//                     for (int yy = 0; yy < h; ++yy) {
//                         int32_t* src_row = ptr + (y0 + yy) * stride_y + x0 * stride_x;
//                         int32_t* dst_row = globalScratch.data() + yy * w;
//                         if (stride_x == 1) {
//                             std::memcpy(dst_row, src_row, w * sizeof(int32_t));
//                         } else {
//                             for (int xx = 0; xx < w; ++xx) {
//                                 dst_row[xx] = src_row[xx * stride_x];
//                             }
//                         }
//                     }
//                     if (_morton_mode > 0) {
//                         if (w == h && w > 1) {
//                             int32_t* mortonBuf = globalScratch.data() + (w * h);
//                             if (_morton_mode == 1) {
//                                 MortonCache& mortonCodes = getMortonCache(w);
//                                 for (int i = 0; i < w * h; i++) {
//                                     mortonBuf[mortonCodes.to_morton[i]] = globalScratch[i];
//                                 }
//                             } else {
//                                 for (int y = 0; y < h; y++) {
//                                     for (int x = 0; x < w; x++) {
//                                         size_t idx = y * w + x;
//                                         uint32_t m = libmorton::morton2D_32_encode(
//                                             static_cast<uint_fast16_t>(x),
//                                             static_cast<uint_fast16_t>(y)
//                                         );
//                                         mortonBuf[m] = globalScratch[idx];
//                                     }
//                                 }
//                             }
//                             codec->allocEncoded(mortonBuf, w * h);
//                             codec->encodeArray(mortonBuf, w * h);
//                         } else {
//                             codec->allocEncoded(globalScratch.data(), w * h);
//                             codec->encodeArray(globalScratch.data(), w * h);
//                         }
//                     } else {
//                         codec->allocEncoded(globalScratch.data(), w * h);
//                         codec->encodeArray(globalScratch.data(), w * h);
//                     }
//                 }
//                 _size_bytes += codec->encodedNumValues();
//                 _compressedBlocks.push_back(std::move(codec));
//             }
//         }
//     }

//     py::array_t<int32_t> ReadBlock(int id, int sw, int sh) {
//         if (!HasBlock(id))
//             throw std::out_of_range("Block out of range");

//         auto* codec = _compressedBlocks[id].get();

//         size_t n = sw * sh;
//         codec->decodeArray(globalScratch.data(), n);

//         if (_morton_mode > 0 && sw == sh && sw > 1) {
//             int32_t* mortonBuf = globalScratch.data() + (sw * sh);
//             if (_morton_mode == 1) {
//                 MortonCache& mortonCodes = getMortonCache(sw);
//                 for (int i = 0; i < sw * sh; i++) {
//                     mortonBuf[i] = globalScratch[mortonCodes.from_morton[i]];
//                 }
//             } else {
//                 for (int y = 0; y < sh; y++) {
//                     for (int x = 0; x < sw; x++) {
//                         size_t idx = y * sw + x;
//                         uint32_t m = libmorton::morton2D_32_encode(
//                             static_cast<uint_fast16_t>(x),
//                             static_cast<uint_fast16_t>(y)
//                         );
//                         mortonBuf[idx] = globalScratch[m];
//                     }
//                 }
//             }
//             return py::array_t<int32_t>(
//                 {sh, sw},
//                 {sizeof(int32_t) * sw, sizeof(int32_t)},
//                 mortonBuf
//             );
//         }

//         return py::array_t<int32_t>(
//             {sh, sw},
//             {sizeof(int32_t) * sw, sizeof(int32_t)},
//             globalScratch.data()
//         );
//     }

//     size_t SizeBytes() {
//         return _size_bytes;
//     }

//     // void finalize() {
//     //     _arena.shrink_to_fit();
//     // }
// };
