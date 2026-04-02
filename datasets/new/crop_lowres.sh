#!/usr/bin/env bash
# Crop ETOPO1 and LandScan from existing server files to benchmark regions.
# WorldClim cropping is handled in download_srtm_worldclim.sh.
#
# Prerequisites:
#   - ETOPO1: already on server at /maps/omsst2/diss/papers/rasterlite/ETOPO1/
#   - LandScan: must be downloaded first (see download_instructions.md)
#
# Usage:
#   bash crop_lowres.sh [outdir]

set -euo pipefail

OUTDIR="${1:-.}"
cd "$OUTDIR"

# ─────────────────────────────────────────────
# ETOPO1 (1 arc-minute, ~1.86 km) — already on server
# ─────────────────────────────────────────────
echo "=== ETOPO1 (6 regions including ocean) ==="
mkdir -p etopo1

ETOPO1_SRC="/maps/omsst2/diss/papers/rasterlite/ETOPO1/ETOPO1_Ice_g_geotiff.tif"

if [ ! -f "$ETOPO1_SRC" ]; then
    echo "ERROR: ETOPO1 source not found at $ETOPO1_SRC" >&2
    echo "  Update ETOPO1_SRC to match your server path." >&2
else
    gdal_translate -q -projwin  -84.0 50.7  -64.0 30.7 "$ETOPO1_SRC" etopo1/ETOPO1_nyc.tif
    gdal_translate -q -projwin -103.5 52.0  -83.5 32.0 "$ETOPO1_SRC" etopo1/ETOPO1_iowa.tif
    gdal_translate -q -projwin   76.9 37.9   96.9 17.9 "$ETOPO1_SRC" etopo1/ETOPO1_himalayas.tif
    gdal_translate -q -projwin   -8.0 34.0   12.0 14.0 "$ETOPO1_SRC" etopo1/ETOPO1_sahara.tif
    gdal_translate -q -projwin  -70.0  7.0  -50.0 -13.0 "$ETOPO1_SRC" etopo1/ETOPO1_amazon.tif
    gdal_translate -q -projwin  -40.0 55.0  -20.0 35.0 "$ETOPO1_SRC" etopo1/ETOPO1_ocean.tif
    echo "  done — output in etopo1/ETOPO1_*.tif"
fi

# ─────────────────────────────────────────────
# LandScan (~1 km) — requires manual download first
# ─────────────────────────────────────────────
echo ""
echo "=== LandScan (5 regions, skip ocean) ==="
mkdir -p landscan

LANDSCAN_SRC="landscan/landscan-global-2024.tif"

if [ ! -f "$LANDSCAN_SRC" ]; then
    echo "SKIP: LandScan global file not found at $LANDSCAN_SRC"
    echo "  Download it first — see download_instructions.md"
else
    gdal_translate -q -projwin  -84.0 50.7  -64.0 30.7 "$LANDSCAN_SRC" landscan/LandScan_nyc.tif
    gdal_translate -q -projwin -103.5 52.0  -83.5 32.0 "$LANDSCAN_SRC" landscan/LandScan_iowa.tif
    gdal_translate -q -projwin   76.9 37.9   96.9 17.9 "$LANDSCAN_SRC" landscan/LandScan_himalayas.tif
    gdal_translate -q -projwin   -8.0 34.0   12.0 14.0 "$LANDSCAN_SRC" landscan/LandScan_sahara.tif
    gdal_translate -q -projwin  -70.0  7.0  -50.0 -13.0 "$LANDSCAN_SRC" landscan/LandScan_amazon.tif
    echo "  done — output in landscan/LandScan_*.tif"
fi

echo ""
echo "=== Done ==="
