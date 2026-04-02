Download via Microsoft Planetary Computer STAC (collection: nasadem).
Handled by download_stac.py (--dataset nasadem).

    python3 download_stac.py --dataset nasadem --outdir .

The script queries the STAC catalog for all NASADEM tiles overlapping each region
bbox, signs the asset URLs, and downloads elevation.tif for each tile into
srtm_highres/SRTM_HighRes_{region}_{tile_id}.tif.

Each region typically requires 1–4 tiles (NASADEM tiles are 1°×1°). The files
are individual tiles; no mosaicing or cropping was needed since region bboxes
are 1°×1° and tile boundaries align to whole degrees.

No authentication required — Planetary Computer STAC is open access.
