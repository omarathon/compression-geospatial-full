run `download_landsat_b4_mosaic.sh`

gdalwarp \
  -t_srs EPSG:32631 \
  -tr 30 30 \
  -tap \
  -r near \
  -dstnodata 65535 \
  -ot UInt16 \
  *_B4.TIF \
  landsat8_mosaic_B4.tif