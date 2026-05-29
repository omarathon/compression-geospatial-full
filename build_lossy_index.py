#!/usr/bin/env python3
"""Build a per-(TIF, band) index of maxZError values for target NRMSE thresholds.

For each TIF and each band produces:
  - Band population std (all valid pixels, nodata excluded)
  - Band value range (max - min, valid pixels only)
  - For each NRMSE target (5 %, 10 %, 15 %):
      maxZError_formula  = std * target          (closed-form)
      maxZError_search   = binary-searched value (tolerance ±1 DN)
      nrmse_achieved     = actual NRMSE at maxZError_search

Binary search bounds:
  low  = 1 (minimum useful maxZError)
  high = band range (max - min of valid pixels) — always a safe upper bound

LERC roundtrip: full-TIF gdal.Translate with COMPRESS=LERC, read back directly.
Results for all bands are computed from a single roundtrip and cached, so
different (band, target) binary searches that need the same maxZError only
trigger one LERC write.

Usage:
    python3 build_lossy_index.py [--workers N] [--output lossy_index.json]
"""

import argparse
import glob as glob_module
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

sys.path.insert(0, os.path.join(os.path.dirname(__file__), "scripts"))
from lossy_transform_tiff import lerc_roundtrip, detect_layout_creation_options

# ---------------------------------------------------------------------------
# Dataset inventory (same as run_benchmarks_sweep_realdata_parallel.py)
# ---------------------------------------------------------------------------

DISS = "/maps/omsst2/diss"

