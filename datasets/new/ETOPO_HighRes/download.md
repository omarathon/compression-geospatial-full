Download tiled GeoTIFFs from NOAA NGDC and build a global VRT, then crop.
Handled by download_srtm_worldclim.sh.

    BASE_URL="https://www.ngdc.noaa.gov/mgg/global/relief/ETOPO2022/data/15s/15s_surface_elev_gtif"
    ETOPO_VRT="etopo_highres/ETOPO_2022_v1_15s_surface_global.vrt"

    wget -q -r -np -nd \
        -A "ETOPO_2022_v1_15s_*_surface.tif" \
        -R "index.html*" \
        -P etopo_highres/tiles \
        "$BASE_URL/"

    gdalbuildvrt "$ETOPO_VRT" etopo_highres/tiles/ETOPO_2022_v1_15s_*_surface.tif

    gdal_translate -q -projwin  -84.0 50.7  -64.0 30.7 "$ETOPO_VRT" etopo_highres/ETOPO_HighRes_nyc.tif
    gdal_translate -q -projwin -103.5 52.0  -83.5 32.0 "$ETOPO_VRT" etopo_highres/ETOPO_HighRes_iowa.tif
    gdal_translate -q -projwin   76.9 37.9   96.9 17.9 "$ETOPO_VRT" etopo_highres/ETOPO_HighRes_himalayas.tif
    gdal_translate -q -projwin   -8.0 34.0   12.0 14.0 "$ETOPO_VRT" etopo_highres/ETOPO_HighRes_sahara.tif
    gdal_translate -q -projwin  -70.0  7.0  -50.0 -13.0 "$ETOPO_VRT" etopo_highres/ETOPO_HighRes_amazon.tif
    gdal_translate -q -projwin  -40.0 55.0  -20.0 35.0 "$ETOPO_VRT" etopo_highres/ETOPO_HighRes_ocean.tif

NOTE — Int32 quantization (×1000):
The raw tiles are Float32. gdal_translate -scale did NOT apply the ×1000 factor
correctly (values remained in float metres despite the -ot Int32 flag). Use
gdal_calc.py for reliable fixed-point conversion:

    mkdir -p etopo_highres_quant
    for region in nyc iowa himalayas sahara amazon ocean; do
        gdal_calc.py -A etopo_highres/ETOPO_HighRes_${region}.tif \
            --calc="(A*1000).astype(int)" \
            --outfile=etopo_highres_quant/ETOPO_HighRes_${region}.tif \
            --type=Int32 --NoDataValue=-2147483647 --overwrite
    done

Quantization factor derivation: worst-case elevation ~11,000 m; Float32 ULP at
11,000 m ≈ 0.001 m (1 mm). Factor ×1000 gives millimetre precision, max stored
value = 11,000,000 which fits within 2^24 = 16,777,216 (user constraint). Int32
range is ±2^31, so no overflow.
