Download via Microsoft Planetary Computer STAC (collection: sentinel-2-l2a).
Handled by download_stac.py (--dataset sentinel2).

    python3 download_stac.py --dataset sentinel2 --outdir .

The script selects the lowest cloud-cover scene in 2024 for each region bbox,
signs the asset URLs via planetary_computer.sign(), and downloads B02, B03, B04,
B08 GeoTIFFs into sentinel2/Sentinel2_{band}_{region}.tif.

Output: 20 files (5 regions × 4 bands).
Each file is a full 10980×10980 Sentinel-2 tile (~110 km × 110 km).
No authentication required — Planetary Computer STAC is open access.