COLLECTIONS = [
    ("Fuieri_2014_Landsat8_Pano_B8",        256, [f"{DISS}/papers/rasterlite/landsat_pano/LC08_192029_20130702_B8.TIF"],                                             [16]),
    ("Fuieri_2014_srtm",                    256, [f"{DISS}/papers/rasterlite/SRTM_Italy/srtm_italy_exact/srtm_tiles/SRTM_Italy_final.tif"],                          [16]),
    ("Fuieri_2019_ETOPO1",                  256, [f"{DISS}/papers/rasterlite/ETOPO1/ETOPO1_Ice_g_geotiff.tif"],                                                      [16]),
    # ("Fuieri_2019_Tuscany_DTM",             32, [f"{DISS}/papers/rasterlite/tuscany_dtm/quantized_2000.tif"],                                                       [32]),
    ("Zalipynis_2018_0_Landsat8_B4_B5_Mosaic", 256, [f"{DISS}/papers/zalipynis/2018/landsat8_mosaic_B4.tif",
                                                      f"{DISS}/papers/zalipynis/2018/landsat8_mosaic_B5.tif"],                                                       [16]),
    ("Zalipynis_2018_1_Landsat8_B1",        256, [f"{DISS}/papers/zalipynis/2018_2/landsat/landsat8_path190_row031_stack.tif"],                                      [16]),
    ("Zalipynis_2019_Landsat8_B4_Mosaic",   256, [f"{DISS}/papers/zalipynis/2019_landsat_band4_mosaic/exact/landsat8_mosaic_B4.tif"],                                [16]),
    ("other_sentinel2_B2",                  256, [f"{DISS}/others/sentinel2/Sentinel2_B02_*.tif"],                                                                   [16]),
    ("other_sentinel2_B3",                  256, [f"{DISS}/others/sentinel2/Sentinel2_B03_*.tif"],                                                                   [16]),
    ("other_sentinel2_B4",                  256, [f"{DISS}/others/sentinel2/Sentinel2_B04_*.tif"],                                                                   [16]),
    ("other_sentinel2_B8",                  256, [f"{DISS}/others/sentinel2/Sentinel2_B08_*.tif"],                                                                   [16]),
    ("Zaytar_2025_Sentinel2_B2",            256, [f"{DISS}/papers/zaytar/2025/final/split_bands/Sentinel_*_B2.tif"],                                                 [16]),
    ("Zaytar_2025_Sentinel2_B3",            256, [f"{DISS}/papers/zaytar/2025/final/split_bands/Sentinel_*_B3.tif"],                                                 [16]),
    ("Zaytar_2025_Sentinel2_B4",            256, [f"{DISS}/papers/zaytar/2025/final/split_bands/Sentinel_*_B4.tif"],                                                 [16]),
    ("Zaytar_2025_Sentinel2_B8",            256, [f"{DISS}/papers/zaytar/2025/final/split_bands/Sentinel_*_B8.tif"],                                                 [16]),
    ("other_landsat8_B1",                   256, [f"{DISS}/others/landsat8/Landsat8_B1_*.tif"],                                                                      [16]),
    ("other_landsat8_B2",                   256, [f"{DISS}/others/landsat8/Landsat8_B2_*.tif"],                                                                      [16]),
    ("other_landsat8_B3",                   256, [f"{DISS}/others/landsat8/Landsat8_B3_*.tif"],                                                                      [16]),
    ("other_landsat8_B4",                   256, [f"{DISS}/others/landsat8/Landsat8_B4_*.tif"],                                                                      [16]),
    ("other_landsat8_B5",                   256, [f"{DISS}/others/landsat8/Landsat8_B5_*.tif"],                                                                      [16]),
    ("other_landsat8_B8",                   256, [f"{DISS}/others/landsat8/Landsat8_B8_*.tif"],                                                                      [16]),
    # ("other_sentinel2",                     256, [f"{DISS}/others/sentinel2/*.tif"],                                                                                 [16]),
    ("other_srtm_highres",                  256, [f"{DISS}/others/srtm_highres/*.tif"],                                                                              [16]),
    ("other_worldclim",                     256, [f"{DISS}/others/worldclim/*.tif"],                                                                                 [16]),
    ("other_worldcover_int16",              256, [f"{DISS}/others/worldcover_int16/*.tif"],                                                                          [16]),
    ("other_etopo1",                        256, [f"{DISS}/others/etopo1/*.tif"],                                                                                    [16]),
    # ("other_etopo_highres_quant",           256, [f"{DISS}/others/etopo_highres_quant/*.tif"],                                                                       [32]),
    # ("other_landscan",                      128, [f"{DISS}/others/landscan/*.tif"],                                                                                  [32]),
    ("other_srtm",                          256, [f"{DISS}/others/srtm/*.tif"],                                                                                      [16]),
    # ("other_worldcover",                    256, [f"{DISS}/others/worldcover/*.tif"],                                                                                [16]),
    # # original sweep datasets
    ("srtm_45_15",                          256, [f"{DISS}/srtm_45_15.tif"],                                                                                        [16]),
    ("JRC_TMF",                             256, [f"{DISS}/JRC_TMF_AnnualChange_v1_1990_AFR_ID16_S10_E10.tif"],                                                      [16]),
    # ("accessibility",                        64, [f"{DISS}/accessibility.tif"],                                                                                      [32]),
    ("slope_srtm",                          256, [f"{DISS}/slope-srtm_35_11.tif"],                                                                                   [16]),
]

NRMSE_TARGETS   = [0.05, 0.10, 0.15]
SEARCH_TOLERANCE = 1.0  # binary search stops when high - low ≤ this (DN)

WORKER_CORES_NODE0 = [0, 4, 8, 12, 16, 20, 24, 28]
WORKER_CORES_NODE1 = [64, 68, 72, 76, 80, 84, 88, 92]

_WORKER_CORE     = None
_WORKER_NUMA_NODE = None

# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------

def expand_globs(patterns):
    paths = []
    for p in patterns:
        matched = sorted(glob_module.glob(p))
        paths.extend(matched if matched else [p])
    return paths


def _valid_pixels(arr, nodata):
    """Return 1-D float64 array of valid (non-nodata) pixels."""
    flat = arr.ravel()
    if nodata is not None:
        flat = flat[flat != nodata]
    return flat.astype(np.float64)


def band_stats(arr, nodata):
    """Return (std, range, mean) for valid pixels, or (None, None, None)."""
    v = _valid_pixels(arr, nodata)
    if len(v) == 0:
        return None, None, None
    mean = float(v.mean())
    std  = float(v.std())           # population std
    rng  = float(v.max() - v.min())
    return std, rng, mean


