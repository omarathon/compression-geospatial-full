run `download.py`

then:

ls raw/*_B02.tif | sed 's#raw/\(.*\)_B02.tif#\1#' | \
xargs -I{} -P 4 sh -c '
  gdalbuildvrt -separate "tmp_vrts/{}.vrt" \
    "raw/{}_B02.tif" \
    "raw/{}_B03.tif" \
    "raw/{}_B04.tif" \
    "raw/{}_B08.tif" >/dev/null &&
  gdal_translate "tmp_vrts/{}.vrt" "final/{}.tif" \
    -of COG \
    -co BLOCKSIZE=512 \
    -co COMPRESS=LERC_ZSTD >/dev/null
'

