#!/usr/bin/env python3
"""Sweep benchmarks across RSS thresholds for 16-bit or 32-bit pipeline.

Each 'collection' is a named group of TIFs sharing a blocksize.
Per-collection output aggregates timing stats (mean + pstdev) over all TIFs
per band, and also includes the raw per-TIF results for offline aggregation.

Parallel execution:
  --workers N    Use N worker processes (default: 1 = sequential).
                 Each worker is pinned via taskset to a distinct AMD CCX on
                 NUMA node 0 (cores 0, 4, 8, …, stride 4). Max recommended = 8
                 for an EPYC 7702-class box (16 MiB L3 per 4-core CCX).
  --node N       NUMA node to bind workers to (default: 0). On a dual-socket
                 box, use 1 to run on the other socket without cross-socket
                 traffic.

Set --workers >1 only on an otherwise-idle box. Run a small sequential vs
parallel comparison first to confirm the timings haven't drifted.

Usage:
    python3 run_benchmarks_sweep_realdata.py
    python3 run_benchmarks_sweep_realdata.py --workers 8
"""
import argparse
import glob as glob_module
import json
import math
import multiprocessing as mp
import os
import re
import shutil
import statistics
import subprocess
from concurrent.futures import ProcessPoolExecutor, as_completed

BENCH = "/home/omsst2/diss/compression-geospatial-full/build/bench_pipeline"
DISS = "/maps/omsst2/diss"