def nrmse_from_arrays(orig_arr, recon_arr, nodata):
    """NRMSE_std between orig and recon for the valid pixels."""
    if nodata is not None:
        mask = orig_arr.ravel() != nodata 
    else:
        mask = np.ones(orig_arr.size, dtype=bool)
    if not mask.any():
        return None
    o = orig_arr.ravel().astype(np.float64)[mask]
    r = recon_arr.ravel().astype(np.float64)[mask]
    rmse = math.sqrt(float(((r - o) ** 2).mean()))
    std  = float(o.std())
    return (rmse / std) if std > 0.0 else None


def do_lerc_roundtrip(tif_path, maxz, creation_opts, src_ds, n_bands):
    """Call lossy_transform_tiff.lerc_roundtrip, read back all band arrays.

    Uses the same code path as the lossy_transform_tiff script (two-step
    translate + metadata copy), so pixel values are guaranteed consistent.
    Returns dict: band_num (1-based) -> numpy array, or None on failure.
    """
    tmp = f"/scratch/omsst2/diss/temp/lerc_idx_{os.getpid()}_{uuid.uuid4().hex[:12]}.tif"
    os.makedirs(os.path.dirname(tmp), exist_ok=True)
    try:
        lerc_roundtrip(tif_path, tmp, maxz, creation_opts, src_ds)
        recon_ds = gdal.Open(tmp, gdal.GA_ReadOnly)
        if recon_ds is None:
            return None
        arrays = {b: recon_ds.GetRasterBand(b).ReadAsArray() for b in range(1, n_bands + 1)}
        recon_ds = None
    finally:
        if os.path.exists(tmp):
            os.remove(tmp)
    return arrays


# ---------------------------------------------------------------------------
# Per-TIF processing
# ---------------------------------------------------------------------------

def process_tif(tif_path):
    orig_ds = gdal.Open(tif_path, gdal.GA_ReadOnly)
    if orig_ds is None:
        return {"error": f"cannot open {tif_path}"}

    n_bands = orig_ds.RasterCount

    # Read all original arrays and compute per-band stats upfront.
    orig_arrays = {}
    nodatas     = {}
    stds        = {}
    ranges      = {}

    for b in range(1, n_bands + 1):
        band       = orig_ds.GetRasterBand(b)
        nodata     = band.GetNoDataValue()
        arr        = band.ReadAsArray()
        std, rng, _mean = band_stats(arr, nodata)
        orig_arrays[b] = arr
        nodatas[b]     = nodata
        stds[b]        = std
        ranges[b]      = rng

    # Keep orig_ds open — lerc_roundtrip needs it for metadata copying.
    creation_opts = detect_layout_creation_options(orig_ds)

    # Cache: maxZError -> {band -> nrmse}  (one full-TIF roundtrip serves all bands)
    roundtrip_cache = {}

    def get_nrmse(b, maxz):
        if maxz not in roundtrip_cache:
            recon = do_lerc_roundtrip(tif_path, maxz, creation_opts, orig_ds, n_bands)
            if recon is None:
                roundtrip_cache[maxz] = {}
            else:
                roundtrip_cache[maxz] = {
                    bk: nrmse_from_arrays(orig_arrays[bk], recon[bk], nodatas[bk])
                    for bk in range(1, n_bands + 1)
                }
        return roundtrip_cache[maxz].get(b)

    # Binary search per (band, target).
    bands_out = {}
    for b in range(1, n_bands + 1):
        std = stds[b]
        rng = ranges[b]

        if std is None or std == 0.0:
            bands_out[str(b)] = {"error": "zero or undefined std"}
            continue

        targets_out = {}
        for target in NRMSE_TARGETS:
            formula_maxz = std * target

            low  = 1.0
            high = rng

            # Verify the upper bound actually exceeds the target.
            nrmse_high = get_nrmse(b, high)
            if nrmse_high is None:
                targets_out[str(target)] = {
                    "maxZError_formula": formula_maxz,
                    "error": "LERC roundtrip failed at upper bound",
                }
                continue
            if nrmse_high <= target:
                targets_out[str(target)] = {
                    "maxZError_formula": formula_maxz,
                    "error": (
                        f"NRMSE({rng:.1f}) = {nrmse_high:.4f} ≤ target {target}; "
                        "range is not a sufficient upper bound for this band"
                    ),
                }
                continue

            last_valid_maxz  = 0.0
            last_valid_nrmse = 0.0

            while high - low > SEARCH_TOLERANCE:
                mid   = (low + high) / 2.0
                nrmse = get_nrmse(b, mid)
                if nrmse is None:
                    break
                if nrmse <= target:
                    last_valid_maxz  = mid
                    last_valid_nrmse = nrmse
                    low = mid
                else:
                    high = mid

            targets_out[str(target)] = {
                "maxZError_formula": formula_maxz,
                "maxZError_search":  last_valid_maxz,
                "nrmse_achieved":    last_valid_nrmse,
            }

        bands_out[str(b)] = {
            "std":   std,
            "range": rng,
            "nrmse_targets": targets_out,
        }

    orig_ds = None
    return bands_out


