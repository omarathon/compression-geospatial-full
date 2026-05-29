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
from lossy_transform_tiff import detect_layout_creation_options, median_filter_transform

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

NRMSE_TARGETS    = [0.05, 0.10, 0.15, 0.30]
SEARCH_TOLERANCE = 1.0  # binary search stops when high - low ≤ this (DN)
MEDIAN_KERNELS   = [3, 5, 7]

WORKER_CORES_NODE0 = [0, 4, 8, 12, 16, 20, 24, 28]
WORKER_CORES_NODE1 = [64, 68, 72, 76, 80, 84, 88, 92]

_WORKER_CORE      = None
_WORKER_NUMA_NODE = None
_DO_FORMULA       = True
_DO_SEARCH        = False
_DO_MEDIAN        = False

# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------

def expand_globs(patterns):
    paths = []
    for p in patterns:
        matched = sorted(glob_module.glob(p))
        paths.extend(matched if matched else [p])
    return paths


STATS_BLOCK_SIZE = 256
MAX_STATS_BLOCKS = 2000


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


def band_stats_windowed(band, nodata, windows):
    """Population std, range, mean over sampled windows, nodata excluded."""
    sum_x = sum_x2 = 0.0
    vmin = float("inf")
    vmax = float("-inf")
    count = 0
    for x, y, cols, rows in windows:
        arr = band.ReadAsArray(x, y, cols, rows)
        if arr is None:
            continue
        flat = arr.ravel().astype(np.float64)
        if nodata is not None:
            flat = flat[arr.ravel() != nodata]
        if len(flat) == 0:
            continue
        sum_x  += float(flat.sum())
        sum_x2 += float((flat * flat).sum())
        vmin    = min(vmin, float(flat.min()))
        vmax    = max(vmax, float(flat.max()))
        count  += len(flat)
    if count == 0:
        return None, None, None
    mean = sum_x / count
    var  = max(0.0, sum_x2 / count - mean * mean)
    return math.sqrt(var), vmax - vmin, mean


def nrmse_windowed(orig_band, recon_band, nodata, windows):
    """NRMSE_std computed over sampled windows."""
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


def do_median_nrmse(tif_path, kernel, orig_ds, windows):
    """Apply median_filter_transform and compute NRMSE over sampled windows.

    Uses lossy_transform_tiff.median_filter_transform for the filtering step,
    matching the same code path as the benchmark script.
    Returns NRMSE_std (float) or None on failure.
    """
    tmp = f"/scratch/omsst2/diss/temp/median_idx_{os.getpid()}_{uuid.uuid4().hex[:12]}.tif"
    os.makedirs(os.path.dirname(tmp), exist_ok=True)
    creation_opts = detect_layout_creation_options(orig_ds)
    try:
        median_filter_transform(tmp, kernel, creation_opts, orig_ds)
        filt_ds = gdal.Open(tmp, gdal.GA_ReadOnly)
        if filt_ds is None:
            return None
        nodata = orig_ds.GetRasterBand(1).GetNoDataValue()
        result = nrmse_windowed(orig_ds.GetRasterBand(1), filt_ds.GetRasterBand(1), nodata, windows)
        filt_ds = None
    finally:
        if os.path.exists(tmp):
            os.remove(tmp)
    return result


def do_lerc_roundtrip(tif_path, maxz, orig_ds, n_bands, windows):
    """Compress to LERC and compute per-band NRMSE over sampled windows.

    Reads directly from the LERC-compressed file (one gdal.Translate, not two).
    Pixel values are identical to a full lerc_roundtrip decompress step — LERC
    quantises on write; reading the compressed file back gives the same values.
    Returns dict: band_num (1-based) -> nrmse float, or None on failure.
    """
    tmp = f"/scratch/omsst2/diss/temp/lerc_idx_{os.getpid()}_{uuid.uuid4().hex[:12]}.tif"
    os.makedirs(os.path.dirname(tmp), exist_ok=True)
    try:
        gdal.Translate(
            tmp, tif_path,
            format="GTiff",
            creationOptions=["COMPRESS=LERC", f"MAX_Z_ERROR={maxz}"],
        )
        recon_ds = gdal.Open(tmp, gdal.GA_ReadOnly)
        if recon_ds is None:
            return None
        result = {}
        for b in range(1, n_bands + 1):
            nodata = orig_ds.GetRasterBand(b).GetNoDataValue()
            result[b] = nrmse_windowed(
                orig_ds.GetRasterBand(b), recon_ds.GetRasterBand(b), nodata, windows
            )
        recon_ds = None
    finally:
        if os.path.exists(tmp):
            os.remove(tmp)
    return result


# ---------------------------------------------------------------------------
# Per-TIF processing
# ---------------------------------------------------------------------------

