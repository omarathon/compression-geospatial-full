#!/usr/bin/env python3
"""Plot bench_sweep.json results.

Usage:
    python3 scripts/plot_sweep.py [bench_sweep.json] [--out DIR]

Produces:
    <out>/speedup_<collection>_<width>bit.pdf  – speedup vs target RSS per collection
    <out>/speed_all.pdf                         – aggregate mean_sum_ns per codec
"""

import argparse
import json
import math
import os
import sys
from pathlib import Path

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
import matplotlib.ticker as ticker
import numpy as np
import pandas as pd

# ── Codec display names and colours ──────────────────────────────────────────

CODEC_SHORT = {
    "custom_direct_access":                                          "baseline (no codec)",
    "simdcomp_fused":                                                "simdcomp",
    "FastPFor_fused_corrected_SIMDPFor+VariableByte":                "PFor",
    "simdcomp_fused_delta_local":                                    "simdcomp+Δlocal",
    "simdcomp_fused_delta_carry":                                    "simdcomp+Δcarry",
    "FastPFor_fused_corrected_delta_local_SIMDPFor+VariableByte":   "PFor+Δlocal",
    "FastPFor_fused_corrected_delta_carry_SIMDPFor+VariableByte":   "PFor+Δcarry",
}

CODEC_ORDER = list(CODEC_SHORT.keys())

PALETTE = [
    "#1f77b4", "#ff7f0e", "#2ca02c",
    "#d62728", "#9467bd", "#8c564b", "#e377c2",
]

BASELINE = "custom_direct_access"

# ── Load ─────────────────────────────────────────────────────────────────────

def load(path: str) -> pd.DataFrame:
    with open(path) as f:
        raw = json.load(f)
    df = pd.DataFrame(raw)

    # Expand per_tif list → one row per (record, tif).
    # We keep the per_tif data alongside the top-level aggregate columns.
    tif_df = (
        df.explode("per_tif")
          .reset_index(drop=True)
    )
    tif_expanded = pd.json_normalize(tif_df["per_tif"])
    tif_df = pd.concat(
        [tif_df.drop(columns=["per_tif"]).reset_index(drop=True), tif_expanded],
        axis=1,
    )

    # tif_df has per-tif columns: tif, dec_ns, trans_ns, sum_ns, rss_kb
    # df     has aggregate: mean_sum_ns, std_sum_ns, mean_dec_ns, …

    return df, tif_df


# ── Helpers ──────────────────────────────────────────────────────────────────

def codec_label(codec: str) -> str:
    return CODEC_SHORT.get(codec, codec)

def codec_color(codec: str) -> str:
    idx = CODEC_ORDER.index(codec) if codec in CODEC_ORDER else -1
    return PALETTE[idx % len(PALETTE)]


# ── Plot 1: speedup vs target RSS per collection ─────────────────────────────

def plot_speedup_per_collection(df: pd.DataFrame, out_dir: Path) -> None:
    """One PDF per (collection × width) with speedup relative to baseline."""

    for (collection, width), grp in df.groupby(["collection", "width"]):
        codecs = [c for c in CODEC_ORDER if c in grp["codec"].unique()]
        if BASELINE not in grp["codec"].values:
            continue  # no reference to normalise against

        # Pivot: index=target_rss_mb × band, columns=codec, values=mean_sum_ns
        pivot = (
            grp.groupby(["target_rss_mb", "band", "codec"])["mean_sum_ns"]
               .mean()
               .reset_index()
               .pivot_table(index=["target_rss_mb", "band"],
                            columns="codec",
                            values="mean_sum_ns")
        )
        if BASELINE not in pivot.columns:
            continue

        # Average over bands
        pivot_avg = pivot.groupby("target_rss_mb").mean()
        baseline_col = pivot_avg[BASELINE]

        rss_vals = pivot_avg.index.values
        x = np.arange(len(rss_vals))
        x_labels = [f"{v}" for v in rss_vals]

        fig, ax = plt.subplots(figsize=(9, 5))
        for codec in codecs:
            if codec == BASELINE or codec not in pivot_avg.columns:
                continue
            speedup = baseline_col / pivot_avg[codec]
            ax.plot(x, speedup.values, marker="o", label=codec_label(codec),
                    color=codec_color(codec), linewidth=1.8, markersize=4)

        ax.axhline(1.0, color="grey", linestyle="--", linewidth=1, label="baseline")
        ax.set_xticks(x)
        ax.set_xticklabels(x_labels, rotation=45, ha="right", fontsize=8)
        ax.set_xlabel("Target RSS (MB)")
        ax.set_ylabel("Speedup over baseline")
        ax.set_title(f"{collection}  ({width}-bit)")
        ax.legend(fontsize=7, loc="best")
        ax.grid(True, alpha=0.3)

        fname = out_dir / f"speedup_{collection}_{width}bit.pdf"
        fig.tight_layout()
        fig.savefig(fname)
        plt.close(fig)
        print(f"  saved {fname}")


