#!/usr/bin/env python3
"""Sweep benchmarks across RSS thresholds for 16-bit or 32-bit pipeline.

Each 'collection' is a named group of TIFs sharing a blocksize.
Per-collection output aggregates timing stats (mean + pstdev) over all TIFs
per band, and also includes the raw per-TIF results for offline aggregation.

Usage:
    python3 run_benchmarks_sweep_realdata.py --16bit
    python3 run_benchmarks_sweep_realdata.py --32bit
"""
import argparse
import glob as glob_module
import json
import math
import re
import statistics
import subprocess

BENCH = "/home/omsst2/diss/compression-geospatial-full/build/bench_pipeline"
DISS = "/maps/omsst2/diss"

# (collection_name, blocksize, [tif_glob_or_path, ...], [widths_to_bench])
# widths_to_bench: list of ints from {16, 32}.
#   16 → native type (no --force-int32)
#   32 → pass --force-int32
COLLECTIONS = [
    ("Fuieri_2014_Landsat8_Pano_B8",        256, [f"{DISS}/papers/rasterlite/landsat_pano/LC08_192029_20130702_B8.TIF"],                                             [16]),
    ("Fuieri_2014_srtm",                    256, [f"{DISS}/papers/rasterlite/SRTM_Italy/srtm_italy_exact/srtm_tiles/SRTM_Italy_final.tif"],                          [16]),
    ("Fuieri_2019_ETOPO1",                  256, [f"{DISS}/papers/rasterlite/ETOPO1/ETOPO1_Ice_g_geotiff.tif"],                                                      [16]),
    # ("Fuieri_2019_Tuscany_DTM",             32, [f"{DISS}/papers/rasterlite/tuscany_dtm/quantized_2000.tif"],                                                       [32]), # min=-10771.0  max=4107244.0 => range = 4118015 => only 32x32 fits in uint32 range for sum
    ("Zalipynis_2018_0_Landsat8_B4_B5_Mosaic", 256, [f"{DISS}/papers/zalipynis/2018/landsat8_mosaic_B4.tif",
                                                      f"{DISS}/papers/zalipynis/2018/landsat8_mosaic_B5.tif"],                                                       [16]),
    ("Zalipynis_2018_1_Landsat8_B1",        256, [f"{DISS}/papers/zalipynis/2018_2/landsat/landsat8_path190_row031_stack.tif"],                                      [16]),
    ("Zalipynis_2019_Landsat8_B4_Mosaic",   256, [f"{DISS}/papers/zalipynis/2019_landsat_band4_mosaic/exact/landsat8_mosaic_B4.tif"],                                [16]),
    # ("Zaytar_2025_Sentinel2_B2_B3_B4_B8",  256, [f"{DISS}/papers/zaytar/2025/final/*.tif"],                                                                         [16]),
    ("other_landsat8",                      256, [f"{DISS}/others/landsat8/*.tif"],                                                                                  [16]),
    # ("other_sentinel2",                     256, [f"{DISS}/others/sentinel2/*.tif"],                                                                                 [16]),
    ("other_srtm_highres",                  256, [f"{DISS}/others/srtm_highres/*.tif"],                                                                              [16]),
    ("other_worldclim",                     256, [f"{DISS}/others/worldclim/*.tif"],                                                                                 [16]),
    ("other_worldcover_int16",              256, [f"{DISS}/others/worldcover_int16/*.tif"],                                                                          [16]),
    ("other_etopo1",                        256, [f"{DISS}/others/etopo1/*.tif"],                                                                                    [16]),
    # ("other_etopo_highres_quant",           256, [f"{DISS}/others/etopo_highres_quant/*.tif"],                                                                       [32]),
    # ("other_landscan",                      128, [f"{DISS}/others/landscan/*.tif"],                                                                                  [32]), # global min=0.0  global max=135253.0 => range = 135253 => 128x128 works for uint32 sum
    ("other_srtm",                          256, [f"{DISS}/others/srtm/*.tif"],                                                                                      [16]),
    ("other_worldcover",                    256, [f"{DISS}/others/worldcover/*.tif"],                                                                                [16]),
    # # original sweep datasets
    ("srtm_45_15",                          256, [f"{DISS}/srtm_45_15.tif"],                                                                                        [16]),
    ("JRC_TMF",                             256, [f"{DISS}/JRC_TMF_AnnualChange_v1_1990_AFR_ID16_S10_E10.tif"],                                                      [16]),
    # ("accessibility",                        64, [f"{DISS}/accessibility.tif"],                                                                                      [32]),
    ("slope_srtm",                          256, [f"{DISS}/slope-srtm_35_11.tif"],                                                                                   [16]),
]

CODECS = [
    ("custom_direct_access",                            "linearSumSimd"),
    ("simdcomp_fused",                                  "linearSumFused"),
    # ("FastPFor_fused_SIMDPFor+VariableByte",             "linearSumFused"),
    ("FastPFor_fused_corrected_SIMDPFor+VariableByte",   "linearSumFused"),
    ("simdcomp_fused_delta_local",                       "linearSumFused"),
    ("simdcomp_fused_delta_carry",                       "linearSumFused"),
    ("FastPFor_fused_corrected_delta_local_SIMDPFor+VariableByte",  "linearSumFused"),
    ("FastPFor_fused_corrected_delta_carry_SIMDPFor+VariableByte",  "linearSumFused"),
]

