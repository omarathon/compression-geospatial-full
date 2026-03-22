#!/usr/bin/env python3
import subprocess
import re
import sys

BENCH = "/home/omsst2/diss/compression-geospatial-full/build/bench_pipeline"
GDIR = "/maps/omsst2/diss"

configs = [
    ("srtm_45_15.tif",                                        256, 16000, "custom_direct_access",              "linearSumSimd"),
    ("srtm_45_15.tif",                                        256, 16000, "simdcomp_fused",                    "linearSumFused"),
    ("srtm_45_15.tif",                                        256, 16000, "FastPFor_fused_SIMDPFor+VariableByte", "linearSumFused"),
    ("JRC_TMF_AnnualChange_v1_1990_AFR_ID16_S10_E10.tif",    256, 16000, "custom_direct_access",              "linearSumSimd"),
    ("JRC_TMF_AnnualChange_v1_1990_AFR_ID16_S10_E10.tif",    256, 16000, "simdcomp_fused",                    "linearSumFused"),
    ("JRC_TMF_AnnualChange_v1_1990_AFR_ID16_S10_E10.tif",    256, 16000, "FastPFor_fused_SIMDPFor+VariableByte", "linearSumFused"),
    ("accessibility.tif",                                     64,  64000, "custom_direct_access",              "linearSumSimd"),
    ("accessibility.tif",                                     64,  64000, "simdcomp_fused",                    "linearSumFused"),
    ("accessibility.tif",                                     64,  64000, "FastPFor_fused_SIMDPFor+VariableByte", "linearSumFused"),
    ("slope-srtm_35_11.tif",                                  256, 16000, "custom_direct_access",              "linearSumSimd"),
    ("slope-srtm_35_11.tif",                                  256, 16000, "simdcomp_fused",                    "linearSumFused"),
    ("slope-srtm_35_11.tif",                                  256, 16000, "FastPFor_fused_SIMDPFor+VariableByte", "linearSumFused"),
]

results = []

for i, (tif, bs, nb, codec, atrans) in enumerate(configs):
    print(f"[{i+1}/{len(configs)}] {tif}  codec={codec}  atrans={atrans}", flush=True)

    cmd = [
        "/usr/bin/time", "-v",
        BENCH, f"{GDIR}/{tif}",
        "-b", str(bs), "-n", str(nb), "-r", "5",
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

    print(f"  meantimedec={dec_ns:.1f} ns  meantimetrans={trans_ns:.1f} ns  sum={sum_ns:.1f} ns  RSS={rss_kb} kB", flush=True)
    results.append((tif, codec, dec_ns, trans_ns, sum_ns, rss_kb))

print()
print(f"{'File':<55} {'Codec':<40} {'dec(ns)':>10} {'trans(ns)':>10} {'sum(ns)':>10} {'RSS(MB)':>9}")
print("-" * 140)
for tif, codec, dec, trans, s, rss in results:
    rss_mb = rss / 1024 if rss else None
    print(f"{tif:<55} {codec:<40} {dec:>10.1f} {trans:>10.1f} {s:>10.1f} {rss_mb:>9.1f}")
