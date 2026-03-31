#!/usr/bin/env bash
set -euo pipefail

# Build an Italy-region SRTM substitute from a directory full of CGIAR SRTM zip files.
#
# Expected:
# - You already downloaded many srtm_*.zip files into one directory.
# - Each zip contains one .tif tile.
#
# Output:
# - SRTM_Italy_final.tif
#
# Region target:
#   west=5 east=20 south=35 north=50

WEST=5
SOUTH=35
EAST=20
NORTH=50

OUT="SRTM_Italy_final.tif"
TMPDIR="${TMPDIR:-./_srtm_build_tmp}"
EXTRACT_DIR="$TMPDIR/extracted"
SELECT_DIR="$TMPDIR/selected"
VRT="$TMPDIR/srtm_italy.vrt"

mkdir -p "$EXTRACT_DIR" "$SELECT_DIR"

for cmd in unzip gdalinfo gdalbuildvrt gdalwarp awk sed grep; do
  command -v "$cmd" >/dev/null 2>&1 || {
    echo "Error: required command not found: $cmd" >&2
    exit 1
  }
done

echo "== Extracting zip files =="
shopt -s nullglob
zips=( *.zip )
if [ ${#zips[@]} -eq 0 ]; then
  echo "Error: no .zip files found in $(pwd)" >&2
  exit 1
fi

for z in "${zips[@]}"; do
  echo "Extracting $z"
  unzip -o -q "$z" -d "$EXTRACT_DIR"
done

echo
echo "== Finding GeoTIFF tiles =="
mapfile -t tif_files < <(find "$EXTRACT_DIR" -type f \( -iname "*.tif" -o -iname "*.tiff" \) | sort)

if [ ${#tif_files[@]} -eq 0 ]; then
  echo "Error: no .tif files found after extraction" >&2
  exit 1
fi

echo "Found ${#tif_files[@]} TIFF files"
echo

intersects() {
  # args: ulx uly lrx lry west south east north
  awk -v ulx="$1" -v uly="$2" -v lrx="$3" -v lry="$4" \
      -v west="$5" -v south="$6" -v east="$7" -v north="$8" '
    BEGIN {
      # raster bbox:
      # minx=ulx, maxx=lrx, miny=lry, maxy=uly
      # intersection test with target bbox
      if (lrx <= west || ulx >= east || uly <= south || lry >= north) {
        print 0
      } else {
        print 1
      }
    }'
}

selected_count=0

echo "== Inspecting extents and selecting intersecting tiles =="
for tif in "${tif_files[@]}"; do
  info="$(gdalinfo "$tif")"

  ul_line="$(printf '%s\n' "$info" | grep "Upper Left" || true)"
  lr_line="$(printf '%s\n' "$info" | grep "Lower Right" || true)"

  if [ -z "$ul_line" ] || [ -z "$lr_line" ]; then
    echo "Skipping $tif (could not parse bounds)"
    continue
  fi

  # Extract decimal coordinates from:
  # Upper Left  (   5.0000000,  50.0000000)
  # Lower Right (  10.0000000,  45.0000000)
  ulx="$(printf '%s\n' "$ul_line" | sed -E 's/.*\(([[:space:]]*[-0-9.]+),([[:space:]]*[-0-9.]+)\).*/\1/' | tr -d ' ')"
  uly="$(printf '%s\n' "$ul_line" | sed -E 's/.*\(([[:space:]]*[-0-9.]+),([[:space:]]*[-0-9.]+)\).*/\2/' | tr -d ' ')"
  lrx="$(printf '%s\n' "$lr_line" | sed -E 's/.*\(([[:space:]]*[-0-9.]+),([[:space:]]*[-0-9.]+)\).*/\1/' | tr -d ' ')"
  lry="$(printf '%s\n' "$lr_line" | sed -E 's/.*\(([[:space:]]*[-0-9.]+),([[:space:]]*[-0-9.]+)\).*/\2/' | tr -d ' ')"

  keep="$(intersects "$ulx" "$uly" "$lrx" "$lry" "$WEST" "$SOUTH" "$EAST" "$NORTH")"

  printf '%s\n' "$tif"
  printf '  UL=(%s,%s)  LR=(%s,%s)\n' "$ulx" "$uly" "$lrx" "$lry"

  if [ "$keep" = "1" ]; then
    cp -f "$tif" "$SELECT_DIR/"
    echo "  -> selected"
    selected_count=$((selected_count + 1))
  else
    echo "  -> skipped"
  fi
done

echo
echo "Selected $selected_count tiles"

mapfile -t selected_tifs < <(find "$SELECT_DIR" -maxdepth 1 -type f \( -iname "*.tif" -o -iname "*.tiff" \) | sort)

if [ ${#selected_tifs[@]} -eq 0 ]; then
  echo "Error: no tiles selected for target region" >&2
  exit 1
fi

echo
echo "== Building VRT =="
gdalbuildvrt "$VRT" "${selected_tifs[@]}"

echo
echo "== Warping to exact target extent/grid =="
gdalwarp \
  -te "$WEST" "$SOUTH" "$EAST" "$NORTH" \
  -tr 0.000833333333333 0.000833333333333 \
  -r bilinear \
  -srcnodata -32768 \
  -dstnodata -9999 \
  -ot Int16 \
  "$VRT" "$OUT"

echo
echo "== Final validation =="
gdalinfo "$OUT" | egrep "Size is|Origin =|Pixel Size =|Upper Left|Lower Left|Upper Right|Lower Right|NoData|Type=" || true

echo
echo "Done: $OUT"