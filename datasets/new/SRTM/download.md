Download CGIAR 5°×5° tiles and crop to 1°×1°.
Handled by download_srtm_worldclim.sh.

    SRTM_BASE="https://srtm.csi.cgiar.org/wp-content/uploads/files/srtm_5x5/TIFF"

    for tile in srtm_22_04 srtm_18_04 srtm_54_07 srtm_37_08 srtm_24_13; do
        wget -q -nc -O "srtm/${tile}.zip" "${SRTM_BASE}/${tile}.zip"
        unzip -q -o "srtm/${tile}.zip" -d srtm/
    done

    gdal_translate -q -projwin -74.5 41.2 -73.5 40.2 srtm/srtm_22_04.tif srtm/SRTM_nyc.tif
    gdal_translate -q -projwin -94.0 42.5 -93.0 41.5 srtm/srtm_18_04.tif srtm/SRTM_iowa.tif
    gdal_translate -q -projwin  86.4 28.4  87.4 27.4 srtm/srtm_54_07.tif srtm/SRTM_himalayas.tif
    gdal_translate -q -projwin   1.5 24.5   2.5 23.5 srtm/srtm_37_08.tif srtm/SRTM_sahara.tif
    gdal_translate -q -projwin -61.0 -2.5 -60.0 -3.5 srtm/srtm_24_13.tif srtm/SRTM_amazon.tif

FIX NOTES:
  - Sahara: original tile was srtm_37_07 (covers 25-30°N). Crop at 23.5-24.5°N
    falls in tile srtm_37_08 (covers 20-25°N). Tile formula: YY = floor((60-lat)/5)+1;
    for lat=24, YY=8. Changed to srtm_37_08 → 100% valid data.
  - Amazon: original crop [-60.5, -59.5°] straddled the CGIAR tile boundary at -60°.
    Tile srtm_24_13 only covers [-65°, -60°], leaving the eastern half as nodata.
    Shifted crop 0.5° west to [-61.0, -60.0°], staying entirely within srtm_24_13.
    → 100% valid data, same terrain character (lowland tropical forest).
