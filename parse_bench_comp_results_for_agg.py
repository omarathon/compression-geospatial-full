#!/usr/bin/env python3
"""Like parse_bench_comp_results.py but aggregates over FoR window sizes.

For each group of codecs that differ only in the flat-FoR window (w=N) or
hierarchical-FoR windows (g=G, l=L), keeps only the entry with the minimum
mean CF per (collection, variant).  Window parameters are stripped from the
codec name in the output so all columns are consistent across experiments.

A companion *_best_windows_*.csv records which window was selected for each
FoR codec / collection / variant.

Codec names are also shortened for readability:
  FoR, FoR_sep, HFoR, HFoR_sep, PFor, Pack, direct.

The lossless matrix gains a "Mean NRMSE" column (always "0%") so every
variant section has the same two-column prefix [Collection, Mean NRMSE].

Usage:
    python3 parse_bench_comp_results_for_agg.py <log_dir>
            [--index lossy_index.json] [--output-dir .]
"""

import argparse
import csv
import json
import os
import re
import sys
from collections import defaultdict

# ── variant tables (unchanged from original) ────────────────────────────────

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

MEDIAN_KERNEL = {"median_k3": "3", "median_k5": "5", "median_k7": "7"}

LERC_TARGET = {
    "lerc_formula_t05pct": "0.05",
    "lerc_formula_t10pct": "0.1",
    "lerc_formula_t15pct": "0.15",
    "lerc_formula_t30pct": "0.3",
}

# ── regexes ──────────────────────────────────────────────────────────────────

RE_EXPERIMENT  = re.compile(r"^Experiment:\s+(.+)$")
RE_TIF_PATH    = re.compile(r"^TIF:\s+(.+)$")
RE_BENCH_START = re.compile(r"^>>> BENCH START:.*\|\s*variant=(\S+)\s*\|\s*nodata=(\d+)")
RE_BENCH_META  = re.compile(r"ordering=(\w+)")
RE_CODEC_LINE  = re.compile(r"^(\d+)=(.+)$")
RE_RESULT_LINE = re.compile(r"^c:(\d+),n:\d+,cfmean:([\d.eE+\-]+),")

# Window-stripping patterns
RE_FOR_WIN      = re.compile(r"(custom_for_unvec_u16)_w\d+")
RE_FOR_HIER_WIN = re.compile(r"(custom_for_hier_unvec_u16)_g\d+_l\d+")

# Window-extraction patterns (capture the values)
RE_FOR_WIN_VALS      = re.compile(r"custom_for_unvec_u16_w(\d+)")
RE_FOR_HIER_WIN_VALS = re.compile(r"custom_for_hier_unvec_u16_g(\d+)_l(\d+)")

# ── name transformations ─────────────────────────────────────────────────────

# Applied in order; hier/_sep must come before their prefixes.
_SHORT = [
    ("TurboPFor_TurboPFor128",        "PFor"),
    ("TurboPFor_TurboPack128",        "Pack"),
    ("[+]_",                          ""),
    ("custom_for_hier_unvec_u16_sep", "HFoR_sep"),
    ("custom_for_hier_unvec_u16",     "HFoR"),
    ("custom_for_unvec_u16_sep",      "FoR_sep"),
    ("custom_for_unvec_u16",          "FoR"),
    ("custom_direct_access",          "direct"),
]


def shorten(name):
    for old, new in _SHORT:
        name = name.replace(old, new)
    return name


def canonicalize(name):
    """Strip FoR window parameters from a codec name."""
    name = RE_FOR_WIN.sub(r"\1", name)
    name = RE_FOR_HIER_WIN.sub(r"\1", name)
    return name


def extract_window_str(name):
    """Return a compact string of the window params present in *name*."""
    m = RE_FOR_HIER_WIN_VALS.search(name)
    if m:
        return f"g{m.group(1)}_l{m.group(2)}"
    m = RE_FOR_WIN_VALS.search(name)
    if m:
        return f"w{m.group(1)}"
    return ""


# ── log parsing (identical to original) ─────────────────────────────────────

def parse_logs(log_dir):
    data = defaultdict(
        lambda: defaultdict(
            lambda: defaultdict(
                lambda: defaultdict(list))))

    codec_order     = []
    codec_order_set = set()
    collection_tifs = defaultdict(set)

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


# ── NRMSE helpers (identical to original) ────────────────────────────────────

def build_median_nrmse(index, collection_tifs):
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


def build_lerc_nrmse(index, collection_tifs):
    result = defaultdict(dict)
    for coll, tif_paths in collection_tifs.items():
        for variant, target_key in LERC_TARGET.items():
            values = []
            for tif in tif_paths:
                entry = index.get(tif, {})
                t = entry.get("1", {}).get("nrmse_targets", {}).get(target_key, {})
                nrmse = t.get("nrmse_at_formula_maxz")
                if nrmse is not None:
                    values.append(nrmse)
            if values:
                result[coll][variant] = sum(values) / len(values)
    return result


# ── FoR window aggregation ────────────────────────────────────────────────────

def aggregate_for_windows(data):
    """For each (key, coll, variant), collapse FoR codec groups to the best window.

    Returns:
        agg_data  – same structure as `data` but keyed by canonical codec name,
                    value is a single-element list [best_mean_cf].
        best_wins – best_wins[key][coll][variant][canonical] = window_str
    """
    agg_data  = defaultdict(lambda: defaultdict(lambda: defaultdict(lambda: defaultdict(list))))
    best_wins = defaultdict(lambda: defaultdict(lambda: defaultdict(dict)))

    for key, key_data in data.items():
        for coll, coll_data in key_data.items():
            for variant, var_data in coll_data.items():
                # Group original codec names by their canonical form.
                groups = defaultdict(dict)   # canonical -> {original: mean_cf}
                for codec_name, values in var_data.items():
                    if not values:
                        continue
                    m = sum(values) / len(values)
                    groups[canonicalize(codec_name)][codec_name] = m

                for canon, candidates in groups.items():
                    best_orig = min(candidates, key=candidates.__getitem__)
                    agg_data[key][coll][variant][canon] = [candidates[best_orig]]
                    best_wins[key][coll][variant][canon] = extract_window_str(best_orig)

    return agg_data, best_wins


