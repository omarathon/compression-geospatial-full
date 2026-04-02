# Plan: Download Benchmark Datasets Across 6 Diverse Regions

## Context
For compression benchmarking, we need samples of 8 datasets across 6 terrain types. Each download is a single scene/tile/crop — no mosaicing needed.

## Diversity Evaluation

**Data types covered**: Int16, UInt16, UInt32, UInt8 (byte).
**Resolution tiers**: 10m (Sentinel-2, WorldCover), 15m (Landsat B8), 30m (Landsat B1/B4/B5), ~90m (SRTM), ~1km (WorldClim/LandScan), ~1.86km (ETOPO1).
**Terrain types**: urban, agriculture, mountains, desert, tropical forest, ocean.
**Value distributions**: categorical (WorldCover, ~11 unique values), sparse/heavy-tailed (LandScan), smooth continuous (temperature, elevation), bounded positive (precipitation), spectral (satellite bands).

**Gap**: No float32/float64 — could add ERA5 or MODIS LST later if needed.

## Ocean Region

Only **ETOPO1** has data over the open North Atlantic (~45°N, 30°W). All others are excluded:
- SRTM, WorldClim, LandScan, WorldCover: land-only
- Landsat, Sentinel-2: do not routinely image open ocean

## Regions

All coordinates WGS84 (EPSG:4326). Bounding boxes as `[west, south, east, north]`.

| Region | Centre | 1°×1° bbox (high-res: Landsat, Sentinel-2, WorldCover, SRTM) | 20°×20° bbox (low-res: WorldClim, LandScan, ETOPO1) |
|--------|--------|-------------------------------------------------------------|------------------------------------------------------|
| NYC (urban) | 40.7°N, 74.0°W | [-74.5, 40.2, -73.5, 41.2] | [-84.0, 30.7, -64.0, 50.7] |
| Iowa (agriculture) | 42.0°N, 93.5°W | [-94.0, 41.5, -93.0, 42.5] | [-103.5, 32.0, -83.5, 52.0] |
| Himalayas (mountains) | 27.9°N, 86.9°E | [86.4, 27.4, 87.4, 28.4] | [76.9, 17.9, 96.9, 37.9] |
| Sahara (desert) | 24.0°N, 2.0°E | [1.5, 23.5, 2.5, 24.5] | [-8.0, 14.0, 12.0, 34.0] |
| Amazon (tropical forest) | 3.0°S, 60.0°W | [-60.5, -3.5, -59.5, -2.5] | [-70.0, -13.0, -50.0, 7.0] |
| North Atlantic (ocean) | 45.0°N, 30.0°W | — | [-40.0, 35.0, -20.0, 55.0] |

**Note on file sizes**: Low-res datasets (~1km) at 20°×20° produce small files (12-23 MB). That's inherent to the coarse resolution — a 20°×20° region is only 2400×2400 pixels at 1km. This is fine for benchmarking.

---

## Dataset 1: LandScan

| Property | Value |
|----------|-------|
| Data type | UInt32 |
| Resolution | 30 arc-seconds (~1 km) |
| Source | ORNL DAAC |
| Auth | **NASA Earthdata login** — one-time registration from local browser, then fully headless |
| Regions | 5 (skip ocean) |
| Est. size/region | ~23 MB (20°×20° = 2400×2400 × 4 bytes) |