# (collection_name, blocksize, [tif_glob_or_path, ...], [widths_to_bench])
# widths_to_bench: list of ints from {16, 32}.
#   16 → native type (no --force-int32)
#   32 → pass --force-int32
# COLLECTIONS = [
#     ("Fuieri_2014_Landsat8_Pano_B8",        256, [f"{DISS}/papers/rasterlite/landsat_pano/LC08_192029_20130702_B8.TIF"],                                             [16]),
#     ("Fuieri_2014_srtm",                    256, [f"{DISS}/papers/rasterlite/SRTM_Italy/srtm_italy_exact/srtm_tiles/SRTM_Italy_final.tif"],                          [16]),
#     ("Fuieri_2019_ETOPO1",                  256, [f"{DISS}/papers/rasterlite/ETOPO1/ETOPO1_Ice_g_geotiff.tif"],                                                      [16]),
#     # ("Fuieri_2019_Tuscany_DTM",             32, [f"{DISS}/papers/rasterlite/tuscany_dtm/quantized_2000.tif"],                                                       [32]),
#     ("Zalipynis_2018_0_Landsat8_B4_B5_Mosaic", 256, [f"{DISS}/papers/zalipynis/2018/landsat8_mosaic_B4.tif",
#                                                       f"{DISS}/papers/zalipynis/2018/landsat8_mosaic_B5.tif"],                                                       [16]),
#     ("Zalipynis_2018_1_Landsat8_B1",        256, [f"{DISS}/papers/zalipynis/2018_2/landsat/landsat8_path190_row031_stack.tif"],                                      [16]),
#     ("Zalipynis_2019_Landsat8_B4_Mosaic",   256, [f"{DISS}/papers/zalipynis/2019_landsat_band4_mosaic/exact/landsat8_mosaic_B4.tif"],                            [16]),
#     ("other_sentinel2_B2", 256, [f"{DISS}/others/sentinel2/Sentinel2_B02_*.tif"],                                                                   [16]),
#     ("other_sentinel2_B3", 256, [f"{DISS}/others/sentinel2/Sentinel2_B03_*.tif"],                                                                   [16]),
#     ("other_sentinel2_B4", 256, [f"{DISS}/others/sentinel2/Sentinel2_B04_*.tif"],                                                                   [16]),
#     ("other_sentinel2_B8", 256, [f"{DISS}/others/sentinel2/Sentinel2_B08_*.tif"],                                                                   [16]),
#     ("Zaytar_2025_Sentinel2_B2",  256, [f"{DISS}/papers/zaytar/2025/final/split_bands/Sentinel_*_B2.tif"],                                                                         [16]),
#     ("Zaytar_2025_Sentinel2_B3",  256, [f"{DISS}/papers/zaytar/2025/final/split_bands/Sentinel_*_B3.tif"],                                                                         [16]),
#     ("Zaytar_2025_Sentinel2_B4",  256, [f"{DISS}/papers/zaytar/2025/final/split_bands/Sentinel_*_B4.tif"],                                                                         [16]),
#     ("Zaytar_2025_Sentinel2_B8",  256, [f"{DISS}/papers/zaytar/2025/final/split_bands/Sentinel_*_B8.tif"],                                                                         [16]),
#     ("other_landsat8_B1",                   256, [f"{DISS}/others/landsat8/Landsat8_B1_*.tif"],                                                                   [16]),
#     ("other_landsat8_B2",                   256, [f"{DISS}/others/landsat8/Landsat8_B2_*.tif"],                                                                   [16]),
#     ("other_landsat8_B3",                   256, [f"{DISS}/others/landsat8/Landsat8_B3_*.tif"],                                                                   [16]),
#     ("other_landsat8_B4",                   256, [f"{DISS}/others/landsat8/Landsat8_B4_*.tif"],                                                                   [16]),
#     ("other_landsat8_B5",                   256, [f"{DISS}/others/landsat8/Landsat8_B5_*.tif"],                                                                   [16]),
#     ("other_landsat8_B8",                   256, [f"{DISS}/others/landsat8/Landsat8_B8_*.tif"],                                                                   [16]),
#     # ("other_sentinel2",                     256, [f"{DISS}/others/sentinel2/*.tif"],                                                                                 [16]),
#     ("other_srtm_highres",                  256, [f"{DISS}/others/srtm_highres/*.tif"],                                                                              [16]),
#     ("other_worldclim",                     256, [f"{DISS}/others/worldclim/*.tif"],                                                                                 [16]),
#     ("other_worldcover_int16",              256, [f"{DISS}/others/worldcover_int16/*.tif"],                                                                          [16]),
#     ("other_etopo1",                        256, [f"{DISS}/others/etopo1/*.tif"],                                                                                    [16]),
#     # ("other_etopo_highres_quant",           256, [f"{DISS}/others/etopo_highres_quant/*.tif"],                                                                       [32]),
#     # ("other_landscan",                      128, [f"{DISS}/others/landscan/*.tif"],                                                                                  [32]),
#     ("other_srtm",                          256, [f"{DISS}/others/srtm/*.tif"],                                                                                      [16]),
#     # ("other_worldcover",                    256, [f"{DISS}/others/worldcover/*.tif"],                                                                                [16]),
#     # # original sweep datasets
#     ("srtm_45_15",                          256, [f"{DISS}/srtm_45_15.tif"],                                                                                        [16]),
#     ("JRC_TMF",                             256, [f"{DISS}/JRC_TMF_AnnualChange_v1_1990_AFR_ID16_S10_E10.tif"],                                                      [16]),
#     # ("accessibility",                        64, [f"{DISS}/accessibility.tif"],                                                                                      [32]),
#     ("slope_srtm",                          256, [f"{DISS}/slope-srtm_35_11.tif"],                                                                                   [16]),
# ]

#just sentinel
COLLECTIONS = [
    ("other_sentinel2_B2", 256, [f"{DISS}/others/sentinel2/Sentinel2_B02_*.tif"],                                                                   [16]),
    ("other_sentinel2_B3", 256, [f"{DISS}/others/sentinel2/Sentinel2_B03_*.tif"],                                                                   [16]),
    ("other_sentinel2_B4", 256, [f"{DISS}/others/sentinel2/Sentinel2_B04_*.tif"],                                                                   [16]),
    ("other_sentinel2_B8", 256, [f"{DISS}/others/sentinel2/Sentinel2_B08_*.tif"],                                                                   [16]),
    ("Zaytar_2025_Sentinel2_B2",  256, [f"{DISS}/papers/zaytar/2025/final/split_bands/Sentinel_*_B2.tif"],                                                                         [16]),
    ("Zaytar_2025_Sentinel2_B3",  256, [f"{DISS}/papers/zaytar/2025/final/split_bands/Sentinel_*_B3.tif"],                                                                         [16]),
    ("Zaytar_2025_Sentinel2_B4",  256, [f"{DISS}/papers/zaytar/2025/final/split_bands/Sentinel_*_B4.tif"],                                                                         [16]),
    ("Zaytar_2025_Sentinel2_B8",  256, [f"{DISS}/papers/zaytar/2025/final/split_bands/Sentinel_*_B8.tif"],                                                                         [16]),
]

