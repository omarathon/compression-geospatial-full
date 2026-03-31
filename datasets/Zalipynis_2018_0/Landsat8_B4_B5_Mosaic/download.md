download_landsat_band_mosaic.sh B4
download_landsat_band_mosaic.sh B5

gdalwarp \
  -t_srs EPSG:32632 \
  -tr 30 30 \
  -tap \
  -r near \
  -dstnodata 65535 \
  -ot UInt16 \
  *_B4.TIF \
  landsat8_mosaic_B4.tif

gdalwarp \
  -t_srs EPSG:32632 \
  -tr 30 30 \
  -tap \
  -r near \
  -dstnodata 65535 \
  -ot UInt16 \
  *_B5.TIF \
  landsat8_mosaic_B5.tif