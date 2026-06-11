#!/usr/bin/env python3
"""Plot speedup vs uncompressed from benchmark sweep JSON results.

Usage:
    python3 scripts/plot_speedup.py bench_sweep_16bit.json [-o plot.pdf]
"""
import argparse
import json
from collections import defaultdict

import matplotlib.pyplot as plt
import matplotlib.ticker as ticker
import numpy as np


CODEC_LABELS = {
    "FastPFor_fused_SIMDPFor+VariableByte": "SIMDPFor+VariableByte",
    "simdcomp_fused": "simdcomp",
}
CODEC_COLORS = {
    "FastPFor_fused_SIMDPFor+VariableByte": "#1f77b4",  # blue
    "simdcomp_fused": "#ff7f0e",                         # orange
}
CODEC_ORDER = ["FastPFor_fused_SIMDPFor+VariableByte", "simdcomp_fused"]

TIF_LABELS = {
    "srtm_45_15.tif": "srtm_45_15",
    "JRC_TMF_AnnualChange_v1_1990_AFR_ID16_S10_E10.tif": "JRC_TMF_AnnualChange_v1_1990_AFR_ID16_S10_E10",
    "slope-srtm_35_11.tif": "slope-srtm_35_11",
    "accessibility.tif": "accessibility",
}


def format_rss(mb):
    if mb >= 1024:
        return f"{mb/1024:.1f} GB"
    return f"{mb:.0f} MB"


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("json_file", help="JSON results from run_benchmarks_sweep")
    parser.add_argument("-o", "--output", default="speedup_plot.pdf", help="Output file")
    args = parser.parse_args()

    with open(args.json_file) as f:
        results = json.load(f)

    # Group: tif -> target_rss_mb -> codec -> record
    by_tif = defaultdict(lambda: defaultdict(dict))
    for r in results:
        key = r.get("target_rss_mb", r["numblocks"])
        by_tif[r["tif"]][key][r["codec"]] = r

    tif_order = list(dict.fromkeys(r["tif"] for r in results))
    n_tifs = len(tif_order)

    fig, axes = plt.subplots(1, n_tifs, figsize=(3.5 * n_tifs, 3.8), sharey=True)
    if n_tifs == 1:
        axes = [axes]

    legend_handles = []
    legend_labels = []

    for ax_idx, tif in enumerate(tif_order):
        ax = axes[ax_idx]
        rss_data = by_tif[tif]
        sorted_rss = sorted(rss_data.keys())

        # Get baseline (custom_direct_access) sum_ns for each RSS threshold
        baselines = {}
        for rss_key in sorted_rss:
            if "custom_direct_access" in rss_data[rss_key]:
                baselines[rss_key] = rss_data[rss_key]["custom_direct_access"]["sum_ns"]

        sorted_rss = [r for r in sorted_rss if r in baselines]
        if not sorted_rss:
            continue

        # X-axis labels from theoretical RSS thresholds
        rss_labels = [format_rss(r) for r in sorted_rss]

        x = np.arange(len(sorted_rss))
        bar_width = 0.35
        offsets = [-(bar_width / 2), bar_width / 2]

        for c_idx, codec in enumerate(CODEC_ORDER):
            speedups = []
            for rss_key in sorted_rss:
                if codec in rss_data[rss_key] and baselines[rss_key]:
                    s = baselines[rss_key] / rss_data[rss_key][codec]["sum_ns"]
                    speedups.append(s)
                else:
                    speedups.append(0)

            bars = ax.bar(x + offsets[c_idx], speedups, bar_width,
                          color=CODEC_COLORS[codec],
                          label=CODEC_LABELS[codec] if ax_idx == 0 else None)

            if ax_idx == 0:
                legend_handles.append(bars)
                legend_labels.append(CODEC_LABELS[codec])

        ax.set_title(TIF_LABELS.get(tif, tif), fontsize=11, fontweight="bold")
        ax.set_xticks(x)
        ax.set_xticklabels(rss_labels, rotation=45, ha="right", fontsize=8)
        ax.set_xlabel("Resident Set Size\n(RSS)", fontsize=9)

        ax.axhline(y=1, color="black", linestyle="--", linewidth=0.8)
        ax.text(x[0] - 0.4, 1.08, "faster", fontsize=7, ha="left", va="bottom")
        ax.text(x[0] - 0.4, 0.92, "slower", fontsize=7, ha="left", va="top")

        ax.set_ylim(0, None)
        ax.yaxis.set_major_locator(ticker.MultipleLocator(1.0))
        ax.yaxis.set_minor_locator(ticker.MultipleLocator(0.5))

        if ax_idx == 0:
            ax.set_ylabel("Speedup vs. Uncompressed\n(ratio t\u2080/t\u2081)", fontsize=10)

    fig.legend(legend_handles, legend_labels,
               loc="upper center", ncol=2, fontsize=10,
               bbox_to_anchor=(0.5, 1.02), frameon=True)

    fig.tight_layout(rect=[0, 0, 1, 0.93])
    fig.savefig(args.output, bbox_inches="tight", dpi=200)
    print(f"Saved to {args.output}")


if __name__ == "__main__":
    main()
