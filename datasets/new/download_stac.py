#!/usr/bin/env python3
"""Unified STAC downloader for benchmark datasets.

Downloads Landsat 8, Sentinel-2, ESA WorldCover, and NASADEM (SRTM_HighRes)
across 5 geographic regions from Microsoft Planetary Computer.

Usage:
  python3 download_stac.py [--dataset DATASET] [--region REGION] [--outdir DIR]

Examples:
  python3 download_stac.py                          # download everything
  python3 download_stac.py --dataset landsat        # Landsat only
  python3 download_stac.py --region nyc             # NYC region only
  python3 download_stac.py --dataset sentinel2 --region himalayas
"""

import argparse
import os
import sys
import requests
from pystac_client import Client
import planetary_computer
import subprocess

CATALOG_URL = "https://planetarycomputer.microsoft.com/api/stac/v1"

REGIONS = {
    "nyc":       [-74.5, 40.2, -73.5, 41.2],
    "iowa":      [-94.0, 41.5, -93.0, 42.5],
    "himalayas": [86.4, 27.4, 87.4, 28.4],
    "sahara":    [1.5, 23.5, 2.5, 24.5],
    "amazon":    [-60.5, -3.5, -59.5, -2.5],
}

# Landsat 8 Collection 2 Level 2 — asset key -> output band name
LANDSAT_BANDS = {
    "coastal": "B1",
    "blue":    "B2",
    "green":   "B3",
    "red":     "B4",
    "nir08":   "B5",
    "pan":     "B8",
}

# Sentinel-2 L2A — band names used as both asset keys and output names
SENTINEL2_BANDS = ["B02", "B03", "B04", "B08"]


def download_file(url, out_path):
    """Stream-download a file, skipping if it already exists."""
    if os.path.exists(out_path):
        print(f"  skip (exists): {out_path}")
        return
    print(f"  downloading: {out_path}")
    r = requests.get(url, stream=True)
    r.raise_for_status()
    with open(out_path, "wb") as f:
        for chunk in r.iter_content(1024 * 1024):
            if chunk:
                f.write(chunk)


def download_landsat(catalog, regions, outdir):
    """Download Landsat 8 bands (B1, B2, B3, B4, B5, B8) from the old public GCS bucket."""
    print("\n=== Landsat 8 (6 bands) ===")
    os.makedirs(os.path.join(outdir, "landsat8"), exist_ok=True)

    band_files = {
        "B1": "B1",
        "B2": "B2",
        "B3": "B3",
        "B4": "B4",
        "B5": "B5",
        "B8": "B8",
    }

    for name, bbox in regions.items():
        print(f"\n[{name}]")
        search = catalog.search(
            collections=["landsat-c2-l2"],
            bbox=bbox,
            datetime="2021-01-01/2021-12-31",
            query={"eo:cloud_cover": {"lt": 10}},
        )
        items = [x for x in search.items() if x.id.startswith("LC08_")]
        if not items:
            print(f"  WARNING: no Landsat 8 scenes found for {name}")
            continue

        item = min(items, key=lambda x: x.properties.get("eo:cloud_cover", 999))
        cc = item.properties.get("eo:cloud_cover", "?")
        print(f"  scene: {item.id}  (cloud cover: {cc}%)")

        wrs_path = item.properties.get("landsat:wrs_path")
        wrs_row = item.properties.get("landsat:wrs_row")
        dt = item.datetime
        if wrs_path is None or wrs_row is None or dt is None:
            print("  WARNING: missing path/row/date metadata")
            continue

        path_str = f"{int(wrs_path):03d}"
        row_str = f"{int(wrs_row):03d}"
        acq_date = dt.strftime("%Y%m%d")

        prefix = f"gs://gcp-public-data-landsat/LC08/01/{path_str}/{row_str}/"
        print(f"  locating GCS scene for path/row/date {path_str}/{row_str}/{acq_date} ...")

        try:
            result = subprocess.run(
                ["gsutil", "ls", f"{prefix}**_B8.TIF"],
                check=False,
                capture_output=True,
                text=True,
            )
        except FileNotFoundError:
            print("  WARNING: gsutil not found")
            continue

        matches = []
        for line in result.stdout.splitlines():
            obj = line.strip()
            if not obj or obj.endswith("$folder$"):
                continue
            base = os.path.basename(obj)
            parts = base.split("_")
            if len(parts) < 8:
                continue
            # LC08_L1TP_192029_20130702_20170310_01_T1_B8.TIF
            obj_date = parts[3]
            if obj_date == acq_date:
                matches.append(obj)

        if not matches:
            print(f"  WARNING: no matching GCS scene found for date {acq_date}")
            continue

        chosen = ""
        for level in ("L1TP", "L1GT", "L1GS"):
            for m in matches:
                base = os.path.basename(m)
                if base.startswith(f"LC08_{level}_{path_str}{row_str}_{acq_date}_"):
                    chosen = m
                    break
            if chosen:
                break

        if not chosen:
            chosen = matches[0]

        chosen_base = os.path.basename(chosen)
        scene_id = chosen_base[:-len("_B8.TIF")]
        print(f"  GCS scene: {scene_id}")

        for band_name, gcs_band in band_files.items():
            url = (
                "https://storage.googleapis.com/gcp-public-data-landsat/"
                f"LC08/01/{path_str}/{row_str}/{scene_id}/{scene_id}_{gcs_band}.TIF"
            )
            out = os.path.join(outdir, "landsat8", f"Landsat8_{band_name}_{name}.tif")
            try:
                download_file(url, out)
            except Exception as e:
                print(f"  WARNING: failed to download {band_name}: {e}")