# Theoretical uncompressed RSS thresholds
RSS_THRESHOLDS_MB = [1, 4, 8, 16, 64, 256, 512, 1024]


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


def run_one(tif_path, blocksize, numblocks, codec, atrans, force_int32=False):
    """Run bench_pipeline on one TIF; return a list of per-band result dicts."""
    cmd = [
        "/usr/bin/time", "-v",
        "stdbuf", "-oL",
        BENCH, tif_path,
        "-b", str(blocksize), "-n", str(numblocks), "-r", "5",
        "--icodec", codec, "--acodec", codec,
        "--ordering", "morton", "--itrans", "none",
        "--pattern", "linear", "--atrans", atrans,
        "--normalize",
    ]
    if force_int32:
        cmd.append("--force-int32")
    proc = subprocess.run(cmd, capture_output=True, text=True)
    out = proc.stdout + proc.stderr

    rss = re.search(r"Maximum resident set size \(kbytes\):\s*(\d+)", out)
    rss_kb = int(rss.group(1)) if rss else None

    # Each band produces one **BENCHMARK ACCESS** block.
    blocks = re.split(r"\*\*BENCHMARK ACCESS\*\*", out)
    results = []
    for block in blocks[1:]:  # skip text before first marker
        band_m  = re.search(r"band=(\d+)",           block)
        dec_m   = re.search(r"meantimedec:([\d.]+)",  block)
        trans_m = re.search(r"meantimetrans:([\d.]+)", block)

        band     = int(band_m.group(1))     if band_m  else None
        dec_ns   = float(dec_m.group(1))   if dec_m   else None
        trans_ns = float(trans_m.group(1)) if trans_m else None
        sum_ns   = dec_ns + trans_ns if (dec_ns is not None and trans_ns is not None) else None

        results.append({
            "tif": tif_path, "band": band,
            "dec_ns": dec_ns, "trans_ns": trans_ns, "sum_ns": sum_ns,
            "rss_kb": rss_kb,
        })
    return results


def aggregate_band_results(band_results):
    """Given a list of per-tif dicts for one band, return aggregate stats."""
    def stats(vals):
        clean = [v for v in vals if v is not None]
        if not clean:
            return None, None
        return statistics.mean(clean), statistics.pstdev(clean)

    dec_vals   = [r["dec_ns"]   for r in band_results]
    trans_vals = [r["trans_ns"] for r in band_results]
    sum_vals   = [r["sum_ns"]   for r in band_results]

    mean_dec,   std_dec   = stats(dec_vals)
    mean_trans, std_trans = stats(trans_vals)
    mean_sum,   std_sum   = stats(sum_vals)

    return {
        "mean_dec_ns":   mean_dec,   "std_dec_ns":   std_dec,
        "mean_trans_ns": mean_trans, "std_trans_ns": std_trans,
        "mean_sum_ns":   mean_sum,   "std_sum_ns":   std_sum,
        "per_tif": [
            {"tif": r["tif"], "dec_ns": r["dec_ns"],
             "trans_ns": r["trans_ns"], "sum_ns": r["sum_ns"],
             "rss_kb": r["rss_kb"]}
            for r in band_results
        ],
    }


def main():
    parser = argparse.ArgumentParser()
    args = parser.parse_args()

    # Expand globs and enumerate all (collection, width) combos up front.
    expanded = [
        (name, bs, expand_globs(globs), width, width == 32)
        for name, bs, globs, widths in COLLECTIONS
        for width in widths
    ]

    # Precompute all runs; elem_bytes comes from the per-entry width.
    runs = []
    for name, bs, tifs, width, force_int32 in expanded:
        elem_bytes = width // 8
        for rss_mb in RSS_THRESHOLDS_MB:
            nb = numblocks_for_rss(rss_mb * 1024 * 1024, bs, elem_bytes)
            for codec, atrans in CODECS:
                runs.append((name, bs, tifs, width, nb, rss_mb, codec, atrans, force_int32))

    results = []
    total_tif_runs = sum(len(r[2]) for r in runs)
    tif_run_idx = 0

    for name, bs, tifs, width, nb, rss_mb, codec, atrans, force_int32 in runs:
        # per_band: band -> list of per-tif result dicts
        per_band = {}
        for tif_path in tifs:
            tif_run_idx += 1
            print(f"[{tif_run_idx}/{total_tif_runs}] {name}  {tif_path}  "
                  f"width={width}bit  rss={rss_mb}MB  n={nb}  codec={codec}", flush=True)
            for r in run_one(tif_path, bs, nb, codec, atrans, force_int32):
                per_band.setdefault(r["band"], []).append(r)
                sum_str = f"{r['sum_ns']:.1f}" if r["sum_ns"] is not None else "N/A"
                print(f"  band={r['band']}  sum={sum_str} ns  RSS={r['rss_kb']} kB",
                      flush=True)

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

    with open("bench_sweep.json", "w") as f:
        json.dump(results, f, indent=2)
    print("\nResults saved to bench_sweep.json")


if __name__ == "__main__":
    main()
