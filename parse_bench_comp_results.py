#!/usr/bin/env python3
"""Parse bench_comp output logs and produce per-(ordering, nodata) CSVs.

Generates 4 CSV files, one for each (ordering, nodata%) combination.
Each CSV contains matrices for: lossless, LERC 5/10/15/30%, median 3x3/5x5/7x7.
Median matrices include a "Mean NRMSE" column (requires --index).

Usage:
    python3 parse_bench_comp_results.py <log_dir> [--index lossy_index.json] [--output-dir .]
"""

import argparse
import csv
import json
import os
import re
import sys
from collections import defaultdict

LERC_VARIANTS = [
    "lossless",
    "lerc_formula_t05pct",
    "lerc_formula_t10pct",
    "lerc_formula_t15pct",
    "lerc_formula_t30pct",
]

MEDIAN_VARIANTS = [
    "median_k3",
    "median_k5",
    "median_k7",
]

VARIANTS = LERC_VARIANTS + MEDIAN_VARIANTS

VARIANT_LABELS = {
    "lossless":            "Compression ratio (lossless)",
    "lerc_formula_t05pct": "Compression ratio (NRMSE <= 5%)",
    "lerc_formula_t10pct": "Compression ratio (NRMSE <= 10%)",
    "lerc_formula_t15pct": "Compression ratio (NRMSE <= 15%)",
    "lerc_formula_t30pct": "Compression ratio (NRMSE <= 30%)",
    "median_k3":           "Compression ratio (Median 3x3)",
    "median_k5":           "Compression ratio (Median 5x5)",
    "median_k7":           "Compression ratio (Median 7x7)",
}

MEDIAN_KERNEL = {
    "median_k3": "3",
    "median_k5": "5",
    "median_k7": "7",
}

RE_EXPERIMENT  = re.compile(r"^Experiment:\s+(.+)$")
RE_TIF_PATH    = re.compile(r"^TIF:\s+(.+)$")
RE_BENCH_START = re.compile(r"^>>> BENCH START:.*\|\s*variant=(\S+)\s*\|\s*nodata=(\d+)")
RE_BENCH_META  = re.compile(r"ordering=(\w+)")
RE_CODEC_LINE  = re.compile(r"^(\d+)=(.+)$")
RE_RESULT_LINE = re.compile(r"^c:(\d+),n:\d+,cfmean:([\d.eE+\-]+),")


def parse_logs(log_dir):
    """Return (data, codec_order, collection_tifs).

    data[(ordering, nodata)][collection][variant][codec] = [cfmean, ...]
    codec_order: list of codec names in encounter order.
    collection_tifs[collection] = set of TIF paths seen.
    """
    data = defaultdict(
        lambda: defaultdict(
            lambda: defaultdict(
                lambda: defaultdict(list))))

    codec_order      = []
    codec_order_set  = set()
    collection_tifs  = defaultdict(set)

    for fname in sorted(os.listdir(log_dir)):
        if not fname.endswith(".txt"):
            continue
        with open(os.path.join(log_dir, fname), encoding="utf-8", errors="replace") as f:
            lines = f.readlines()

        current_collection = None
        current_variant    = None
        current_nodata     = None
        current_ordering   = None
        in_bench           = False
        next_is_meta       = False
        collecting_codecs  = False
        codec_map          = {}

        for line in lines:
            line = line.rstrip("\n").rstrip()

            m = RE_EXPERIMENT.match(line)
            if m:
                current_collection = m.group(1).strip()
                current_variant = current_nodata = current_ordering = None
                continue

            m = RE_TIF_PATH.match(line)
            if m and current_collection:
                collection_tifs[current_collection].add(m.group(1).strip())
                continue

            m = RE_BENCH_START.match(line)
            if m:
                current_variant   = m.group(1).strip()
                current_nodata    = m.group(2).strip()
                in_bench          = True
                codec_map         = {}
                collecting_codecs = False
                next_is_meta      = False
                continue

            if not in_bench:
                continue

            if line.startswith("**BENCHMARK**"):
                codec_map         = {}
                collecting_codecs = False
                next_is_meta      = True
                continue

            if next_is_meta:
                next_is_meta = False
                m = RE_BENCH_META.search(line)
                current_ordering = m.group(1) if m else "unknown"
                continue

            if line == "*CODECS:*":
                collecting_codecs = True
                continue

            if line == "*ENDCODECS*":
                collecting_codecs = False
                continue

            if collecting_codecs:
                m = RE_CODEC_LINE.match(line)
                if m:
                    idx, name = int(m.group(1)), m.group(2).strip()
                    codec_map[idx] = name
                    if name not in codec_order_set:
                        codec_order.append(name)
                        codec_order_set.add(name)
                continue

            m = RE_RESULT_LINE.match(line)
            if m and current_collection and current_variant and current_ordering and current_nodata:
                ci     = int(m.group(1))
                cfmean = float(m.group(2))
                name   = codec_map.get(ci)
                if name:
                    key = (current_ordering, current_nodata)
                    data[key][current_collection][current_variant][name].append(cfmean)
                continue

            if line.startswith(">>> RUN END"):
                in_bench = False
                continue

    return data, codec_order, collection_tifs


