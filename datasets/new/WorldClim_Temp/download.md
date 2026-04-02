Download via remote crop using GDAL /vsizip//vsicurl/ (no full archive downloaded).
Then convert Float32 → Int16 with ×10 scaling.
Handled by download_srtm_worldclim.sh + manual conversion step.

Step 1 — crop (produces Float32 files):

    src="/vsizip//vsicurl/https://geodata.ucdavis.edu/climate/worldclim/2_1/base/wc2.1_30s_tavg.zip/wc2.1_30s_tavg_01.tif"

    gdal_translate -q -projwin  -84.0 50.7  -64.0 30.7 "$src" worldclim/WorldClim_temp_nyc.tif
    gdal_translate -q -projwin -103.5 52.0  -83.5 32.0 "$src" worldclim/WorldClim_temp_iowa.tif
    gdal_translate -q -projwin   76.9 37.9   96.9 17.9 "$src" worldclim/WorldClim_temp_himalayas.tif
    gdal_translate -q -projwin   -8.0 34.0   12.0 14.0 "$src" worldclim/WorldClim_temp_sahara.tif
    gdal_translate -q -projwin  -70.0  7.0  -50.0 -13.0 "$src" worldclim/WorldClim_temp_amazon.tif

Step 2 — convert Float32 → Int16 (×10, nodata-safe):

    for region in nyc iowa himalayas sahara amazon; do
        src="worldclim/WorldClim_temp_${region}.tif"
        tmp="worldclim/WorldClim_temp_${region}_int16.tif"
        # Get Float32 nodata value:
        nd=$(gdalinfo "$src" | grep -oP 'NoData Value=\K[^ ]+')
        gdal_translate -ot Int16 \
            -a_nodata -32768 \
            -scale -999 999 -9990 9990 \
            "$src" "$tmp"
        mv "$tmp" "$src"
    done

NOTE — data type:
WorldClim v2.1 stores temperature as Float32 actual °C (unlike v1.4 which was
Int16 °C×10). The ×10 conversion preserves all scientific precision — WorldClim's
stated accuracy is ~0.1°C. Range after conversion: approximately -600 to +500
(−60.0°C to +50.0°C), well within Int16 range (−32767 to 32767).
