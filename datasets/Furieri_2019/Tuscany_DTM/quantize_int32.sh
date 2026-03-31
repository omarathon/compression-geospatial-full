#!/usr/bin/env bash
set -euo pipefail

if [ "$#" -lt 3 ] || [ "$#" -gt 4 ]; then
  echo "Usage: $0 INPUT_RASTER OUTPUT_RASTER MULTIPLIER [OUTPUT_NODATA]"
  echo
  echo "Example:"
  echo "  $0 output.tif quantized_int32.tif 256"
  echo "  $0 output.tif quantized_int32.tif 100 -2147483648"
  exit 1
fi

INPUT="$1"
OUTPUT="$2"
MULT="$3"
OUT_NODATA="${4:--2147483648}"

for cmd in gdalinfo gdal_calc.py gdal_translate python3; do
  command -v "$cmd" >/dev/null 2>&1 || {
    echo "Error: required command not found: $cmd"
    exit 1
  }
done

SRC_NODATA="$(gdalinfo "$INPUT" | awk -F= '/NoData Value=/ {print $2; exit}' | tr -d '[:space:]')"

if [ -z "${SRC_NODATA}" ]; then
  echo "Error: source raster has no NoData value set."
  exit 1
fi

TMPDIR="$(mktemp -d)"
trap 'rm -rf "$TMPDIR"' EXIT

QUANT_TMP="$TMPDIR/quantized_tmp.tif"
DEQUANT_TMP="$TMPDIR/dequantized_tmp.tif"
ERR_TMP="$TMPDIR/error_tmp.tif"
STATS_JSON="$TMPDIR/error_stats.json"

echo "Input:         $INPUT"
echo "Output:        $OUTPUT"
echo "Multiplier:    $MULT"
echo "Source NoData: $SRC_NODATA"
echo "Output NoData: $OUT_NODATA"
echo

echo "Quantizing..."
gdal_calc.py \
  -A "$INPUT" \
  --outfile="$QUANT_TMP" \
  --calc="where(A==${SRC_NODATA}, ${OUT_NODATA}, around(A*${MULT}))" \
  --type=Int32 \
  --NoDataValue="${OUT_NODATA}" \
  --co=COMPRESS=ZSTD \
  --co=PREDICTOR=2 \
  --co=TILED=YES \
  --overwrite

mv "$QUANT_TMP" "$OUTPUT"