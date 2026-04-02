Download via remote crop using GDAL /vsizip//vsicurl/ (no full archive downloaded).
Files are natively Int16 — no conversion needed.
Handled by download_srtm_worldclim.sh.

    src="/vsizip//vsicurl/https://geodata.ucdavis.edu/climate/worldclim/2_1/base/wc2.1_30s_prec.zip/wc2.1_30s_prec_01.tif"

    gdal_translate -q -projwin  -84.0 50.7  -64.0 30.7 "$src" worldclim/WorldClim_prec_nyc.tif
    gdal_translate -q -projwin -103.5 52.0  -83.5 32.0 "$src" worldclim/WorldClim_prec_iowa.tif
    gdal_translate -q -projwin   76.9 37.9   96.9 17.9 "$src" worldclim/WorldClim_prec_himalayas.tif
    gdal_translate -q -projwin   -8.0 34.0   12.0 14.0 "$src" worldclim/WorldClim_prec_sahara.tif
    gdal_translate -q -projwin  -70.0  7.0  -50.0 -13.0 "$src" worldclim/WorldClim_prec_amazon.tif

NOTE: Unlike WorldClim_Temp, precipitation remains Int16 in v2.1 (mm/month).
No post-processing required.
