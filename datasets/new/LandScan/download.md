Requires NASA Earthdata account. One-time registration from a browser, then headless.

Step 1 — register (one-time, from local browser):
    https://urs.earthdata.nasa.gov/  (free account)

Step 2 — on the server, create ~/.netrc:
    machine urs.earthdata.nasa.gov login YOUR_USERNAME password YOUR_PASSWORD

    chmod 600 ~/.netrc

Step 3 — get the download URL from https://landscan.ornl.gov/
    (log in, navigate to LandScan Global 2024, copy the direct .tif download link)

Step 4 — download global file:
    wget --auth-no-challenge \
         --load-cookies ~/.cookies --save-cookies ~/.cookies \
         -O landscan/landscan-global-2024.tif \
         "DOWNLOAD_URL_FROM_LANDSCAN_WEBSITE"

Step 5 — crop (handled by crop_lowres.sh):
    LANDSCAN_SRC="landscan/landscan-global-2024.tif"

    gdal_translate -q -projwin  -84.0 50.7  -64.0 30.7 "$LANDSCAN_SRC" landscan/LandScan_nyc.tif
    gdal_translate -q -projwin -103.5 52.0  -83.5 32.0 "$LANDSCAN_SRC" landscan/LandScan_iowa.tif
    gdal_translate -q -projwin   76.9 37.9   96.9 17.9 "$LANDSCAN_SRC" landscan/LandScan_himalayas.tif
    gdal_translate -q -projwin   -8.0 34.0   12.0 14.0 "$LANDSCAN_SRC" landscan/LandScan_sahara.tif
    gdal_translate -q -projwin  -70.0  7.0  -50.0 -13.0 "$LANDSCAN_SRC" landscan/LandScan_amazon.tif