# ── Plot 2: absolute sum_ns vs RSS, one line per codec, faceted by collection ─

def plot_abs_speed(df: pd.DataFrame, out_dir: Path) -> None:
    collections = df["collection"].unique()
    n_cols = 3
    n_rows = math.ceil(len(collections) / n_cols)

    codecs = [c for c in CODEC_ORDER if c in df["codec"].unique()]

    fig, axes = plt.subplots(n_rows, n_cols,
                              figsize=(5 * n_cols, 3.5 * n_rows),
                              squeeze=False)
    axes_flat = axes.flatten()

    for ax_idx, collection in enumerate(sorted(collections)):
        ax = axes_flat[ax_idx]
        grp = df[df["collection"] == collection]
        for codec in codecs:
            sub = (
                grp[grp["codec"] == codec]
                .groupby("target_rss_mb")["mean_sum_ns"]
                .mean()
            )
            if sub.empty:
                continue
            ax.plot(sub.index, sub.values,
                    marker="o", markersize=3, linewidth=1.5,
                    label=codec_label(codec), color=codec_color(codec))
        ax.set_title(collection, fontsize=7)
        ax.set_xlabel("RSS (MB)", fontsize=7)
        ax.set_ylabel("mean sum_ns", fontsize=7)
        ax.set_xscale("log", base=2)
        ax.xaxis.set_major_formatter(ticker.ScalarFormatter())
        ax.tick_params(labelsize=6)
        ax.grid(True, alpha=0.3)

    # Hide unused axes
    for ax in axes_flat[len(collections):]:
        ax.set_visible(False)

    # Shared legend below figure
    handles, labels = axes_flat[0].get_legend_handles_labels()
    fig.legend(handles, labels, loc="lower center",
               ncol=len(codecs), fontsize=7,
               bbox_to_anchor=(0.5, -0.02))

    fname = out_dir / "speed_all.pdf"
    fig.tight_layout(rect=[0, 0.04, 1, 1])
    fig.savefig(fname, bbox_inches="tight")
    plt.close(fig)
    print(f"  saved {fname}")


# ── Plot 3: actual RSS (from per_tif) vs target RSS ──────────────────────────

def plot_actual_rss(tif_df: pd.DataFrame, out_dir: Path) -> None:
    """Check that actual measured RSS tracks target RSS (sanity plot)."""
    # Average rss_kb per (codec, target_rss_mb)
    rss_grp = (
        tif_df.dropna(subset=["rss_kb"])
              .groupby(["codec", "target_rss_mb"])["rss_kb"]
              .mean()
              .reset_index()
    )

    codecs = [c for c in CODEC_ORDER if c in rss_grp["codec"].unique()]

    fig, ax = plt.subplots(figsize=(7, 5))
    for codec in codecs:
        sub = rss_grp[rss_grp["codec"] == codec]
        ax.plot(sub["target_rss_mb"], sub["rss_kb"] / 1024,
                marker="o", markersize=4, linewidth=1.5,
                label=codec_label(codec), color=codec_color(codec))

    # Reference line: actual == target
    lim = rss_grp["target_rss_mb"].max() * 1.1
    ax.plot([0, lim], [0, lim], "k--", linewidth=0.8, label="actual=target")

    ax.set_xlabel("Target RSS (MB)")
    ax.set_ylabel("Actual RSS (MB, mean over TIFs)")
    ax.set_title("Actual vs target RSS per codec")
    ax.legend(fontsize=7)
    ax.grid(True, alpha=0.3)

    fname = out_dir / "actual_rss.pdf"
    fig.tight_layout()
    fig.savefig(fname)
    plt.close(fig)
    print(f"  saved {fname}")


# ── Main ─────────────────────────────────────────────────────────────────────

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("json", nargs="?", default="bench_sweep.json",
                        help="Path to bench_sweep.json (default: bench_sweep.json)")
    parser.add_argument("--out", default="plots",
                        help="Output directory for PDFs (default: plots/)")
    args = parser.parse_args()

    if not os.path.exists(args.json):
        sys.exit(f"ERROR: {args.json} not found")

    out_dir = Path(args.out)
    out_dir.mkdir(parents=True, exist_ok=True)

    print(f"Loading {args.json} …")
    df, tif_df = load(args.json)
    print(f"  {len(df)} aggregate rows, {len(tif_df)} per-tif rows")
    print(f"  collections: {sorted(df['collection'].unique())}")
    print(f"  codecs:      {sorted(df['codec'].unique())}")

    print("Plotting speedup per collection …")
    plot_speedup_per_collection(df, out_dir)

    print("Plotting absolute speed faceted by collection …")
    plot_abs_speed(df, out_dir)

    print("Plotting actual vs target RSS …")
    plot_actual_rss(tif_df, out_dir)

    print("Done.")


if __name__ == "__main__":
    main()