**Setup** (one-time, from your local machine's browser):
1. Register at https://urs.earthdata.nasa.gov/ (free)
2. On the server, create `~/.netrc`:
   ```
   machine urs.earthdata.nasa.gov login YOUR_USERNAME password YOUR_PASSWORD
   ```
3. `chmod 600 ~/.netrc`

**Download**: Get the LandScan Global download URL from https://landscan.ornl.gov/, then:
```bash
wget --auth-no-challenge \
     --load-cookies ~/.cookies --save-cookies ~/.cookies \
     -O landscan_global.tif \
     "DOWNLOAD_URL_HERE"
```

**Crop** (20°×20°):
```bash
gdal_translate -projwin -84.0 50.7 -64.0 30.7 landscan_global.tif LandScan_nyc.tif
gdal_translate -projwin -103.5 52.0 -83.5 32.0 landscan_global.tif LandScan_iowa.tif
gdal_translate -projwin 76.9 37.9 96.9 17.9 landscan_global.tif LandScan_himalayas.tif
gdal_translate -projwin -8.0 34.0 12.0 14.0 landscan_global.tif LandScan_sahara.tif
gdal_translate -projwin -70.0 7.0 -50.0 -13.0 landscan_global.tif LandScan_amazon.tif
```

**Note**: Very sparse — most pixels are 0. If ORNL requires browser-only download: register locally, copy the direct download URL, wget on server.

---

## Dataset 2: WorldClim 1.4 Temperature

| Property | Value |
|----------|-------|
| Data type | Int16 (°C × 10) |
| Resolution | 30 arc-seconds (~1 km) |
| Source | https://biogeo.ucdavis.edu/data/worldclim/v1.4/grid/cur/ |
| Auth | None |
| Headless | Yes — direct wget |
| Regions | 5 (skip ocean) |
| Est. size/region | ~12 MB (20°×20° = 2400×2400 × 2 bytes) |

```bash
wget https://biogeo.ucdavis.edu/data/worldclim/v1.4/grid/cur/tmean_30s_bil.zip
unzip tmean_30s_bil.zip
# Produces tmean1.bil through tmean12.bil (GDAL reads BIL directly)

# Crop January (tmean1) to 20°×20°:
gdal_translate -projwin -84.0 50.7 -64.0 30.7 tmean1.bil WorldClim_temp_nyc.tif
gdal_translate -projwin -103.5 52.0 -83.5 32.0 tmean1.bil WorldClim_temp_iowa.tif
gdal_translate -projwin 76.9 37.9 96.9 17.9 tmean1.bil WorldClim_temp_himalayas.tif
gdal_translate -projwin -8.0 34.0 12.0 14.0 tmean1.bil WorldClim_temp_sahara.tif
gdal_translate -projwin -70.0 7.0 -50.0 -13.0 tmean1.bil WorldClim_temp_amazon.tif
```

**Note**: If v1.4 server is down, use WorldClim v2.1 — GeoTIFF format, but float64. Convert: `gdal_translate -ot Int16 -scale`.

---

## Dataset 3: WorldClim Precipitation

| Property | Value |
|----------|-------|
| Data type | Int16 (mm) |
| Resolution | 30 arc-seconds (~1 km) |
| Source | Same as temperature |
| Auth | None |
| Headless | Yes — direct wget |
| Regions | 5 (skip ocean) |
| Est. size/region | ~12 MB |

```bash
wget https://biogeo.ucdavis.edu/data/worldclim/v1.4/grid/cur/prec_30s_bil.zip
unzip prec_30s_bil.zip

gdal_translate -projwin -84.0 50.7 -64.0 30.7 prec1.bil WorldClim_prec_nyc.tif
gdal_translate -projwin -103.5 52.0 -83.5 32.0 prec1.bil WorldClim_prec_iowa.tif
gdal_translate -projwin 76.9 37.9 96.9 17.9 prec1.bil WorldClim_prec_himalayas.tif
gdal_translate -projwin -8.0 34.0 12.0 14.0 prec1.bil WorldClim_prec_sahara.tif
gdal_translate -projwin -70.0 7.0 -50.0 -13.0 prec1.bil WorldClim_prec_amazon.tif
```

**Note**: Precipitation is **Int16** (not uint16/uint32). Spikier distribution than temperature.

---

## Dataset 4: SRTM

| Property | Value |
|----------|-------|
| Data type | Int16 |
| Resolution | 3 arc-seconds (~90m) — matches existing SRTM Italy |
| Source | https://srtm.csi.cgiar.org/wp-content/uploads/files/srtm_5x5/TIFF/ |
| Auth | None |
| Headless | Yes — direct wget |
| Regions | 5 (skip ocean) |
| Est. size/region | ~2.9 MB (1°×1° = 1200×1200 × 2 bytes) |

Tile index: `XX = floor((lon + 180) / 5) + 1`, `YY = floor((60 - lat) / 5) + 1`

| Region | Tile ID |
|--------|---------|
| NYC | srtm_22_04 |
| Iowa | srtm_18_04 |
| Himalayas | srtm_54_07 |
| Sahara | srtm_37_07 |
| Amazon | srtm_24_13 |

```bash
for tile in srtm_22_04 srtm_18_04 srtm_54_07 srtm_37_07 srtm_24_13; do
    wget -nc "https://srtm.csi.cgiar.org/wp-content/uploads/files/srtm_5x5/TIFF/${tile}.zip"
    unzip -o "${tile}.zip"
done

# Crop to 1°×1°:
gdal_translate -projwin -74.5 41.2 -73.5 40.2 srtm_22_04/srtm_22_04.tif SRTM_nyc.tif
gdal_translate -projwin -94.0 42.5 -93.0 41.5 srtm_18_04/srtm_18_04.tif SRTM_iowa.tif
gdal_translate -projwin 86.4 28.4 87.4 27.4 srtm_54_07/srtm_54_07.tif SRTM_himalayas.tif
gdal_translate -projwin 1.5 24.5 2.5 23.5 srtm_37_07/srtm_37_07.tif SRTM_sahara.tif
gdal_translate -projwin -60.5 -2.5 -59.5 -3.5 srtm_24_13/srtm_24_13.tif SRTM_amazon.tif
```

---

## Dataset 4b: SRTM_HighRes

| Property | Value |
|----------|-------|
| Data type | Int16 |
| Resolution | 1 arc-second (~30m) |
| Source | Planetary Computer STAC (`nasadem`) or NASA Earthdata |
| Auth | None (Planetary Computer) |
| Headless | Yes — STAC API + Python |
| Regions | 5 (skip ocean) |
| Est. size/region | ~26 MB (1°×1° = 3600×3600 × 2 bytes) |

**Note**: NASADEM on Planetary Computer is the void-filled SRTM 1 arc-second product. Same underlying data as SRTM but at 3× the resolution per axis (9× pixels).

```python
from pystac_client import Client
import planetary_computer, requests, os

catalog = Client.open("https://planetarycomputer.microsoft.com/api/stac/v1")

regions = {
    "nyc":       [-74.5, 40.2, -73.5, 41.2],
    "iowa":      [-94.0, 41.5, -93.0, 42.5],
    "himalayas": [86.4, 27.4, 87.4, 28.4],
    "sahara":    [1.5, 23.5, 2.5, 24.5],
    "amazon":    [-60.5, -3.5, -59.5, -2.5],
}

for name, bbox in regions.items():
    search = catalog.search(
        collections=["nasadem"],
        bbox=bbox,
    )
    items = list(search.get_items())
    print(f"{name}: {len(items)} tile(s)")
    for item in items:
        signed = planetary_computer.sign(item)
        url = signed.assets["elevation"].href
        out = f"SRTM_HighRes_{name}_{item.id}.tif"
        if os.path.exists(out): continue
        r = requests.get(url, stream=True); r.raise_for_status()
        with open(out, "wb") as f:
            for chunk in r.iter_content(1024*1024):
                if chunk: f.write(chunk)
```

Then crop to 1°×1°:
```bash
gdal_translate -projwin -74.5 41.2 -73.5 40.2 <tile>.tif SRTM_HighRes_nyc.tif
gdal_translate -projwin -94.0 42.5 -93.0 41.5 <tile>.tif SRTM_HighRes_iowa.tif
gdal_translate -projwin 86.4 28.4 87.4 27.4 <tile>.tif SRTM_HighRes_himalayas.tif
gdal_translate -projwin 1.5 24.5 2.5 23.5 <tile>.tif SRTM_HighRes_sahara.tif
gdal_translate -projwin -60.5 -2.5 -59.5 -3.5 <tile>.tif SRTM_HighRes_amazon.tif
```

---

## Dataset 5: ETOPO1

| Property | Value |
|----------|-------|
| Data type | Int16 |
| Resolution | 1 arc-minute (~1.86 km) — matches existing ETOPO1 |
| Source | Already on server |
| Regions | **6 (including ocean)** — only dataset with ocean coverage |
| Est. size/region | ~3 MB (20°×20° = 1200×1200 × 2 bytes) |

```bash
SRC=/maps/omsst2/diss/papers/rasterlite/ETOPO1/ETOPO1_Ice_g_geotiff.tif

gdal_translate -projwin -84.0 50.7 -64.0 30.7 "$SRC" ETOPO1_nyc.tif
gdal_translate -projwin -103.5 52.0 -83.5 32.0 "$SRC" ETOPO1_iowa.tif
gdal_translate -projwin 76.9 37.9 96.9 17.9 "$SRC" ETOPO1_himalayas.tif
gdal_translate -projwin -8.0 34.0 12.0 14.0 "$SRC" ETOPO1_sahara.tif
gdal_translate -projwin -70.0 7.0 -50.0 -13.0 "$SRC" ETOPO1_amazon.tif
gdal_translate -projwin -40.0 55.0 -20.0 35.0 "$SRC" ETOPO1_ocean.tif
```

---

## Dataset 5b: ETOPO_HighRes

| Property | Value |
|----------|-------|
| Data type | Int16 |
| Resolution | 15 arc-seconds (~450m) |
| Source | NOAA — direct download |
| Auth | None |
| Headless | Yes — direct wget |
| Regions | **6 (including ocean)** |
| Est. size/region | ~46 MB (20°×20° = 4800×4800 × 2 bytes) |

**Note**: ETOPO 2022 at 15 arc-second — 4× resolution per axis vs ETOPO1 (16× pixels). Ice surface variant.

```bash
wget -nc https://www.ngdc.noaa.gov/mgg/global/relief/ETOPO2022/data/15s/15s_surface_elev_gtif/ETOPO_2022_v1_15s_N90W180_surface.tif

SRC=ETOPO_2022_v1_15s_N90W180_surface.tif

gdal_translate -projwin -84.0 50.7 -64.0 30.7 "$SRC" ETOPO_HighRes_nyc.tif
gdal_translate -projwin -103.5 52.0 -83.5 32.0 "$SRC" ETOPO_HighRes_iowa.tif
gdal_translate -projwin 76.9 37.9 96.9 17.9 "$SRC" ETOPO_HighRes_himalayas.tif
gdal_translate -projwin -8.0 34.0 12.0 14.0 "$SRC" ETOPO_HighRes_sahara.tif
gdal_translate -projwin -70.0 7.0 -50.0 -13.0 "$SRC" ETOPO_HighRes_amazon.tif
gdal_translate -projwin -40.0 55.0 -20.0 35.0 "$SRC" ETOPO_HighRes_ocean.tif
```

---

## Dataset 6: Landsat 8 (Bands 1, 2, 3, 4, 5, 8)

| Property | Value |
|----------|-------|
| Data type | UInt16 |
| Resolution | 30m (B1, B2, B3, B4, B5) and 15m (B8 panchromatic) — mixed resolution, downloaded as separate files |
| Source | Planetary Computer STAC (`landsat-c2-l2`) |
| Auth | None |
| Headless | Yes — STAC API + Python |
| Regions | 5 (skip ocean) |
| Est. size/region | ~243 MB (5 × ~27 MB at 30m + 1 × ~108 MB at 15m) |

**Note**: B8 (panchromatic) is 15m resolution — 4× the pixels of the 30m bands. B2/B3/B4 = Blue/Green/Red (RGB). Each band is downloaded as a separate TIF.

```python
from pystac_client import Client
import planetary_computer, requests, os

catalog = Client.open("https://planetarycomputer.microsoft.com/api/stac/v1")

regions = {
    "nyc":       [-74.5, 40.2, -73.5, 41.2],
    "iowa":      [-94.0, 41.5, -93.0, 42.5],
    "himalayas": [86.4, 27.4, 87.4, 28.4],
    "sahara":    [1.5, 23.5, 2.5, 24.5],
    "amazon":    [-60.5, -3.5, -59.5, -2.5],
}

# Asset keys — check item.assets.keys() on first run to confirm
bands = {"coastal": "B1", "blue": "B2", "green": "B3", "red": "B4", "nir08": "B5", "pan": "B8"}

for name, bbox in regions.items():
    search = catalog.search(
        collections=["landsat-c2-l2"],
        bbox=bbox,
        datetime="2024-01-01/2024-12-31",
        query={"eo:cloud_cover": {"lt": 10}},
    )
    item = min(search.get_items(), key=lambda x: x.properties["eo:cloud_cover"])
    signed = planetary_computer.sign(item)
    print(f"{name}: {item.id}")
    print(f"  Assets: {list(item.assets.keys())}")

    for asset_key, band_name in bands.items():
        url = signed.assets[asset_key].href
        out = f"Landsat8_{band_name}_{name}.tif"
        if os.path.exists(out): continue
        r = requests.get(url, stream=True); r.raise_for_status()
        with open(out, "wb") as f:
            for chunk in r.iter_content(1024*1024):
                if chunk: f.write(chunk)
```

---

## Dataset 7: Sentinel-2 (Bands B02, B03, B04, B08)

| Property | Value |
|----------|-------|
| Data type | UInt16 |
| Resolution | 10m — matches existing Sentinel-2 |
| Source | Planetary Computer STAC (`sentinel-2-l2a`) |
| Auth | None |
| Headless | Yes — STAC API + Python |
| Regions | 5 (skip ocean) |
| Est. size/region | ~241 MB per band, ~964 MB for 4 bands |

```python
from pystac_client import Client
import planetary_computer, requests, os

catalog = Client.open("https://planetarycomputer.microsoft.com/api/stac/v1")

regions = {
    "nyc":       [-74.5, 40.2, -73.5, 41.2],
    "iowa":      [-94.0, 41.5, -93.0, 42.5],
    "himalayas": [86.4, 27.4, 87.4, 28.4],
    "sahara":    [1.5, 23.5, 2.5, 24.5],
    "amazon":    [-60.5, -3.5, -59.5, -2.5],
}

bands = ["B02", "B03", "B04", "B08"]

for name, bbox in regions.items():
    search = catalog.search(
        collections=["sentinel-2-l2a"],
        bbox=bbox,
        datetime="2024-01-01/2024-12-31",
        query={"eo:cloud_cover": {"lt": 10}},
    )
    item = min(search.get_items(), key=lambda x: x.properties["eo:cloud_cover"])
    signed = planetary_computer.sign(item)
    print(f"{name}: {item.id}")

    for band in bands:
        url = signed.assets[band].href
        out = f"Sentinel2_{band}_{name}.tif"
        if os.path.exists(out): continue
        r = requests.get(url, stream=True); r.raise_for_status()
        with open(out, "wb") as f:
            for chunk in r.iter_content(1024*1024):
                if chunk: f.write(chunk)
```

---

## Dataset 8: ESA WorldCover

| Property | Value |
|----------|-------|
| Data type | UInt8 (byte) |
| Resolution | 10m |
| Source | Planetary Computer STAC (`esa-worldcover`) |
| Auth | None |
| Headless | Yes — STAC API + Python |
| Regions | 5 (skip ocean) |
| Est. size/region | ~117 MB (1°×1° = 10800×10800 × 1 byte) |

Tiles are 3°×3° — download tile, then crop to 1°×1°.

```python
from pystac_client import Client
import planetary_computer, requests, os

catalog = Client.open("https://planetarycomputer.microsoft.com/api/stac/v1")

regions = {
    "nyc":       [-74.5, 40.2, -73.5, 41.2],
    "iowa":      [-94.0, 41.5, -93.0, 42.5],
    "himalayas": [86.4, 27.4, 87.4, 28.4],
    "sahara":    [1.5, 23.5, 2.5, 24.5],
    "amazon":    [-60.5, -3.5, -59.5, -2.5],
}

for name, bbox in regions.items():
    search = catalog.search(
        collections=["esa-worldcover"],
        bbox=bbox,
        datetime="2021",
    )
    items = list(search.get_items())
    print(f"{name}: {len(items)} tile(s)")
    for item in items:
        signed = planetary_computer.sign(item)
        url = signed.assets["map"].href
        out = f"WorldCover_{name}_{item.id}.tif"
        if os.path.exists(out): continue
        r = requests.get(url, stream=True); r.raise_for_status()
        with open(out, "wb") as f:
            for chunk in r.iter_content(1024*1024):
                if chunk: f.write(chunk)
```

Then crop to 1°×1°:
```bash
gdal_translate -projwin $WEST $NORTH $EAST $SOUTH WorldCover_raw.tif WorldCover_${region}.tif
```

**Note**: ~11 unique values. Categorical data compresses fundamentally differently from continuous.

---

## Size Summary

| Dataset | Type | Res | Per-region | Regions | Total | Download |
|---------|------|-----|------------|---------|-------|----------|
| LandScan | UInt32 | ~1km | ~23 MB | 5 | ~115 MB | Earthdata (one-time reg) |
| WorldClim temp | Int16 | ~1km | ~12 MB | 5 | ~60 MB | wget (no auth) |
| WorldClim precip | Int16 | ~1km | ~12 MB | 5 | ~60 MB | wget (no auth) |
| SRTM | Int16 | ~90m | ~2.9 MB | 5 | ~15 MB | wget (no auth) |
| SRTM_HighRes | Int16 | ~30m | ~26 MB | 5 | ~130 MB | STAC API (headless) |
| ETOPO1 | Int16 | ~1.86km | ~3 MB | 6 | ~18 MB | already on server |
| ETOPO_HighRes | Int16 | ~450m | ~46 MB | 6 | ~276 MB | wget (no auth) |
| Landsat 8 (6 bands) | UInt16 | 30m/15m | ~243 MB | 5 | ~1.2 GB | STAC API (headless) |
| Sentinel-2 (4 bands) | UInt16 | 10m | ~964 MB | 5 | ~4.8 GB | STAC API (headless) |
| ESA WorldCover | UInt8 | 10m | ~117 MB | 5 | ~585 MB | STAC API (headless) |
| **Total** | | | | | **~7.4 GB** | |

## Files to create
- `datasets/new/download_stac.py` — unified STAC downloader for Landsat, Sentinel-2, and WorldCover across all regions
- `datasets/new/download_srtm_worldclim.sh` — wget commands for SRTM tiles and WorldClim global files + crops
- `datasets/new/crop_lowres.sh` — gdal_translate commands to crop ETOPO1, WorldClim, LandScan
- `datasets/new/download_instructions.md` — documents one-time Earthdata registration for LandScan

## Verification
After downloading, run `visualize.py` on each output file and inspect the collages for corruption.