def process_tif(tif_path):
    fname = os.path.basename(tif_path)
    print(f"  [{os.getpid()}] start {fname}", flush=True)

    orig_ds = gdal.Open(tif_path, gdal.GA_ReadOnly)
    if orig_ds is None:
        return {"error": f"cannot open {tif_path}"}

    n_bands = orig_ds.RasterCount
    if n_bands > 1:
        orig_ds = None
        return {"error": f"multi-band TIF ({n_bands} bands) skipped"}

    windows = generate_windows(orig_ds.RasterXSize, orig_ds.RasterYSize)

    stds   = {}
    ranges = {}
    for b in range(1, n_bands + 1):
        band   = orig_ds.GetRasterBand(b)
        nodata = band.GetNoDataValue()
        std, rng, _ = band_stats_windowed(band, nodata, windows)
        stds[b]   = std
        ranges[b] = rng

    # Formula needs only std — no LERC at all.
    # Search needs orig_ds open for nrmse_windowed reads during binary search.
    if not _DO_SEARCH:
        orig_ds = None

    if _DO_SEARCH:
        print(f"  [{os.getpid()}] stats done {fname}, starting binary search", flush=True)
        roundtrip_cache = {}
        iteration_count = [0]

        def get_nrmse(b, maxz):
            if maxz not in roundtrip_cache:
                iteration_count[0] += 1
                result = do_lerc_roundtrip(tif_path, maxz, orig_ds, n_bands, windows)
                roundtrip_cache[maxz] = result or {}
                nrmse_val = roundtrip_cache[maxz].get(b)
                print(
                    f"  [{os.getpid()}] {fname} iter={iteration_count[0]}"
                    f" maxz={maxz:.2f} nrmse={nrmse_val:.4f}" if nrmse_val is not None else
                    f"  [{os.getpid()}] {fname} iter={iteration_count[0]}"
                    f" maxz={maxz:.2f} nrmse=None",
                    flush=True,
                )
            return roundtrip_cache[maxz].get(b)

    bands_out = {}
    for b in range(1, n_bands + 1):
        std = stds[b]
        rng = ranges[b]

        if std is None or std == 0.0:
            bands_out[str(b)] = {"error": "zero or undefined std"}
            continue

        targets_out = {}
        for target in NRMSE_TARGETS:
            entry = {}

            if _DO_FORMULA:
                entry["maxZError_formula"] = std * target

            if _DO_SEARCH:
                low  = 1.0
                high = max(std, 2.0)

                for _ in range(5):
                    nrmse_high = get_nrmse(b, high)
                    if nrmse_high is not None and nrmse_high > target:
                        break
                    high *= 3.0
                else:
                    entry["search_error"] = f"could not find upper bound where NRMSE > {target}"
                    targets_out[str(target)] = entry
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

                entry["maxZError_search"] = last_valid_maxz
                entry["nrmse_achieved"]   = last_valid_nrmse
                print(
                    f"  [{os.getpid()}] {fname} target={target} done:"
                    f" maxZError_search={last_valid_maxz:.2f} nrmse={last_valid_nrmse:.4f}",
                    flush=True,
                )

            targets_out[str(target)] = entry

        median_out = {}
        if _DO_MEDIAN:
            for kernel in MEDIAN_KERNELS:
                print(f"  [{os.getpid()}] {fname} median k={kernel}", flush=True)
                nrmse = do_median_nrmse(tif_path, kernel, orig_ds, windows)
                median_out[str(kernel)] = nrmse

        bands_out[str(b)] = {
            "std":            std,
            "range":          rng,
            "nrmse_targets":  targets_out,
            **({"median_filter_nrmse": median_out} if _DO_MEDIAN else {}),
        }

    orig_ds = None
    return bands_out


# ---------------------------------------------------------------------------
# Worker plumbing (mirrors run_benchmarks_sweep_realdata_parallel.py)
# ---------------------------------------------------------------------------

def _worker_run(tif_path):
    return tif_path, process_tif(tif_path)


def _init_worker(cores_list, numa_node, counter, do_formula, do_search, do_median):
    global _WORKER_CORE, _WORKER_NUMA_NODE, _DO_FORMULA, _DO_SEARCH, _DO_MEDIAN
    with counter.get_lock():
        idx = counter.value
        counter.value += 1
    _WORKER_CORE      = cores_list[idx]
    _WORKER_NUMA_NODE = numa_node
    _DO_FORMULA       = do_formula
    _DO_SEARCH        = do_search
    _DO_MEDIAN        = do_median
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
    parser.add_argument("--formula", action=argparse.BooleanOptionalAction, default=True,
                        help="Compute maxZError = std * target for each threshold (default: on).")
    parser.add_argument("--search", action=argparse.BooleanOptionalAction, default=False,
                        help="Binary-search for maxZError achieving exact NRMSE target (default: off).")
    parser.add_argument("--median", action=argparse.BooleanOptionalAction, default=False,
                        help="Compute NRMSE for median filter kernels 3x3, 5x5, 7x7 (default: off).")
    args = parser.parse_args()

    if not args.formula and not args.search and not args.median:
        parser.error("at least one of --formula / --search / --median must be enabled")

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
    print(f"workers={args.workers}  node={args.node}  cores={worker_cores}")
    print(f"formula={args.formula}  search={args.search}  median={args.median}", flush=True)

    index = {}
    done  = 0

    if args.workers == 1:
        global _DO_FORMULA, _DO_SEARCH, _DO_MEDIAN
        _DO_FORMULA = args.formula
        _DO_SEARCH  = args.search
        _DO_MEDIAN  = args.median
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
            initargs=(worker_cores, args.node, counter, args.formula, args.search, args.median),
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