# CODECS = [
#     ("custom_direct_access",                            "linearSumSimd"),
#     ("simdcomp_fused",                                  "linearSumFused"),
#     # ("FastPFor_fused_SIMDPFor+VariableByte",                              "linearSumFused"),
#     ("FastPFor_fused_corrected_global_b_SIMDPFor+VariableByte",           "linearSumFused"),
#     ("FastPFor_fused_corrected_adaptive_b_SIMDPFor+VariableByte",         "linearSumFused"),
#     # ("simdcomp_fused_delta_local",                                        "linearSumFused"),
#     # ("simdcomp_fused_delta_carry",                                        "linearSumFused"),
#     # ("FastPFor_fused_corrected_delta_local_SIMDPFor+VariableByte",        "linearSumFused"),
#     # ("FastPFor_fused_corrected_delta_carry_SIMDPFor+VariableByte",        "linearSumFused"),
#     ("simdcomp_fused_for_global",                                         "linearSumFused"),
#     ("FastPFor_fused_corrected_for_global_global_b",                      "linearSumFused"),
#     ("FastPFor_fused_corrected_for_global_adaptive_b",                    "linearSumFused"),
# ]

CODECS = [
   ("custom_direct_access",                            "linearSumSimd"),
   ("simdcomp_fused",                                  "linearSumFused"),
    # ("FastPFor_fused_SIMDPFor+VariableByte",                              "linearSumFused"),
   ("FastPFor_fused_corrected_global_b_SIMDPFor+VariableByte",           "linearSumFused"),
   ("FastPFor_fused_corrected_adaptive_b_SIMDPFor+VariableByte",         "linearSumFused"),
    # ("simdcomp_fused_delta_local",                                        "linearSumFused"),
    # ("simdcomp_fused_delta_carry",                                        "linearSumFused"),
    # ("FastPFor_fused_corrected_delta_local_SIMDPFor+VariableByte",        "linearSumFused"),
    # ("FastPFor_fused_corrected_delta_carry_SIMDPFor+VariableByte",        "linearSumFused"),
   ("simdcomp_fused_for_global",                                         "linearSumFused"),
   ("FastPFor_fused_corrected_for_global_global_b",                      "linearSumFused"),
   ("FastPFor_fused_corrected_for_global_adaptive_b",                    "linearSumFused"),
    ("FastPFor_fused_corrected_for_global_adaptive_b_p256",                    "linearSumFused"),
    ("FastPFor_fused_corrected_for_global_adaptive_b_p512",                    "linearSumFused"),
    ("FastPFor_fused_corrected_for_global_adaptive_b_p1024",                    "linearSumFused"),
    ("FastPFor_fused_corrected_for_global_adaptive_b_p2048",                    "linearSumFused"),
    # ("FastPFor_fused_corrected_for_global_adaptive_b_p4096",                    "linearSumFused"),
    # ("FastPFor_fused_corrected_for_global_adaptive_b_p8192",                    "linearSumFused")
    ("FastPFor_fused_corrected_for_global_adaptive_b_w128",                   "linearSumFused"),
    ("FastPFor_fused_corrected_for_global_adaptive_b_w64",                    "linearSumFused"),
    # ("FastPFor_fused_corrected_for_global_adaptive_b_w32",                    "linearSumFused"),
    ("simdcomp_fused_for_global_w128",                                         "linearSumFused"),
   ("simdcomp_fused_for_global_w64",                                         "linearSumFused"),
#    ("simdcomp_fused_for_global_w32",                                         "linearSumFused"),
]

