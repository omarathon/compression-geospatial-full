// CR-parity harness for the 128-bit fused FoR codecs.
//
// Compares each new vectorized codec against the equivalent non-vectorized
// composite (logical FoR → SimdCompFusedCodecU16_128 physical), tile-by-tile
// over the 3 local test TIFs, and reports the byte ratio vec/ref.
//
//   Regular FoR:    SimdCompFusedForCodecU16_128(w, sep)
//                   vs Composite(FORCodecU16(w, sep), SimdCompFusedCodecU16_128)
//   Hierarchical:   SimdCompFusedForHierarchicalCodecU16_128(gw, lw)
//                   vs Composite(FORHierarchicalCodecU16(gw, lw, sep=true), …)
//
// Input is normalized to uint16 by subtracting the global raster min (matches
// the bench's primary min-shift); both codecs see identical input, so the ratio
// is independent of the exact normalization.

#include <gdal_priv.h>

#include <cstdint>
#include <cstdio>
#include <limits>
#include <memory>
#include <string>
#include <vector>

#include "composite_codec.h"
#include "custom_unvec_logic_codecs_u16.h"
#include "simdcomp_fused_codec_uint16_w128.h"
#include "simdcomp_for_codec_uint16_w128.h"

static constexpr int kTile = 256;            // 256×256 = 65536 elements
static constexpr size_t kBlock = kTile * kTile;
static constexpr size_t kMaxTiles = 1500;    // cap (evenly strided) for speed

using Codec = StatefulIntegerCodec<uint16_t>;

static size_t EncodeBytes(Codec& c, const uint16_t* in, size_t n) {
  c.clear();
  c.AllocEncoded(in, n);
  c.EncodeArray(in, n);
  size_t bytes = c.EncodedNumValues() * c.EncodedSizeValue();
  c.clear();
  return bytes;
}

// Read all full kTile×kTile tiles of band 1, normalized to uint16 (minus global
// min). Returns flattened tiles (numTiles × kBlock), strided down to ≤kMaxTiles.
static std::vector<uint16_t> LoadTiles(const std::string& path, size_t& numTiles) {
  GDALDataset* ds = (GDALDataset*)GDALOpen(path.c_str(), GA_ReadOnly);
  if (!ds) { fprintf(stderr, "open failed: %s\n", path.c_str()); return {}; }
  GDALRasterBand* band = ds->GetRasterBand(1);
  const int W = band->GetXSize(), H = band->GetYSize();
  const int tx = W / kTile, ty = H / kTile;
  const size_t allTiles = (size_t)tx * ty;
  const size_t stride = (allTiles + kMaxTiles - 1) / kMaxTiles;  // ≥1

  // Pass 1: global min over the sampled tiles (read as int32 to cover Int16).
  std::vector<int32_t> tile(kBlock);
  int32_t gmin = std::numeric_limits<int32_t>::max();
  std::vector<std::pair<int,int>> coords;
  for (int y = 0; y < ty; ++y)
    for (int x = 0; x < tx; ++x) coords.push_back({x, y});
  std::vector<std::pair<int,int>> sampled;
  for (size_t i = 0; i < coords.size(); i += stride) sampled.push_back(coords[i]);

  for (auto [x, y] : sampled) {
    band->RasterIO(GF_Read, x * kTile, y * kTile, kTile, kTile, tile.data(),
                   kTile, kTile, GDT_Int32, 0, 0);
    for (int32_t v : tile) if (v < gmin) gmin = v;
  }

  // Pass 2: shift to uint16.
  numTiles = sampled.size();
  std::vector<uint16_t> out(numTiles * kBlock);
  for (size_t t = 0; t < numTiles; ++t) {
    auto [x, y] = sampled[t];
    band->RasterIO(GF_Read, x * kTile, y * kTile, kTile, kTile, tile.data(),
                   kTile, kTile, GDT_Int32, 0, 0);
    uint16_t* dst = out.data() + t * kBlock;
    for (size_t i = 0; i < kBlock; ++i)
      dst[i] = (uint16_t)(tile[i] - gmin);
  }
  GDALClose(ds);
  return out;
}

struct Cfg {
  std::string label;
  std::unique_ptr<Codec> vec;
  std::unique_ptr<Codec> ref;
};

static std::unique_ptr<Codec> RefComposite(std::unique_ptr<Codec> logical) {
  return std::make_unique<CompositeStatefulIntegerCodec<uint16_t>>(
      std::move(logical), std::make_unique<SimdCompFusedCodecU16_128>());
}

static std::vector<Cfg> MakeConfigs() {
  std::vector<Cfg> cfgs;
  for (size_t w : {4u, 8u, 16u, 32u, 64u, 128u, 256u}) {
    for (bool sep : {false, true}) {
      Cfg c;
      c.label = "regular w" + std::to_string(w) + (sep ? " sep " : " mixed");
      c.vec = std::make_unique<SimdCompFusedForCodecU16_128>(w, sep);
      c.ref = RefComposite(std::make_unique<FORCodecU16>(w, sep));
      cfgs.push_back(std::move(c));
    }
  }
  for (size_t gw : {128u, 256u}) {
    for (size_t lw : {4u, 8u, 16u, 32u, 64u, 128u, 256u}) {
      if (lw > gw || gw % lw) continue;
      Cfg c;
      c.label = "hier   g" + std::to_string(gw) + " l" + std::to_string(lw);
      c.vec = std::make_unique<SimdCompFusedForHierarchicalCodecU16_128>(gw, lw);
      // unvec hierarchical non-separate is variable-length (uncascadable); the
      // only cascadable reference is sep=true (residuals → physical, metadata
      // held aside) — which is what our single hierarchical variant matches.
      c.ref = RefComposite(
          std::make_unique<FORHierarchicalCodecU16>(gw, lw, /*separate=*/true));
      cfgs.push_back(std::move(c));
    }
  }
  return cfgs;
}

int main(int argc, char** argv) {
  GDALAllRegister();
  std::vector<std::string> tifs = {
      "/home/omar/diss/geotiffs/srtm_45_15.tif",
      "/home/omar/diss/geotiffs/slope-srtm_35_11.tif",
      "/home/omar/diss/geotiffs/WorldCover_nyc_ESA_WorldCover_10m_2021_v200_N39W075.tif",
  };
  if (argc > 1) tifs.assign(argv + 1, argv + argc);

  for (const auto& tif : tifs) {
    size_t numTiles = 0;
    std::vector<uint16_t> tiles = LoadTiles(tif, numTiles);
    if (tiles.empty()) continue;
    const std::string base = tif.substr(tif.find_last_of('/') + 1);
    printf("\nTIF: %s   (tiles=%zu, %zu elems)\n", base.c_str(), numTiles,
           numTiles * kBlock);
    printf("  %-22s %9s %9s %9s\n", "config", "vec_CR", "ref_CR", "vec/ref");

    auto cfgs = MakeConfigs();
    for (auto& c : cfgs) {
      size_t vb = 0, rb = 0;
      for (size_t t = 0; t < numTiles; ++t) {
        const uint16_t* blk = tiles.data() + t * kBlock;
        vb += EncodeBytes(*c.vec, blk, kBlock);
        rb += EncodeBytes(*c.ref, blk, kBlock);
      }
      const double raw = (double)numTiles * kBlock * sizeof(uint16_t);
      printf("  %-22s %8.2f%% %8.2f%% %9.4f\n", c.label.c_str(),
             100.0 * vb / raw, 100.0 * rb / raw, (double)vb / (double)rb);
    }
  }
  return 0;
}