def build_median_nrmse(index, collection_tifs):
    """Return median_nrmse[collection][kernel_str] = mean NRMSE across TIFs."""
    result = defaultdict(dict)
    for coll, tif_paths in collection_tifs.items():
        for kernel in ("3", "5", "7"):
            values = []
            for tif in tif_paths:
                entry = index.get(tif, {})
                nrmse = entry.get("1", {}).get("median_filter_nrmse", {}).get(kernel)
                if nrmse is not None:
                    values.append(nrmse)
            if values:
                result[coll][kernel] = sum(values) / len(values)
    return result


def mean(values):
    return sum(values) / len(values) if values else None


def fmt_cr(v):
    return f"{round(v * 100)}%" if v is not None else ""


def fmt_nrmse(v):
    return f"{v * 100:.1f}%" if v is not None else ""


def used_codecs_for(data_slice, codec_order):
    seen = set()
    for coll_data in data_slice.values():
        for var_data in coll_data.values():
            seen.update(var_data.keys())
    return [c for c in codec_order if c in seen]


def write_csv(data_slice, codec_order, collections_order, median_nrmse, output_path):
    codecs = used_codecs_for(data_slice, codec_order)

    with open(output_path, "w", newline="", encoding="utf-8") as f:
        writer = csv.writer(f)
        first = True

        for variant in VARIANTS:
            if not first:
                writer.writerow([])
                writer.writerow([])
                writer.writerow([])
            first = False

            writer.writerow([VARIANT_LABELS[variant]])

            is_median = variant in MEDIAN_VARIANTS
            kernel    = MEDIAN_KERNEL.get(variant)

            if is_median:
                writer.writerow(["Collection", "Mean NRMSE"] + codecs)
            else:
                writer.writerow(["Collection"] + codecs)

            for coll in collections_order:
                if coll not in data_slice:
                    continue

                if is_median:
                    nrmse_val = median_nrmse.get(coll, {}).get(kernel)
                    row = [coll, fmt_nrmse(nrmse_val)]
                else:
                    row = [coll]

                for codec in codecs:
                    values = data_slice[coll][variant].get(codec, [])
                    row.append(fmt_cr(mean(values)))
                writer.writerow(row)


def main():
    parser = argparse.ArgumentParser(
        description="Parse bench_comp logs into per-(ordering,nodata) CSVs.")
    parser.add_argument("log_dir",
                        help="Directory with per-collection .txt log files.")
    parser.add_argument("--index", default=None,
                        help="lossy_index.json (needed for median NRMSE column).")
    parser.add_argument("--output-dir", default=".",
                        help="Directory to write output CSVs (default: current dir).")
    parser.add_argument("--collections-order", nargs="*",
                        help="Explicit collection row ordering (default: alphabetical).")
    args = parser.parse_args()

    if not os.path.isdir(args.log_dir):
        print(f"Error: {args.log_dir} is not a directory", file=sys.stderr)
        sys.exit(1)

    index = {}
    if args.index:
        with open(args.index) as f:
            raw = json.load(f)
        index = raw.get("tifs", raw)
        print(f"Loaded index: {len(index)} TIFs")
    else:
        print("No --index provided; median NRMSE column will be empty.")

    os.makedirs(args.output_dir, exist_ok=True)

    print(f"Parsing logs from {args.log_dir} ...")
    data, codec_order, collection_tifs = parse_logs(args.log_dir)

    median_nrmse = build_median_nrmse(index, collection_tifs)

    all_collections   = sorted({c for key_data in data.values() for c in key_data})
    collections_order = args.collections_order or all_collections

    print(f"Collections: {len(all_collections)}")
    print(f"Codecs:      {len(codec_order)}")
    print(f"(ordering, nodata) keys found: {sorted(data.keys())}")

    for (ordering, nodata), data_slice in sorted(data.items()):
        fname = f"bench_comp_results_{ordering}_nodata{nodata}.csv"
        out   = os.path.join(args.output_dir, fname)
        write_csv(data_slice, codec_order, collections_order, median_nrmse, out)
        print(f"Written {out}")


if __name__ == "__main__":
    main()