# Theoretical uncompressed RSS thresholds
# RSS_THRESHOLDS_MB = [1, 4, 8, 16, 64, 256, 512, 1024]
RSS_THRESHOLDS_MB = [8, 16, 32]

# Cores to pin workers to. Stride 4 places each worker on a distinct CCX
# (Zen 2: 4 cores per CCX share 16 MiB L3). All cores 0–63 are on NUMA 0.
# 8 workers = 8 isolated CCXs. Bump to 16 (stride 4, cores 0–60) only if you
# confirmed bandwidth headroom on the high-RSS rows.
WORKER_CORES_NODE0 = [0, 4, 8, 12, 16, 20, 24, 28]
WORKER_CORES_NODE1 = [64, 68, 72, 76, 80, 84, 88, 92]


def numblocks_for_rss(rss_bytes, blocksize, elem_bytes):
    """Compute numblocks = rss_bytes / (blocksize * blocksize * elem_bytes)."""
    block_bytes = blocksize * blocksize * elem_bytes
    return max(1, math.floor(rss_bytes / block_bytes))


def expand_globs(patterns):
    """Expand a list of glob patterns; keep literal paths that match no glob."""
    paths = []
    for p in patterns:
        matched = sorted(glob_module.glob(p))
        paths.extend(matched if matched else [p])
    return paths


def run_one(tif_path, blocksize, numblocks, codec, atrans, force_int32=False,
            cpu_core=None, numa_node=None):
    """Run bench_pipeline on one TIF; return a list of per-band result dicts.

    If cpu_core is set, the process is pinned to that single core via taskset.
    If numa_node is set, memory is bound to that node via numactl --membind.
    """
    cmd = []
    if cpu_core is not None and shutil.which("taskset"):
        cmd += ["taskset", "-c", str(cpu_core)]
    if numa_node is not None and shutil.which("numactl"):
        cmd += ["numactl", f"--membind={numa_node}",
                f"--cpunodebind={numa_node}"]
    cmd += [
        "/usr/bin/time", "-v",
        "stdbuf", "-oL",
        BENCH, tif_path,
        "-b", str(blocksize), "-n", str(numblocks), "-r", "10", "--rs", "3",
        "--icodec", codec, "--acodec", codec,
        "--ordering", "default", "--itrans", "none",
        "--pattern", "linear", "--atrans", atrans,
        "--normalize",
    ]
    if force_int32:
        cmd.append("--force-int32")
    # Prevent GDAL, OpenMP, or any transitively-linked threading library from
    # spawning extra threads that would escape taskset pinning and compete with
    # other workers for cache / CPU time.
    env = os.environ.copy()
    env["OMP_NUM_THREADS"] = "1"
    env["GDAL_NUM_THREADS"] = "1"
    env["OPENBLAS_NUM_THREADS"] = "1"
    env["MKL_NUM_THREADS"] = "1"
    proc = subprocess.run(cmd, capture_output=True, text=True, env=env)
    out = proc.stdout + proc.stderr

    rss = re.search(r"Maximum resident set size \(kbytes\):\s*(\d+)", out)
    rss_kb = int(rss.group(1)) if rss else None

    blocks = re.split(r"\*\*BENCHMARK ACCESS\*\*", out)
    results = []
    for block in blocks[1:]:
        band_m  = re.search(r"band=(\d+)",                  block)
        dec_m   = re.search(r"meantimedec:([\d.]+)",        block)
        trans_m = re.search(r"meantimetrans:([\d.]+)",      block)
        cr_m     = re.search(r"meancompratio:([\d.]+)",         block)
        excpt_m  = re.search(r"meanexceptperblock:([-\d.]+)",   block)

        band     = int(band_m.group(1))    if band_m    else None
        dec_ns   = float(dec_m.group(1))   if dec_m     else None
        trans_ns = float(trans_m.group(1)) if trans_m   else None
        sum_ns   = dec_ns + trans_ns if (dec_ns is not None and trans_ns is not None) else None
        comp_ratio = float(cr_m.group(1))  if cr_m      else None
        # -1.0 means the codec doesn't support exception counting; store as None.
        _ep = float(excpt_m.group(1)) if excpt_m else None
        except_per_block = None if (_ep is None or _ep < 0) else _ep

        results.append({
            "tif": tif_path, "band": band,
            "dec_ns": dec_ns, "trans_ns": trans_ns, "sum_ns": sum_ns,
            "rss_kb": rss_kb, "comp_ratio": comp_ratio,
            "mean_except_per_block": except_per_block,
        })
    return results