# ---------------------------------------------------------------------------
# Worker plumbing (mirrors run_benchmarks_sweep_realdata_parallel.py)
# ---------------------------------------------------------------------------

def _worker_run(tif_path):
    return tif_path, process_tif(tif_path)


def _init_worker(cores_list, numa_node, counter):
    global _WORKER_CORE, _WORKER_NUMA_NODE
    with counter.get_lock():
        idx = counter.value
        counter.value += 1
    _WORKER_CORE      = cores_list[idx]
    _WORKER_NUMA_NODE = numa_node
    try:
        os.sched_setaffinity(0, {_WORKER_CORE})
    except (AttributeError, OSError):
        pass


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------

def main():
    parser = argparse.ArgumentParser(
        description="Build per-(TIF, band) LERC maxZError lookup index.")
    parser.add_argument("--workers", type=int, default=1,
                        help="Parallel worker processes (default 1).")
    parser.add_argument("--node", type=int, choices=[0, 1], default=0,
                        help="NUMA node to bind workers to (default 0).")
    parser.add_argument("--output", default="lossy_index.json",
                        help="Output JSON path (default lossy_index.json).")
    args = parser.parse_args()

    pool = WORKER_CORES_NODE0 if args.node == 0 else WORKER_CORES_NODE1
    if args.workers > len(pool):
        parser.error(f"--workers > {len(pool)} not supported for node {args.node}")
    worker_cores = pool[:args.workers]

    seen, tif_paths = set(), []
    for _name, _bs, globs, _widths in COLLECTIONS:
        for path in expand_globs(globs):
            if path not in seen:
                seen.add(path)
                tif_paths.append(path)

    total = len(tif_paths)
    print(f"TIFs to index: {total}")
    print(f"workers={args.workers}  node={args.node}  cores={worker_cores}", flush=True)

    index = {}
    done  = 0

    if args.workers == 1:
        for path in tif_paths:
            _, result = _worker_run(path)
            index[path] = result
            done += 1
            print(f"[{done}/{total}] {os.path.basename(path)}", flush=True)
    else:
        counter = mp.Value("i", 0)
        with ProcessPoolExecutor(
            max_workers=args.workers,
            initializer=_init_worker,
            initargs=(worker_cores, args.node, counter),
        ) as ex:
            futures = {ex.submit(_worker_run, p): p for p in tif_paths}
            for fut in as_completed(futures):
                path, result = fut.result()
                index[path] = result
                done += 1
                print(f"[{done}/{total}] {os.path.basename(path)}", flush=True)

    out = {
        "nrmse_targets":       NRMSE_TARGETS,
        "search_tolerance_dn": SEARCH_TOLERANCE,
        "tifs":                index,
    }
    with open(args.output, "w") as f:
        json.dump(out, f, indent=2)
    print(f"\nIndex saved to {args.output}")


if __name__ == "__main__":
    main()