def download_sentinel2(catalog, regions, outdir):
    """Download Sentinel-2 bands (B02, B03, B04, B08) for each region."""
    print("\n=== Sentinel-2 (4 bands) ===")
    os.makedirs(os.path.join(outdir, "sentinel2"), exist_ok=True)

    for name, bbox in regions.items():
        print(f"\n[{name}]")
        search = catalog.search(
            collections=["sentinel-2-l2a"],
            bbox=bbox,
            datetime="2024-01-01/2024-12-31",
            query={"eo:cloud_cover": {"lt": 10}},
        )
        items = list(search.get_items())
        if not items:
            print(f"  WARNING: no Sentinel-2 scenes found for {name}")
            continue
        item = min(items, key=lambda x: x.properties["eo:cloud_cover"])
        signed = planetary_computer.sign(item)
        cc = item.properties.get("eo:cloud_cover", "?")
        print(f"  scene: {item.id}  (cloud cover: {cc}%)")

        for band in SENTINEL2_BANDS:
            if band not in signed.assets:
                print(f"  WARNING: asset '{band}' not found — available: {list(item.assets.keys())}")
                continue
            url = signed.assets[band].href
            out = os.path.join(outdir, "sentinel2", f"Sentinel2_{band}_{name}.tif")
            download_file(url, out)


def download_worldcover(catalog, regions, outdir):
    """Download ESA WorldCover tiles for each region."""
    print("\n=== ESA WorldCover ===")
    os.makedirs(os.path.join(outdir, "worldcover"), exist_ok=True)

    for name, bbox in regions.items():
        print(f"\n[{name}]")
        search = catalog.search(
            collections=["esa-worldcover"],
            bbox=bbox,
            datetime="2021",
        )
        items = list(search.get_items())
        if not items:
            print(f"  WARNING: no WorldCover tiles found for {name}")
            continue
        print(f"  {len(items)} tile(s)")
        for item in items:
            signed = planetary_computer.sign(item)
            url = signed.assets["map"].href
            out = os.path.join(outdir, "worldcover", f"WorldCover_{name}_{item.id}.tif")
            download_file(url, out)

    print("\nAfter download, crop each tile to 1x1 degree:")
    for name, bbox in regions.items():
        w, s, e, n = bbox
        print(f"  gdal_translate -projwin {w} {n} {e} {s} WorldCover_{name}_*.tif WorldCover_{name}.tif")


def download_nasadem(catalog, regions, outdir):
    """Download NASADEM (SRTM 1 arc-second / ~30m) tiles for each region."""
    print("\n=== SRTM_HighRes (NASADEM, ~30m) ===")
    os.makedirs(os.path.join(outdir, "srtm_highres"), exist_ok=True)

    for name, bbox in regions.items():
        print(f"\n[{name}]")
        search = catalog.search(
            collections=["nasadem"],
            bbox=bbox,
        )
        items = list(search.get_items())
        if not items:
            print(f"  WARNING: no NASADEM tiles found for {name}")
            continue
        print(f"  {len(items)} tile(s)")
        for item in items:
            signed = planetary_computer.sign(item)
            url = signed.assets["elevation"].href
            out = os.path.join(outdir, "srtm_highres", f"SRTM_HighRes_{name}_{item.id}.tif")
            download_file(url, out)

    print("\nAfter download, crop each region to 1x1 degree:")
    for name, bbox in regions.items():
        w, s, e, n = bbox
        print(f"  gdal_translate -projwin {w} {n} {e} {s} <merged>.tif SRTM_HighRes_{name}.tif")


DATASETS = {
    "landsat":    download_landsat,
    "sentinel2":  download_sentinel2,
    "worldcover": download_worldcover,
    "nasadem":    download_nasadem,
}


def main():
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--dataset", choices=list(DATASETS.keys()),
                        help="Download only this dataset (default: all)")
    parser.add_argument("--region", choices=list(REGIONS.keys()),
                        help="Download only this region (default: all)")
    parser.add_argument("--outdir", default=".",
                        help="Output directory (default: current dir)")
    args = parser.parse_args()

    catalog = Client.open(CATALOG_URL)

    regions = REGIONS
    if args.region:
        regions = {args.region: REGIONS[args.region]}

    datasets = DATASETS
    if args.dataset:
        datasets = {args.dataset: DATASETS[args.dataset]}

    os.makedirs(args.outdir, exist_ok=True)

    for name, func in datasets.items():
        func(catalog, regions, args.outdir)

    print("\nDone.")


if __name__ == "__main__":
    main()
