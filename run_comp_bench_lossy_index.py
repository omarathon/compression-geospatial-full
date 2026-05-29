#!/usr/bin/env python3
"""Benchmark bench_comp on lossless and index-derived LERC-lossy TIF variants.

For each TIF in COLLECTIONS:
  1. lossless: bench_comp on the original TIF
  2. For each NRMSE target (5 %, 10 %, 15 %) × 2 interpretations:
       formula : maxZError = std * target  (closed-form from index)
       search  : maxZError = binary-searched value (from index)
     → produce lossy TIF via lossy_transform_tiff.py, bench_comp, delete TIF

maxZError selection for multi-band TIFs: minimum across all bands so that
every band satisfies the NRMSE constraint.  Single-band TIFs are exact.

Output: one .txt log per collection under OUTDIR.

Usage:
    python3 run_comp_bench_lossy_index.py --index lossy_index.json [--workers N]
"""

import argparse
import glob as glob_module
import io
import json
import multiprocessing as mp
import os
import shutil
import subprocess
import uuid
from concurrent.futures import ProcessPoolExecutor, as_completed

# ---------------------------------------------------------------------------
# Paths
# ---------------------------------------------------------------------------

BENCH        = "/home/omsst2/diss/compression-geospatial-full/build/bench_comp"
LOSSY_SCRIPT = "/home/omsst2/diss/compression-geospatial-full/scripts/lossy_transform_tiff.py"
OUTDIR       = "/scratch/omsst2/diss/comp_bench_lossy_index"
TEMP_DIR     = "/scratch/omsst2/diss/temp/comp_bench_lossy_index"

DISS = "/maps/omsst2/diss"

# ---------------------------------------------------------------------------
# Collections (same as build_lossy_index.py)
# ---------------------------------------------------------------------------

COLLECTIONS = [
    ("Fuieri_2014_Landsat8_Pano_B8",        256, [f"{DISS}/papers/rasterlite/landsat_pano/LC08_192029_20130702_B8.TIF"],                                             [16]),
    ("Fuieri_2014_srtm",                    256, [f"{DISS}/papers/rasterlite/SRTM_Italy/srtm_italy_exact/srtm_tiles/SRTM_Italy_final.tif"],                          [16]),
    ("Fuieri_2019_ETOPO1",                  256, [f"{DISS}/papers/rasterlite/ETOPO1/ETOPO1_Ice_g_geotiff.tif"],                                                      [16]),
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
    ("other_srtm_highres",                  256, [f"{DISS}/others/srtm_highres/*.tif"],                                                                              [16]),
    ("other_worldclim",                     256, [f"{DISS}/others/worldclim/*.tif"],                                                                                 [16]),
    ("other_worldcover_int16",              256, [f"{DISS}/others/worldcover_int16/*.tif"],                                                                          [16]),
    ("other_etopo1",                        256, [f"{DISS}/others/etopo1/*.tif"],                                                                                    [16]),
    ("other_srtm",                          256, [f"{DISS}/others/srtm/*.tif"],                                                                                      [16]),
    ("srtm_45_15",                          256, [f"{DISS}/srtm_45_15.tif"],                                                                                        [16]),
    ("JRC_TMF",                             256, [f"{DISS}/JRC_TMF_AnnualChange_v1_1990_AFR_ID16_S10_E10.tif"],                                                      [16]),
    ("slope_srtm",                          256, [f"{DISS}/slope-srtm_35_11.tif"],                                                                                   [16]),
]

# ---------------------------------------------------------------------------
# Benchmark parameters
# ---------------------------------------------------------------------------

NRMSE_TARGETS = [0.05, 0.10, 0.15]
NODATA_VALUES = [0, 101]

BENCH_COMMON_ARGS = [
    "-b", "256",
    "-n", "4000",
    "--ordering", "default",
    "--normalize",
]

THREAD_ENV_OVERRIDES = {
    "OMP_NUM_THREADS":        "1",
    "OPENBLAS_NUM_THREADS":   "1",
    "MKL_NUM_THREADS":        "1",
    "BLIS_NUM_THREADS":       "1",
    "VECLIB_MAXIMUM_THREADS": "1",
    "NUMEXPR_NUM_THREADS":    "1",
    "GDAL_NUM_THREADS":       "1",
}

WORKER_CORES_NODE0 = [0, 4, 8, 12, 16, 20, 24, 28]
WORKER_CORES_NODE1 = [64, 68, 72, 76, 80, 84, 88, 92]

_WORKER_CORE      = None
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


def make_env():
    env = os.environ.copy()
    env.update(THREAD_ENV_OVERRIDES)
    return env