def canonical_codec_order(codec_order):
    """Return deduplicated canonical names in original encounter order."""
    seen, result = set(), []
    for c in codec_order:
        canon = canonicalize(c)
        if canon not in seen:
            seen.add(canon)
            result.append(canon)
    return result


# ── formatting helpers ────────────────────────────────────────────────────────

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


# ── CSV writers ───────────────────────────────────────────────────────────────

def write_csv(data_slice, codec_order, collections_order,
              median_nrmse, lerc_nrmse, output_path):
    """Main results CSV.  Every section has [Collection, Mean NRMSE, ...codecs...].
    Lossless NRMSE column is always '0.0%'.  Codec names are shortened.
    """
    canon_codecs  = used_codecs_for(data_slice, codec_order)
    short_codecs  = [shorten(c) for c in canon_codecs]

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
            writer.writerow(["Collection", "Mean NRMSE"] + short_codecs)

            is_median    = variant in MEDIAN_VARIANTS
            is_lerc_lossy = variant in LERC_TARGET
            kernel        = MEDIAN_KERNEL.get(variant)

            for coll in collections_order:
                if coll not in data_slice:
                    continue

                if is_median:
                    nrmse_val = median_nrmse.get(coll, {}).get(kernel)
                    nrmse_str = fmt_nrmse(nrmse_val)
                elif is_lerc_lossy:
                    nrmse_val = lerc_nrmse.get(coll, {}).get(variant)
                    nrmse_str = fmt_nrmse(nrmse_val)
                else:
                    nrmse_str = "0.0%"   # lossless

                row = [coll, nrmse_str]
                for codec in canon_codecs:
                    values = data_slice[coll][variant].get(codec, [])
                    row.append(fmt_cr(mean(values)))
                writer.writerow(row)


def write_best_windows_csv(best_wins_slice, codec_order, collections_order, output_path):
    """Companion CSV showing which FoR window was chosen for each cell.

    Only columns that ever have a non-empty window string are included.
    Non-FoR codecs are omitted entirely (they have no window to show).
    Codec names are shortened.  Every section has the same two-column prefix
    [Collection, Mean NRMSE] (NRMSE left blank — this table shows windows).
    """
    # Determine which canonical codecs ever produced a non-empty window string.
    for_codecs = []
    for c in codec_order:
        has_win = any(
            best_wins_slice.get(coll, {}).get(variant, {}).get(c, "")
            for coll in collections_order
            for variant in VARIANTS
        )
        if has_win:
            for_codecs.append(c)

    if not for_codecs:
        return   # nothing to write

    short_codecs = [shorten(c) for c in for_codecs]

    with open(output_path, "w", newline="", encoding="utf-8") as f:
        writer = csv.writer(f)
        first = True

        for variant in VARIANTS:
            if not first:
                writer.writerow([])
                writer.writerow([])
                writer.writerow([])
            first = False

            writer.writerow([f"Best window — {VARIANT_LABELS[variant]}"])
            writer.writerow(["Collection", ""] + short_codecs)

            for coll in collections_order:
                row = [coll, ""]
                for codec in for_codecs:
                    win = best_wins_slice.get(coll, {}).get(variant, {}).get(codec, "")
                    row.append(win)
                writer.writerow(row)


# ── main ──────────────────────────────────────────────────────────────────────

def main():
    parser = argparse.ArgumentParser(
        description="Parse bench_comp logs, aggregate FoR windows, write CSVs.")
    parser.add_argument("log_dir")
    parser.add_argument("--index",            default=None)
    parser.add_argument("--output-dir",       default=".")
    parser.add_argument("--collections-order", nargs="*")
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
        print("No --index provided; Mean NRMSE columns will be empty for median/LERC.")

    os.makedirs(args.output_dir, exist_ok=True)

    print(f"Parsing logs from {args.log_dir} ...")
    data, codec_order, collection_tifs = parse_logs(args.log_dir)

    median_nrmse = build_median_nrmse(index, collection_tifs)
    lerc_nrmse   = build_lerc_nrmse(index, collection_tifs)

    all_collections   = sorted({c for kd in data.values() for c in kd})
    collections_order = args.collections_order or all_collections

    print(f"Collections: {len(all_collections)}")
    print(f"Raw codecs:  {len(codec_order)}")
    print(f"(ordering, nodata) keys: {sorted(data.keys())}")

    agg_data, best_wins = aggregate_for_windows(data)
    canon_order = canonical_codec_order(codec_order)
    print(f"Canonical codecs after FoR aggregation: {len(canon_order)}")

    for (ordering, nodata), data_slice in sorted(agg_data.items()):
        wins_slice = best_wins[(ordering, nodata)]

        stem  = f"bench_comp_foragg_{ordering}_nodata{nodata}"
        main_out = os.path.join(args.output_dir, stem + ".csv")
        wins_out = os.path.join(args.output_dir, stem + "_best_windows.csv")

        write_csv(data_slice, canon_order, collections_order,
                  median_nrmse, lerc_nrmse, main_out)
        print(f"Written {main_out}")

        write_best_windows_csv(wins_slice, canon_order, collections_order, wins_out)
        print(f"Written {wins_out}")


if __name__ == "__main__":
    main()