def aggregate_band_results(band_results):
    """Given a list of per-tif dicts for one band, return aggregate stats."""
    def stats(vals):
        clean = [v for v in vals if v is not None]
        if not clean:
            return None, None
        return statistics.mean(clean), statistics.pstdev(clean)

    dec_vals    = [r["dec_ns"]               for r in band_results]
    trans_vals  = [r["trans_ns"]             for r in band_results]
    sum_vals    = [r["sum_ns"]               for r in band_results]
    cr_vals     = [r["comp_ratio"]           for r in band_results]
    excpt_vals  = [r.get("mean_except_per_block") for r in band_results]

    mean_dec,    std_dec    = stats(dec_vals)
    mean_trans,  std_trans  = stats(trans_vals)
    mean_sum,    std_sum    = stats(sum_vals)
    mean_cr,     std_cr     = stats(cr_vals)
    mean_excpt,  std_excpt  = stats(excpt_vals)

    return {
        "mean_dec_ns":          mean_dec,   "std_dec_ns":          std_dec,
        "mean_trans_ns":        mean_trans, "std_trans_ns":        std_trans,
        "mean_sum_ns":          mean_sum,   "std_sum_ns":          std_sum,
        "mean_comp_ratio":      mean_cr,    "std_comp_ratio":      std_cr,
        "mean_except_per_block": mean_excpt, "std_except_per_block": std_excpt,
        "per_tif": [
            {"tif": r["tif"], "dec_ns": r["dec_ns"],
             "trans_ns": r["trans_ns"], "sum_ns": r["sum_ns"],
             "rss_kb": r["rss_kb"], "comp_ratio": r["comp_ratio"],
             "mean_except_per_block": r.get("mean_except_per_block")}
            for r in band_results
        ],
    }


# ── Worker entry point (module-level so it's picklable for ProcessPoolExecutor) ──
#
# Pinning strategy: each worker process is assigned exactly one core for its
# entire lifetime via the `initializer` hook. The core is stored in this
# module-level global (per-process, since each worker is a separate Python
# interpreter). `_worker_run` reads it for every taskset invocation. This
# guarantees that two workers never share a physical core, even when work
# items finish at uneven rates.

_WORKER_CORE = None
_WORKER_NUMA_NODE = None


def _init_worker(cores_list, numa_node, counter):
    """Called once when each worker process starts. Atomically claims the next
    core from `cores_list` (length must be >= max_workers)."""
    global _WORKER_CORE, _WORKER_NUMA_NODE
    with counter.get_lock():
        idx = counter.value
        counter.value += 1
    _WORKER_CORE = cores_list[idx]
    _WORKER_NUMA_NODE = numa_node


def _worker_run(args):
    """Run one TIF unit. Returns (key, tif_path, band_result_rows).

    `key` identifies the (collection, blocksize, numblocks, rss_mb, codec,
    atrans, width) bucket so the parent can aggregate per-band stats across
    TIFs in the same collection.
    """
    (name, bs, tif_path, width, nb, rss_mb, codec, atrans, force_int32) = args
    rows = run_one(tif_path, bs, nb, codec, atrans, force_int32,
                   cpu_core=_WORKER_CORE, numa_node=_WORKER_NUMA_NODE)
    key = (name, bs, nb, rss_mb, codec, atrans, width)
    return key, tif_path, rows


