#!/usr/bin/env python3
"""Patch an existing lossy_index.json with nrmse_at_formula_maxz for each target.

For each (TIF, band, target) that has maxZError_formula but no nrmse_at_formula_maxz,
delegates to lossy_transform_tiff.py for the LERC roundtrip and reads
nrmse_std_original from the stats.json it produces.

Usage:
    python3 patch_index_formula_nrmse.py --index lossy_index.json [--workers N]
"""

import argparse
import json
import multiprocessing as mp
import os
import shutil
import subprocess
import sys
import uuid
from concurrent.futures import ProcessPoolExecutor, as_completed

LOSSY_SCRIPT = "/home/omsst2/diss/compression-geospatial-full/scripts/lossy_transform_tiff.py"
TEMP_DIR = "/scratch/omsst2/diss/temp"


def compute_nrmse_at_maxz(tif_path, band_num, maxz):
    tmp_dir = os.path.join(TEMP_DIR, f"patch_nrmse_{os.getpid()}_{uuid.uuid4().hex[:12]}")
    os.makedirs(tmp_dir, exist_ok=True)
    try:
        proc = subprocess.run(
            [
                "python3", LOSSY_SCRIPT,
                tif_path,
                "--z-errors", str(maxz),
                "--output-dir", tmp_dir,
                "--json-name", "stats.json",
            ],
            capture_output=True, text=True,
        )
        if proc.returncode != 0:
            return None
        json_path = os.path.join(tmp_dir, "stats.json")
        if not os.path.exists(json_path):
            return None
        with open(json_path) as f:
            stats = json.load(f)
        results = stats.get("results", [])
        if not results:
            return None
        band_stats = results[0].get("bands", {}).get(str(band_num), {})
        return band_stats.get("nrmse_std_original")
    finally:
        shutil.rmtree(tmp_dir, ignore_errors=True)


def patch_tif(tif_path, tif_entry):
    """Compute missing nrmse_at_formula_maxz values for one TIF. Returns patched entry."""
    from osgeo import gdal
    gdal.UseExceptions()
    ds = gdal.Open(tif_path, gdal.GA_ReadOnly)
    if ds is None:
        print(f"  SKIP (cannot open): {os.path.basename(tif_path)}", flush=True)
        return tif_entry
    ds = None

    for band_key, band_data in tif_entry.items():
        if not isinstance(band_data, dict) or "nrmse_targets" not in band_data:
            continue
        band_num = int(band_key)

        for target_key, target_data in band_data["nrmse_targets"].items():
            if not isinstance(target_data, dict):
                continue
            if "maxZError_formula" not in target_data:
                continue
            if "nrmse_at_formula_maxz" in target_data:
                continue  # already patched

            maxz = target_data["maxZError_formula"]
            nrmse = compute_nrmse_at_maxz(tif_path, band_num, maxz)
            target_data["nrmse_at_formula_maxz"] = nrmse
            print(f"  {os.path.basename(tif_path)} band={band_key} target={target_key}"
                  f" maxz={maxz:.1f} nrmse={nrmse:.4f}" if nrmse is not None else
                  f"  {os.path.basename(tif_path)} band={band_key} target={target_key}"
                  f" maxz={maxz:.1f} nrmse=None", flush=True)

    return tif_entry


def _worker_run(args):
    tif_path, tif_entry = args
    return tif_path, patch_tif(tif_path, tif_entry)


def main():
    parser = argparse.ArgumentParser(description="Patch lossy_index.json with NRMSE at formula maxZError.")
    parser.add_argument("--index",   required=True, help="Path to lossy_index.json")
    parser.add_argument("--workers", type=int, default=1)
    args = parser.parse_args()

    with open(args.index) as f:
        raw = json.load(f)

    index = raw.get("tifs", raw)
    is_wrapped = "tifs" in raw

    # Only process TIFs that have at least one formula entry needing patching.
    to_patch = {
        path: entry for path, entry in index.items()
        if any(
            "maxZError_formula" in t and "nrmse_at_formula_maxz" not in t
            for band_data in entry.values() if isinstance(band_data, dict)
            for t in band_data.get("nrmse_targets", {}).values() if isinstance(t, dict)
        )
    }

    total = len(to_patch)
    print(f"TIFs needing patch: {total} / {len(index)}", flush=True)

    if total == 0:
        print("Nothing to patch.")
        return

    done = 0

    if args.workers == 1:
        for tif_path, tif_entry in to_patch.items():
            index[tif_path] = _worker_run((tif_path, tif_entry))[1]
            done += 1
            print(f"[{done}/{total}] {os.path.basename(tif_path)}", flush=True)
    else:
        with ProcessPoolExecutor(max_workers=args.workers) as ex:
            futures = {ex.submit(_worker_run, (p, e)): p for p, e in to_patch.items()}
            for fut in as_completed(futures):
                tif_path, patched = fut.result()
                index[tif_path] = patched
                done += 1
                print(f"[{done}/{total}] {os.path.basename(tif_path)}", flush=True)

    out = ({"tifs": index, **{k: v for k, v in raw.items() if k != "tifs"}}
           if is_wrapped else index)

    with open(args.index, "w") as f:
        json.dump(out, f, indent=2)
    print(f"\nPatched index saved to {args.index}")


if __name__ == "__main__":
    main()