def get_maxz(tif_path, target, interpretation, index):
    """Return (maxZError, nrmse_achieved_or_None) for this TIF+target+interpretation.

    For multi-band TIFs uses the minimum maxZError across all bands.
    Returns (None, None) if the TIF or target is absent from the index.
    """
    tif_entry = index.get(tif_path)
    if not tif_entry:
        return None, None

    target_key   = str(target)
    value_field  = "maxZError_formula" if interpretation == "formula" else "maxZError_search"
    nrmse_field  = "nrmse_achieved"    if interpretation == "search"  else None

    best_maxz  = None
    best_nrmse = None

    for band_data in tif_entry.values():
        if not isinstance(band_data, dict):
            continue
        t = band_data.get("nrmse_targets", {}).get(target_key, {})
        maxz = t.get(value_field)
        if maxz is None:
            continue
        if best_maxz is None or maxz < best_maxz:
            best_maxz  = maxz
            best_nrmse = t.get(nrmse_field) if nrmse_field else None

    return best_maxz, best_nrmse


def run_bench(tif_path, variant_label, log, env):
    """Run bench_comp for each NODATA_VALUE, appending output to log."""
    for nodata in NODATA_VALUES:
        print(
            f"\n>>> BENCH START: tif={tif_path} | variant={variant_label} | nodata={nodata}",
            file=log, flush=True,
        )
        cmd = [BENCH, tif_path] + BENCH_COMMON_ARGS + ["--max-nodata-pct", str(nodata)]
        proc = subprocess.run(cmd, capture_output=True, text=True, env=env)
        log.write(proc.stdout)
        log.write(proc.stderr)
        print(f">>> RUN END rc={proc.returncode}", file=log, flush=True)


# ---------------------------------------------------------------------------
# Per-TIF worker
# ---------------------------------------------------------------------------

def process_tif(tif_path, exp_name, tif_idx, index, env):
    """Process one TIF: lossless bench + all lossy variants. Returns log string."""
    log = io.StringIO()

    print("=" * 60,              file=log)
    print(f"Experiment: {exp_name}", file=log)
    print(f"TIF index:  {tif_idx}", file=log)
    print(f"TIF:        {tif_path}", file=log)
    print(f"Host:       {os.uname().nodename}", file=log)
    print("=" * 60,              file=log)

    from osgeo import gdal
    gdal.UseExceptions()
    ds = gdal.Open(tif_path, gdal.GA_ReadOnly)
    if ds is not None and ds.RasterCount > 1:
        print(f">>> SKIP: {tif_path} has {ds.RasterCount} bands (multi-band not supported)", file=log)
        ds = None
        return log.getvalue()
    ds = None

    work_dir = os.path.join(TEMP_DIR, f"{exp_name}_{tif_idx:04d}_{uuid.uuid4().hex[:8]}")
    os.makedirs(work_dir, exist_ok=True)

    # ---- lossless ----------------------------------------------------------
    print("\n>>> VARIANT: lossless", file=log)
    run_bench(tif_path, "lossless", log, env)

    # ---- lossy variants ----------------------------------------------------
    tif_in_index = tif_path in index
    if not tif_in_index:
        print(f"\n>>> SKIP all lossy variants: {tif_path} not found in index", file=log)
    else:
        for target in NRMSE_TARGETS:
            for interp in ("formula", "search"):
                maxz, nrmse_achieved = get_maxz(tif_path, target, interp, index)

                label = f"lerc_{interp}_t{int(target * 100):02d}pct"

                if maxz is None:
                    print(
                        f"\n>>> SKIP: variant={label} | target={target} | interp={interp} "
                        f"| not available in index",
                        file=log,
                    )
                    continue

                nrmse_info = (
                    f" | nrmse_achieved={nrmse_achieved:.4f}" if nrmse_achieved is not None else ""
                )
                print(
                    f"\n>>> VARIANT: {label} | target={target} | maxZError={maxz:.4f}{nrmse_info}",
                    file=log,
                )

                variant_dir = os.path.join(work_dir, label)
                os.makedirs(variant_dir, exist_ok=True)

                print(f">>> Generating lossy TIF (maxZError={maxz:.4f})", file=log, flush=True)
                gen = subprocess.run(
                    [
                        "python3", LOSSY_SCRIPT,
                        tif_path,
                        "--z-errors", str(maxz),
                        "--output-dir", variant_dir,
                        "--json-name", "stats.json",
                    ],
                    capture_output=True, text=True, env=env,
                )
                log.write(gen.stdout)
                log.write(gen.stderr)

                if gen.returncode != 0:
                    print(f">>> ERROR: generation failed (rc={gen.returncode})", file=log)
                    shutil.rmtree(variant_dir, ignore_errors=True)
                    continue

                lossy_tifs = glob_module.glob(os.path.join(variant_dir, "*.tif"))
                if not lossy_tifs:
                    print(">>> ERROR: no output TIF found after generation", file=log)
                    shutil.rmtree(variant_dir, ignore_errors=True)
                    continue

                lossy_tif = lossy_tifs[0]
                run_bench(lossy_tif, label, log, env)

                shutil.rmtree(variant_dir, ignore_errors=True)

    shutil.rmtree(work_dir, ignore_errors=True)

    print(f"\n$$$$$$$$TIFF_FINISHED$$$$$$$$", file=log)
    return log.getvalue()


