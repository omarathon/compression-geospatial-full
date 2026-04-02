Download from GCS public Landsat bucket via Planetary Computer STAC metadata + gsutil.
Handled by download_stac.py (--dataset landsat).

    python3 download_stac.py --dataset landsat --outdir .

The script:
  1. Queries Planetary Computer STAC (landsat-c2-l2) for the lowest cloud-cover
     LC08 scene in 2021 for each region bbox.
  2. Resolves the WRS path/row and acquisition date from STAC metadata.
  3. Uses gsutil to list matching scene files in gs://gcp-public-data-landsat/LC08/01/...
  4. Downloads B1, B2, B3, B4, B5, B8 via HTTPS:
     https://storage.googleapis.com/gcp-public-data-landsat/LC08/01/{path}/{row}/{scene_id}/{scene_id}_{band}.TIF

Output: landsat8/Landsat8_{band}_{region}.tif  (30 files total, 5 regions × 6 bands)

Prerequisites: gsutil installed (part of Google Cloud SDK), no GCS auth needed
(bucket is public read).
