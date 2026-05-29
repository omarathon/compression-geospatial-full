#!/usr/bin/env python3
"""Patch an existing lossy_index.json with nrmse_at_formula_maxz for each target.

For each (TIF, band, target) that has maxZError_formula but no nrmse_at_formula_maxz,
does a LERC roundtrip at that maxZError and measures NRMSE over sampled windows.

Usage:
    python3 patch_index_formula_nrmse.py --index lossy_index.json [--workers N]
"""

import argparse
import json
import math
import multiprocessing as mp
import os
import sys
import uuid
from concurrent.futures import ProcessPoolExecutor, as_completed

import numpy as np
from osgeo import gdal

gdal.UseExceptions()
gdal.SetCacheMax(512 * 1024 * 1024)

STATS_BLOCK_SIZE = 256
MAX_STATS_BLOCKS = 2000
TEMP_DIR = "/scratch/omsst2/diss/temp"


def generate_windows(xsize, ysize):
    nx = math.ceil(xsize / STATS_BLOCK_SIZE)
    ny = math.ceil(ysize / STATS_BLOCK_SIZE)
    total = nx * ny
    indices = np.arange(total, dtype=np.int64)
    if total > MAX_STATS_BLOCKS:
        indices = np.linspace(0, total - 1, num=MAX_STATS_BLOCKS, dtype=np.int64)
    windows = []
    for idx in indices:
        by = int(idx) // nx
        bx = int(idx) % nx
        x = bx * STATS_BLOCK_SIZE
        y = by * STATS_BLOCK_SIZE
        windows.append((x, y, min(STATS_BLOCK_SIZE, xsize - x), min(STATS_BLOCK_SIZE, ysize - y)))
    return windows


def nrmse_windowed(orig_band, recon_band, nodata, windows):
    sum_sq = sum_orig = sum_orig_sq = 0.0
    count = 0
    for x, y, cols, rows in windows:
        o = orig_band.ReadAsArray(x, y, cols, rows)
        r = recon_band.ReadAsArray(x, y, cols, rows)
        if o is None or r is None:
            continue
        valid = (o != nodata) if nodata is not None else np.ones(o.shape, dtype=bool)
        vc = int(valid.sum())
        if vc == 0:
            continue
        if np.issubdtype(o.dtype, np.integer):
            diff = r.astype(np.int64) - o.astype(np.int64)
        else:
            diff = r.astype(np.float64) - o.astype(np.float64)
        o_f = o.astype(np.float64)
        sum_sq      += float(np.sum(diff * diff, where=valid, dtype=np.float64))
        sum_orig    += float(np.sum(o_f,         where=valid, dtype=np.float64))
        sum_orig_sq += float(np.sum(o_f * o_f,  where=valid, dtype=np.float64))
        count += vc
    if count == 0:
        return None
    rmse = math.sqrt(sum_sq / count)
    mean = sum_orig / count
    var  = max(0.0, sum_orig_sq / count - mean * mean)
    std  = math.sqrt(var)
    return (rmse / std) if std > 0.0 else None


def compute_nrmse_at_maxz(tif_path, band_num, maxz, orig_ds, windows):
    os.makedirs(TEMP_DIR, exist_ok=True)
    tmp = os.path.join(TEMP_DIR, f"patch_nrmse_{os.getpid()}_{uuid.uuid4().hex[:12]}.tif")
    try:
        gdal.Translate(tmp, tif_path, format="GTiff",
                       creationOptions=["COMPRESS=LERC", f"MAX_Z_ERROR={maxz}"])
        recon_ds = gdal.Open(tmp, gdal.GA_ReadOnly)
        if recon_ds is None:
            return None
        nodata = orig_ds.GetRasterBand(band_num).GetNoDataValue()
        result = nrmse_windowed(orig_ds.GetRasterBand(band_num),
                                recon_ds.GetRasterBand(band_num), nodata, windows)
        recon_ds = None
    finally:
        if os.path.exists(tmp):
            os.remove(tmp)
    return result


def patch_tif(tif_path, tif_entry):
    """Compute missing nrmse_at_formula_maxz values for one TIF. Returns patched entry."""
    orig_ds = gdal.Open(tif_path, gdal.GA_ReadOnly)
    if orig_ds is None:
        print(f"  SKIP (cannot open): {os.path.basename(tif_path)}", flush=True)
        return tif_entry

    windows = generate_windows(orig_ds.RasterXSize, orig_ds.RasterYSize)

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
            nrmse = compute_nrmse_at_maxz(tif_path, band_num, maxz, orig_ds, windows)
            target_data["nrmse_at_formula_maxz"] = nrmse
            print(f"  {os.path.basename(tif_path)} band={band_key} target={target_key}"
                  f" maxz={maxz:.1f} nrmse={nrmse:.4f}" if nrmse is not None else
                  f"  {os.path.basename(tif_path)} band={band_key} target={target_key}"
                  f" maxz={maxz:.1f} nrmse=None", flush=True)

    orig_ds = None
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
