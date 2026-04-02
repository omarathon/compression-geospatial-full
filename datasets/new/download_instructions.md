# Download Instructions

## Run Order

1. **ETOPO1 crops** (already on server — instant):
   ```bash
   bash crop_lowres.sh
   ```

2. **SRTM + WorldClim + ETOPO_HighRes** (wget, no auth):
   ```bash
   bash download_srtm_worldclim.sh
   ```

3. **Landsat + Sentinel-2 + WorldCover + SRTM_HighRes** (STAC API, no auth):
   ```bash
   python3 download_stac.py
   ```

4. **LandScan** (requires Earthdata registration — see below):
   ```bash
   # After placing landscan_global.tif in landscan/
   bash crop_lowres.sh
   ```

## LandScan — NASA Earthdata Registration

LandScan requires a free NASA Earthdata account. This is the only dataset that needs authentication.

### One-time setup (from your local browser)

1. Register at https://urs.earthdata.nasa.gov/ (free, instant)
2. Go to https://landscan.ornl.gov/ and download the latest LandScan Global dataset
   - You may need to accept a data use agreement
   - Copy the direct download URL from your browser's download manager

### On the server

1. Create `~/.netrc`:
   ```
   machine urs.earthdata.nasa.gov login YOUR_USERNAME password YOUR_PASSWORD
   ```

2. Set permissions:
   ```bash
   chmod 600 ~/.netrc
   ```

3. Download using the URL from step 2 above:
   ```bash
   wget --auth-no-challenge \
        --load-cookies ~/.cookies --save-cookies ~/.cookies \
        -O landscan/landscan_global.tif \
        "PASTE_DOWNLOAD_URL_HERE"
   ```

4. Run `bash crop_lowres.sh` to crop to the 5 benchmark regions.

**Alternative**: If ORNL requires a browser-only download, download on your local machine and `scp` to the server:
```bash
scp landscan_global.tif server:path/to/datasets/new/landscan/
```

## Verification

After all downloads complete, run the visualisation script on each output to check for corruption:

```bash
for f in etopo1/*.tif srtm/*.tif worldclim/*.tif etopo_highres/*.tif \
         landscan/*.tif landsat8/*.tif sentinel2/*.tif worldcover/*.tif \
         srtm_highres/*.tif; do
    python3 ../visualize.py "$f" "vis/$(basename "${f%.tif}").png"
done
```
