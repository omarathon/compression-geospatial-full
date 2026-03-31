download.sh

gdalwarp \
  -t_srs EPSG:32631 \
  -tr 30 30 \
  -tap \
  -r near \
  -dstnodata 65535 \
  -ot UInt16 \
  *_B1.TIF \
  landsat8_path190_row031_stack.tif