Download via Microsoft Planetary Computer STAC (collection: esa-worldcover).
Handled by download_stac.py (--dataset worldcover).

    python3 download_stac.py --dataset worldcover --outdir .

The script queries for all ESA WorldCover 2021 tiles overlapping each region
bbox, signs asset URLs, and downloads the "map" asset into
worldcover/WorldCover_{region}_{tile_id}.tif.

Each tile covers 3°×3° at 10 m (36000×36000 pixels). Each region typically
intersects exactly 1 tile. Files are used as-is (no cropping to 1°×1°).

No authentication required — Planetary Computer STAC is open access.
