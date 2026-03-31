wget -O DTM_Orografico.7z \
  "https://www502.regione.toscana.it/geoscopio/download/altimetria/da_ctr10k/gb/DTM_Orografico.7z"
wget https://www.7-zip.org/a/7z2408-linux-x64.tar.xz
tar -xf 7z2408-linux-x64.tar.xz
./7zz x DTM_Orografico.7z
gdal_translate dtmoro.asc output.tif \
  -co COMPRESS=ZSTD \
  -co TILED=YES \
  -co PREDICTOR=3

then quantized using quantization script `quantize_int32.sh` with 256 and 2000 multiplier based on recommendations.

measured error with `quant_error_stats.sh`