# ---------------------------------------------------------------------------
# Worker plumbing
# ---------------------------------------------------------------------------

_WORKER_INDEX = None
_WORKER_ENV   = None


def _init_worker(cores_list, numa_node, counter, index, env):
    global _WORKER_CORE, _WORKER_NUMA_NODE, _WORKER_INDEX, _WORKER_ENV
    with counter.get_lock():
        idx = counter.value
        counter.value += 1
    _WORKER_CORE      = cores_list[idx]
    _WORKER_NUMA_NODE = numa_node
    _WORKER_INDEX     = index
    _WORKER_ENV       = env
    try:
        os.sched_setaffinity(0, {_WORKER_CORE})
    except (AttributeError, OSError):
        pass


def _worker_run(args):
    tif_path, exp_name, tif_idx = args
    return tif_path, exp_name, process_tif(tif_path, exp_name, tif_idx, _WORKER_INDEX, _WORKER_ENV)


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------

def main():
    parser = argparse.ArgumentParser(
        description="bench_comp sweep over lossless and index-derived LERC-lossy TIF variants.")
    parser.add_argument("--index",   required=True,
                        help="Path to lossy_index.json produced by build_lossy_index.py")
    parser.add_argument("--workers", type=int, default=1,
                        help="Parallel worker processes (default 1).")
    parser.add_argument("--node",    type=int, choices=[0, 1], default=0,
                        help="NUMA node to bind workers to (default 0).")
    args = parser.parse_args()

    with open(args.index) as f:
        raw = json.load(f)
    # Support both {tifs: {...}} wrapper (from build_lossy_index) and bare dict.
    index = raw.get("tifs", raw)

    pool = WORKER_CORES_NODE0 if args.node == 0 else WORKER_CORES_NODE1
    if args.workers > len(pool):
        parser.error(f"--workers > {len(pool)} not supported for node {args.node}")
    worker_cores = pool[:args.workers]

    os.makedirs(OUTDIR,    exist_ok=True)
    os.makedirs(TEMP_DIR,  exist_ok=True)

    # Build flat work list: (tif_path, exp_name, tif_idx_within_collection)
    work = []
    for name, _bs, globs, _widths in COLLECTIONS:
        tifs = expand_globs(globs)
        for idx, tif_path in enumerate(tifs, start=1):
            work.append((tif_path, name, idx))

    total = len(work)
    env   = make_env()
    print(f"Work items: {total}  workers={args.workers}  node={args.node}  cores={worker_cores}",
          flush=True)

    # Accumulate per-collection log buffers.
    coll_logs = {}   # exp_name -> list of (tif_idx, log_str)
    done = 0

    if args.workers == 1:
        global _WORKER_INDEX, _WORKER_ENV
        _WORKER_INDEX = index
        _WORKER_ENV   = env
        for item in work:
            tif_path, exp_name, tif_idx = item
            _, _, log_str = _worker_run(item)
            coll_logs.setdefault(exp_name, []).append((tif_idx, log_str))
            done += 1
            print(f"[{done}/{total}] {exp_name}  {os.path.basename(tif_path)}", flush=True)
    else:
        counter = mp.Value("i", 0)
        with ProcessPoolExecutor(
            max_workers=args.workers,
            initializer=_init_worker,
            initargs=(worker_cores, args.node, counter, index, env),
        ) as ex:
            futures = {ex.submit(_worker_run, item): item for item in work}
            for fut in as_completed(futures):
                tif_path, exp_name, log_str = fut.result()
                _, _, tif_idx = futures[fut]
                coll_logs.setdefault(exp_name, []).append((tif_idx, log_str))
                done += 1
                print(f"[{done}/{total}] {exp_name}  {os.path.basename(tif_path)}", flush=True)

    # Write one file per collection, TIFs in index order.
    for name, _bs, globs, _widths in COLLECTIONS:
        entries = coll_logs.get(name)
        if not entries:
            continue
        entries.sort(key=lambda x: x[0])
        out_path = os.path.join(OUTDIR, f"{name}.txt")
        with open(out_path, "w") as f:
            for _, log_str in entries:
                f.write(log_str)
        print(f"Saved {out_path}")


if __name__ == "__main__":
    main()
