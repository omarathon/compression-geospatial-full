#!/usr/bin/env python3
"""Verify uint16 codec sums against int32 ground truth.

Runs bench_pipeline with --trace-sums for each codec+transform pair
and diffs the per-block sums against the int32 baseline.

Usage:
    python3 scripts/verify_sums.py <tif_file> [options]
    python3 scripts/verify_sums.py --all [options]

Examples:
    python3 scripts/verify_sums.py ~/diss/geotiffs/srtm_45_15.tif
    python3 scripts/verify_sums.py --all -b 256 -n 1000
"""

import argparse
import subprocess
import sys
import os
from pathlib import Path

BENCH = os.environ.get("BENCH_PIPELINE", "./build/bench_pipeline")

# (codec_name, access_transform)
CODECS_UNDER_TEST = [
    ("custom_direct_access", "linearSumSimd"),
    ("simdcomp_fused", "linearSumFused"),
    ("FastPFor_fused_SIMDPFor+VariableByte", "linearSumFused"),
]

GROUND_TRUTH = ("custom_direct_access", "linearSumSimd")


def run_trace(tif, codec, atrans, block_size, num_blocks, extra_flags=None):
    cmd = [
        BENCH, tif,
        "-b", str(block_size),
        "-n", str(num_blocks),
        "-r", "1",
        "--trace-sums",
        "--icodec", codec,
        "--acodec", codec,
        "--atrans", atrans,
    ]
    if extra_flags:
        cmd.extend(extra_flags)
    result = subprocess.run(cmd, capture_output=True, text=True)
    if result.returncode != 0:
        print(f"  FAILED: {' '.join(cmd)}", file=sys.stderr)
        print(result.stderr[:500], file=sys.stderr)
        return None
    traces = {}
    for line in result.stdout.splitlines():
        if line.startswith("TRACE "):
            parts = dict(kv.split("=") for kv in line[len("TRACE "):].split())
            traces[int(parts["block"])] = int(parts["sum"])
    return traces


def verify_file(tif, block_size, num_blocks):
    name = Path(tif).name
    print(f"\n{'='*60}")
    print(f"  {name}  (blocks={num_blocks}, blocksize={block_size})")
    print(f"{'='*60}")

    gt_codec, gt_atrans = GROUND_TRUTH
    print(f"  ground truth: --force-int32 {gt_codec} + {gt_atrans}")
    gt = run_trace(tif, gt_codec, gt_atrans, block_size, num_blocks,
                   extra_flags=["--force-int32"])
    if gt is None:
        print(f"  SKIP: could not get ground truth for {name}")
        return False

    print(f"  got {len(gt)} blocks")
    all_pass = True

    for codec, atrans in CODECS_UNDER_TEST:
        test = run_trace(tif, codec, atrans, block_size, num_blocks)
        if test is None:
            print(f"  FAIL  {codec:45s}  (run error)")
            all_pass = False
            continue

        mismatches = []
        for block in sorted(gt.keys()):
            gt_sum = gt[block]
            test_sum = test.get(block)
            if test_sum is None:
                mismatches.append((block, gt_sum, "MISSING"))
            elif test_sum != gt_sum:
                mismatches.append((block, gt_sum, test_sum))

        if not mismatches:
            print(f"  PASS  {codec:45s}  ({len(gt)} blocks match)")
        else:
            print(f"  FAIL  {codec:45s}  ({len(mismatches)} mismatches)")
            for block, expected, actual in mismatches[:10]:
                print(f"        block={block}  expected={expected}  got={actual}")
            if len(mismatches) > 10:
                print(f"        ... and {len(mismatches) - 10} more")
            all_pass = False

    return all_pass


def find_tifs():
    """Find compatible TIF files (Int16, UInt16, Byte)."""
    search_dir = os.environ.get("GEOTIFF_DIR", os.path.expanduser("~/diss/geotiffs"))
    tifs = []
    for f in sorted(Path(search_dir).glob("*.tif")):
        try:
            info = subprocess.run(
                ["gdalinfo", str(f)],
                capture_output=True, text=True, timeout=10
            )
            for line in info.stdout.splitlines():
                if "Type=" in line:
                    for dt in ("Int16", "UInt16", "Byte"):
                        if f"Type={dt}" in line:
                            tifs.append(str(f))
                    break
        except Exception:
            pass
    return tifs


def main():
    parser = argparse.ArgumentParser(description="Verify uint16 codec sums vs int32 ground truth")
    parser.add_argument("tif", nargs="?", help="GeoTIFF file path")
    parser.add_argument("--all", action="store_true", help="Test all compatible TIFs in GEOTIFF_DIR")
    parser.add_argument("-b", "--blocksize", type=int, default=256, help="Block size (default: 256)")
    parser.add_argument("-n", "--numblocks", type=int, default=100, help="Number of blocks (default: 100)")
    args = parser.parse_args()

    if not args.tif and not args.all:
        parser.print_help()
        sys.exit(1)

    if args.all:
        tifs = find_tifs()
        if not tifs:
            print("No compatible TIFs found. Set GEOTIFF_DIR env var.", file=sys.stderr)
            sys.exit(1)
        print(f"Found {len(tifs)} compatible TIF(s)")
    else:
        tifs = [args.tif]

    all_pass = True
    for tif in tifs:
        if not verify_file(tif, args.blocksize, args.numblocks):
            all_pass = False

    print()
    if all_pass:
        print("ALL PASS")
    else:
        print("SOME FAILURES")
        sys.exit(1)


if __name__ == "__main__":
    main()
