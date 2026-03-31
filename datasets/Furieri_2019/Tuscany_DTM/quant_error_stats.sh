#!/usr/bin/env bash
set -euo pipefail

if [ "$#" -lt 3 ] || [ "$#" -gt 4 ]; then
  echo "Usage: $0 ORIGINAL_RASTER QUANTIZED_RASTER MULTIPLIER [QUANTIZED_NODATA]"
  exit 1
fi

ORIG="$1"
QUANT="$2"
MULT="$3"
QUANT_NODATA="${4:--2147483648}"
ERR_NODATA="-9999"

for cmd in gdalinfo gdal_calc.py awk; do
  command -v "$cmd" >/dev/null 2>&1 || {
    echo "Error: required command not found: $cmd"
    exit 1
  }
done

ORIG_NODATA="$(gdalinfo "$ORIG" | awk -F= '/NoData Value=/ {print $2; exit}' | tr -d '[:space:]')"

if [ -z "${ORIG_NODATA}" ]; then
  echo "Error: original raster has no NoData value set."
  exit 1
fi

TMPDIR="$(mktemp -d)"
trap 'rm -rf "$TMPDIR"' EXIT

ABS_ERR_TIF="$TMPDIR/abs_error.tif"
PCT_ERR_TIF="$TMPDIR/pct_error.tif"

echo "Original raster:  $ORIG"
echo "Quantized raster: $QUANT"
echo "Multiplier:       $MULT"
echo "Original NoData:  $ORIG_NODATA"
echo "Quantized NoData: $QUANT_NODATA"
echo

# Absolute error (always safe)
echo "Computing absolute error..."
gdal_calc.py \
  -A "$ORIG" \
  -B "$QUANT" \
  --outfile="$ABS_ERR_TIF" \
  --calc="where((A==${ORIG_NODATA})|(B==${QUANT_NODATA}), ${ERR_NODATA}, abs(A-(B/${MULT})))" \
  --type=Float64 \
  --NoDataValue="${ERR_NODATA}" \
  --overwrite

# Percentage error (NO exclusions except nodata)
# Avoid runtime warnings by using safe denominator but NOT masking zeros
echo "Computing percentage error..."
gdal_calc.py \
  -A "$ORIG" \
  -B "$QUANT" \
  --outfile="$PCT_ERR_TIF" \
  --calc="where((A==${ORIG_NODATA})|(B==${QUANT_NODATA}), ${ERR_NODATA}, 100*abs(A-(B/${MULT}))/where(A==0,1e-20,abs(A)))" \
  --type=Float64 \
  --NoDataValue="${ERR_NODATA}" \
  --overwrite

get_stats() {
  local tif="$1"
  local label="$2"

  local stats
  stats="$(gdalinfo -stats "$tif")"

  local min max mean stddev
  min="$(printf '%s\n' "$stats" | awk -F= '/STATISTICS_MINIMUM/ {print $2; exit}')"
  max="$(printf '%s\n' "$stats" | awk -F= '/STATISTICS_MAXIMUM/ {print $2; exit}')"
  mean="$(printf '%s\n' "$stats" | awk -F= '/STATISTICS_MEAN/ {print $2; exit}')"
  stddev="$(printf '%s\n' "$stats" | awk -F= '/STATISTICS_STDDEV/ {print $2; exit}')"

  echo "$label"
  echo "  min   = ${min:-N/A}"
  echo "  mean  = ${mean:-N/A}"
  echo "  max   = ${max:-N/A}"
  echo "  stddev= ${stddev:-N/A}"
  echo
}

echo
get_stats "$ABS_ERR_TIF" "Absolute error (same units as DEM):"
get_stats "$PCT_ERR_TIF" "Percentage error (%):"