#!/usr/bin/env bash
# Download SRTM 3 arc-second tiles, WorldClim 1.4 (temp + precip), and ETOPO 2022.
#
# All downloads are direct wget — no authentication required.
# Run from the datasets/new/ directory.
#
# Usage:
#   bash download_srtm_worldclim.sh

set -euo pipefail

OUTDIR="${1:-.}"
cd "$OUTDIR"

# ─────────────────────────────────────────────
# SRTM 3 arc-second (~90m) — CGIAR 5°×5° tiles
# ─────────────────────────────────────────────
echo "=== SRTM 3 arc-second ==="
mkdir -p srtm

SRTM_BASE="https://srtm.csi.cgiar.org/wp-content/uploads/files/srtm_5x5/TIFF"
TILES=(srtm_22_04 srtm_18_04 srtm_54_07 srtm_37_08 srtm_24_13)
TILE_NAMES=(nyc iowa himalayas sahara amazon)

for tile in "${TILES[@]}"; do
    if [ ! -f "srtm/${tile}.zip" ]; then
        echo "  downloading ${tile}..."
        wget -q -nc -O "srtm/${tile}.zip" "${SRTM_BASE}/${tile}.zip"
    fi
    if [ ! -d "srtm/${tile}" ]; then
        echo "  extracting ${tile}..."
        unzip -q -o "srtm/${tile}.zip" -d srtm/
    fi
done

echo "  cropping to 1x1 degree..."
gdal_translate -q -projwin -74.5 41.2 -73.5 40.2 srtm/srtm_22_04.tif srtm/SRTM_nyc.tif
gdal_translate -q -projwin -94.0 42.5 -93.0 41.5 srtm/srtm_18_04.tif srtm/SRTM_iowa.tif
gdal_translate -q -projwin  86.4 28.4  87.4 27.4 srtm/srtm_54_07.tif srtm/SRTM_himalayas.tif
gdal_translate -q -projwin   1.5 24.5   2.5 23.5 srtm/srtm_37_08.tif srtm/SRTM_sahara.tif
gdal_translate -q -projwin -61.0 -2.5 -60.0 -3.5 srtm/srtm_24_13.tif srtm/SRTM_amazon.tif

echo "  done — output in srtm/SRTM_*.tif"

# ─────────────────────────────────────────────
# WorldClim 2.1 Temperature (January, remote crop)
# same 30s resolution, same regions, no full 4 GB download first
# ─────────────────────────────────────────────
echo ""
echo "=== WorldClim 2.1 Temperature ==="
mkdir -p worldclim

src="/vsizip//vsicurl/https://geodata.ucdavis.edu/climate/worldclim/2_1/base/wc2.1_30s_tavg.zip/wc2.1_30s_tavg_01.tif"

echo "  cropping January (tavg_01) to 20x20 degree regions..."
gdal_translate -q -projwin  -84.0 50.7  -64.0 30.7 "$src" worldclim/WorldClim_temp_nyc.tif
gdal_translate -q -projwin -103.5 52.0  -83.5 32.0 "$src" worldclim/WorldClim_temp_iowa.tif
gdal_translate -q -projwin   76.9 37.9   96.9 17.9 "$src" worldclim/WorldClim_temp_himalayas.tif
gdal_translate -q -projwin   -8.0 34.0   12.0 14.0 "$src" worldclim/WorldClim_temp_sahara.tif
gdal_translate -q -projwin  -70.0  7.0  -50.0 -13.0 "$src" worldclim/WorldClim_temp_amazon.tif

echo "  done — output in worldclim/WorldClim_temp_*.tif"

# ─────────────────────────────────────────────
# WorldClim 2.1 Precipitation (January, remote crop)
# same 30s resolution, same regions
# ─────────────────────────────────────────────
echo ""
echo "=== WorldClim 2.1 Precipitation ==="
mkdir -p worldclim

src="/vsizip//vsicurl/https://geodata.ucdavis.edu/climate/worldclim/2_1/base/wc2.1_30s_prec.zip/wc2.1_30s_prec_01.tif"

echo "  cropping January (prec_01) to 20x20 degree regions..."
gdal_translate -q -projwin  -84.0 50.7  -64.0 30.7 "$src" worldclim/WorldClim_prec_nyc.tif
gdal_translate -q -projwin -103.5 52.0  -83.5 32.0 "$src" worldclim/WorldClim_prec_iowa.tif
gdal_translate -q -projwin   76.9 37.9   96.9 17.9 "$src" worldclim/WorldClim_prec_himalayas.tif
gdal_translate -q -projwin   -8.0 34.0   12.0 14.0 "$src" worldclim/WorldClim_prec_sahara.tif
gdal_translate -q -projwin  -70.0  7.0  -50.0 -13.0 "$src" worldclim/WorldClim_prec_amazon.tif

echo "  done — output in worldclim/WorldClim_prec_*.tif"

# ─────────────────────────────────────────────
# ETOPO 2022 (15 arc-second, ~450m)
# full official tiled download + one global VRT
# ─────────────────────────────────────────────
echo ""
echo "=== ETOPO 2022 HighRes (15 arc-second) ==="
mkdir -p etopo_highres/tiles

BASE_URL="https://www.ngdc.noaa.gov/mgg/global/relief/ETOPO2022/data/15s/15s_surface_elev_gtif"
ETOPO_VRT="etopo_highres/ETOPO_2022_v1_15s_surface_global.vrt"

if [ ! -f "$ETOPO_VRT" ]; then
    echo "  downloading all official 15s surface GeoTIFF tiles..."
    wget -q -r -np -nd \
        -A "ETOPO_2022_v1_15s_*_surface.tif" \
        -R "index.html*" \
        -P etopo_highres/tiles \
        "$BASE_URL/"

    echo "  building global VRT..."
    gdalbuildvrt "$ETOPO_VRT" etopo_highres/tiles/ETOPO_2022_v1_15s_*_surface.tif
fi

echo "  cropping to 20x20 degree regions (6 regions including ocean)..."
gdal_translate -q -projwin  -84.0 50.7  -64.0 30.7 "$ETOPO_VRT" etopo_highres/ETOPO_HighRes_nyc.tif
gdal_translate -q -projwin -103.5 52.0  -83.5 32.0 "$ETOPO_VRT" etopo_highres/ETOPO_HighRes_iowa.tif
gdal_translate -q -projwin   76.9 37.9   96.9 17.9 "$ETOPO_VRT" etopo_highres/ETOPO_HighRes_himalayas.tif
gdal_translate -q -projwin   -8.0 34.0   12.0 14.0 "$ETOPO_VRT" etopo_highres/ETOPO_HighRes_sahara.tif
gdal_translate -q -projwin  -70.0  7.0  -50.0 -13.0 "$ETOPO_VRT" etopo_highres/ETOPO_HighRes_amazon.tif
gdal_translate -q -projwin  -40.0 55.0  -20.0 35.0 "$ETOPO_VRT" etopo_highres/ETOPO_HighRes_ocean.tif

echo "  done — output in etopo_highres/ETOPO_HighRes_*.tif"

echo ""
echo "=== All downloads complete ==="