def main():
    parser = argparse.ArgumentParser(
        description="Sweep benchmarks across RSS thresholds.")
    parser.add_argument("--workers", type=int, default=1,
                        help="Number of parallel worker processes (default 1)."
                             " Each is pinned to a distinct CCX. Max 8 "
                             "recommended on EPYC 7702.")
    parser.add_argument("--node", type=int, choices=[0, 1], default=0,
                        help="NUMA node to bind workers to (default 0).")
    parser.add_argument("--output", default="bench_sweep.json",
                        help="Output JSON path (default bench_sweep.json).")
    args = parser.parse_args()

    if args.workers < 1:
        parser.error("--workers must be >= 1")

    worker_cores_pool = (WORKER_CORES_NODE0 if args.node == 0
                          else WORKER_CORES_NODE1)
    if args.workers > len(worker_cores_pool):
        parser.error(
            f"--workers > {len(worker_cores_pool)} not supported by the "
            f"default core list for node {args.node}. Edit WORKER_CORES_NODE{args.node} "
            f"if you really want more.")

    worker_cores = worker_cores_pool[:args.workers]
    print(f"workers={args.workers}  node={args.node}  pinned cores={worker_cores}",
          flush=True)

    # Expand globs and enumerate all (collection, width) combos up front.
    expanded = [
        (name, bs, expand_globs(globs), width, width == 32)
        for name, bs, globs, widths in COLLECTIONS
        for width in widths
    ]

    # Build the full work list: one item per (collection × rss × codec × tif).
    # Cores are NOT attached per-item; each worker is pinned for its lifetime
    # via _init_worker (see above), so it always uses the same physical core
    # regardless of which item it picks up next.
    work = []
    for name, bs, tifs, width, force_int32 in expanded:
        elem_bytes = width // 8
        for rss_mb in RSS_THRESHOLDS_MB:
            nb = numblocks_for_rss(rss_mb * 1024 * 1024, bs, elem_bytes)
            for codec, atrans in CODECS:
                for tif_path in tifs:
                    work.append((name, bs, tif_path, width, nb, rss_mb,
                                 codec, atrans, force_int32))

    total = len(work)
    results_by_key = {}  # key -> {tif_path -> rows}
    done_count = 0

    if args.workers == 1:
        # Sequential path: simulate a single-worker pool by pinning the parent
        # process itself to the first core for the duration of the run.
        global _WORKER_CORE, _WORKER_NUMA_NODE
        _WORKER_CORE = worker_cores[0]
        _WORKER_NUMA_NODE = args.node
        for w in work:
            key, tif_path, rows = _worker_run(w)
            results_by_key.setdefault(key, {}).setdefault(tif_path, []).extend(rows)
            done_count += 1
            print(f"[{done_count}/{total}] {key[0]}  rss={key[3]}MB  "
                  f"codec={key[4]}  tif={os.path.basename(tif_path)}",
                  flush=True)
    else:
        # Each worker process atomically claims one core from `worker_cores`
        # via _init_worker. The counter ensures distinct IDs without races.
        counter = mp.Value('i', 0)
        with ProcessPoolExecutor(
                max_workers=args.workers,
                initializer=_init_worker,
                initargs=(worker_cores, args.node, counter)) as ex:
            futures = {ex.submit(_worker_run, w): w for w in work}
            for fut in as_completed(futures):
                key, tif_path, rows = fut.result()
                results_by_key.setdefault(key, {}).setdefault(tif_path, []).extend(rows)
                done_count += 1
                print(f"[{done_count}/{total}] {key[0]}  rss={key[3]}MB  "
                      f"codec={key[4]}  tif={os.path.basename(tif_path)}",
                      flush=True)

    # Aggregate. Each key bucket contains per-tif row lists; flatten per band.
    results = []
    for key in sorted(results_by_key.keys()):
        name, bs, nb, rss_mb, codec, atrans, width = key
        per_band = {}
        for tif_path, rows in results_by_key[key].items():
            for r in rows:
                per_band.setdefault(r["band"], []).append(r)
        for band, band_results in sorted(per_band.items()):
            agg = aggregate_band_results(band_results)
            results.append({
                "collection": name,
                "blocksize": bs,
                "numblocks": nb,
                "codec": codec,
                "atrans": atrans,
                "band": band,
                "width": width,
                "target_rss_mb": rss_mb,
                **agg,
            })

    with open(args.output, "w") as f:
        json.dump(results, f, indent=2)
    print(f"\nResults saved to {args.output}")


if __name__ == "__main__":
    main()
