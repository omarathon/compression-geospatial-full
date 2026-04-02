Source already on server — no download required.

Crop from global file using crop_lowres.sh:

    SRC=/maps/omsst2/diss/papers/rasterlite/ETOPO1/ETOPO1_Ice_g_geotiff.tif

    gdal_translate -q -projwin  -84.0 50.7  -64.0 30.7 "$SRC" etopo1/ETOPO1_nyc.tif
    gdal_translate -q -projwin -103.5 52.0  -83.5 32.0 "$SRC" etopo1/ETOPO1_iowa.tif
    gdal_translate -q -projwin   76.9 37.9   96.9 17.9 "$SRC" etopo1/ETOPO1_himalayas.tif
    gdal_translate -q -projwin   -8.0 34.0   12.0 14.0 "$SRC" etopo1/ETOPO1_sahara.tif
    gdal_translate -q -projwin  -70.0  7.0  -50.0 -13.0 "$SRC" etopo1/ETOPO1_amazon.tif
    gdal_translate -q -projwin  -40.0 55.0  -20.0 35.0 "$SRC" etopo1/ETOPO1_ocean.tif
