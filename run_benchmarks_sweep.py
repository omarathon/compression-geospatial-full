#!/usr/bin/env python3
"""Sweep benchmarks across RSS thresholds for 16-bit or 32-bit pipeline.

Usage:
    python3 run_benchmarks_sweep.py --16bit
    python3 run_benchmarks_sweep.py --32bit
"""
import argparse
import subprocess
import re
import json
import math

BENCH = "/home/omsst2/diss/compression-geospatial-full/build/bench_pipeline"
GDIR = "/maps/omsst2/diss"

# (filename, blocksize)
TIFS_32BIT = [
    ("srtm_45_15.tif",                                     256),
    ("JRC_TMF_AnnualChange_v1_1990_AFR_ID16_S10_E10.tif",  256),
    ("accessibility.tif",                                   64),
    ("slope-srtm_35_11.tif",                                256),
]

TIFS_16BIT = [
    ("srtm_45_15.tif",                                     256),
    ("JRC_TMF_AnnualChange_v1_1990_AFR_ID16_S10_E10.tif",  256),
    ("slope-srtm_35_11.tif",                                256),
]

CODECS = [
    ("custom_direct_access",                "linearSumSimd"),
    ("simdcomp_fused",                      "linearSumFused"),
    ("FastPFor_fused_SIMDPFor+VariableByte", "linearSumFused"),
]

# Theoretical uncompressed RSS thresholds
RSS_THRESHOLDS_MB = [1, 4, 8, 16, 512, 1024, 2048]


def numblocks_for_rss(rss_bytes, blocksize, elem_bytes):
    """Compute numblocks = rss_bytes / (blocksize * blocksize * elem_bytes)."""
    block_bytes = blocksize * blocksize * elem_bytes
    return max(1, math.floor(rss_bytes / block_bytes))


def run_one(tif, blocksize, numblocks, codec, atrans):
    cmd = [
        "/usr/bin/time", "-v",
        "stdbuf", "-oL",
        BENCH, f"{GDIR}/{tif}",
        "-b", str(blocksize), "-n", str(numblocks), "-r", "5",
        "--icodec", codec, "--acodec", codec,
        "--ordering", "morton", "--itrans", "none",
        "--pattern", "linear", "--atrans", atrans,
    ]
    proc = subprocess.run(cmd, capture_output=True, text=True)
    out = proc.stdout + proc.stderr

    rss = re.search(r"Maximum resident set size \(kbytes\):\s*(\d+)", out)
    dec = re.search(r"meantimedec:([\d.]+)", out)
    trans = re.search(r"meantimetrans:([\d.]+)", out)

    rss_kb   = int(rss.group(1))   if rss   else None
    dec_ns   = float(dec.group(1)) if dec   else None
    trans_ns = float(trans.group(1)) if trans else None
    sum_ns   = dec_ns + trans_ns if (dec_ns is not None and trans_ns is not None) else None

    return {
        "tif": tif, "blocksize": blocksize, "numblocks": numblocks,
        "codec": codec, "atrans": atrans,
        "dec_ns": dec_ns, "trans_ns": trans_ns, "sum_ns": sum_ns,
        "rss_kb": rss_kb,
    }


def main():
    parser = argparse.ArgumentParser()
    group = parser.add_mutually_exclusive_group(required=True)
    group.add_argument("--16bit", dest="bits16", action="store_true")
    group.add_argument("--32bit", dest="bits32", action="store_true")
    args = parser.parse_args()

    if args.bits16:
        tifs = TIFS_16BIT
        elem_bytes = 2
        tag = "16bit"
    else:
        tifs = TIFS_32BIT
        elem_bytes = 4
        tag = "32bit"

    # Precompute all runs
    runs = []
    for tif, bs in tifs:
        for rss_mb in RSS_THRESHOLDS_MB:
            nb = numblocks_for_rss(rss_mb * 1024 * 1024, bs, elem_bytes)
            for codec, atrans in CODECS:
                runs.append((tif, bs, nb, rss_mb, codec, atrans))

    results = []
    for i, (tif, bs, nb, rss_mb, codec, atrans) in enumerate(runs):
        print(f"[{i+1}/{len(runs)}] {tif}  rss={rss_mb}MB  n={nb}  codec={codec}", flush=True)
        r = run_one(tif, bs, nb, codec, atrans)
        r["target_rss_mb"] = rss_mb
        results.append(r)
        print(f"  sum={r['sum_ns']:.1f} ns  RSS={r['rss_kb']} kB", flush=True)

    out_file = f"bench_sweep_{tag}.json"
    with open(out_file, "w") as f:
        json.dump(results, f, indent=2)
    print(f"\nResults saved to {out_file}")


if __name__ == "__main__":
    main()
