#!/usr/bin/env python3
"""Parse bench_comp output and generate interactive Plotly plots per dataset."""

import re
import plotly.graph_objects as go
from plotly.subplots import make_subplots

def parse_results(path):
    """Parse comp_results.txt into a list of datasets."""
    with open(path) as f:
        text = f.read()

    datasets = []
    # Split on "=== ... ==="
    sections = re.split(r"^=== (.+?) ===$", text, flags=re.MULTILINE)
    # sections[0] is empty, then alternating: name, content
    for i in range(1, len(sections), 2):
        name = sections[i]
        body = sections[i + 1]

        # Parse codec names
        codec_map = {}
        for m in re.finditer(r"^(\d+)=(.+)$", body, re.MULTILINE):
            codec_map[int(m.group(1))] = m.group(2)

        # Parse data lines
        rows = []
        for m in re.finditer(
            r"^c:(\d+),cfmean:([\d.eE+-]+),cfvar:([\d.eE+-]+),.*?"
            r"tdecmean:([\d.eE+-]+),tdecvar:([\d.eE+-]+)",
            body, re.MULTILINE
        ):
            idx = int(m.group(1))
            cfmean = float(m.group(2))
            cfvar = float(m.group(3))
            tdec = float(m.group(4))
            tdecvar = float(m.group(5))
            cfstd = cfvar ** 0.5
            tdecstd = tdecvar ** 0.5
            # Propagate cf std to ratio via delta method: ratio=1/cf,
            # d(ratio)/d(cf) = -1/cf^2, so ratio_std ≈ cfstd / cf^2
            ratio = 1.0 / cfmean if cfmean > 0 else 0
            ratio_std = cfstd / (cfmean ** 2) if cfmean > 0 else 0
            rows.append({
                "idx": idx,
                "name": codec_map.get(idx, f"codec_{idx}"),
                "cfmean": cfmean,
                "ratio": ratio,
                "ratio_std": ratio_std,
                "tdec_us": tdec / 1000.0,
                "tdec_std_us": tdecstd / 1000.0,
            })

        datasets.append({"file": name, "rows": rows})
    return datasets


def short_name(full):
    """Shorten codec names for labels."""
    s = full.replace("custom_", "").replace("_vecsse", "")
    s = s.replace("FastPFor_fused_SIMDPFor+VariableByte", "FastPFor")
    s = s.replace("simdcomp_fused", "simdcomp")
    s = s.replace("custom_direct_access", "direct")
    s = s.replace("direct_access", "direct")
    s = s.replace("[+]_", "")
    return s


def _logical_group(name):
    """Identify which logical codec family a name belongs to."""
    # Order matters: check more specific names first
    if "doubledelta" in name: return "DoubleDelta"
    if "xordelta" in name: return "XorDelta"
    if "byteshuffle" in name: return "ByteShuffle"
    if "pred_jpegls" in name: return "JPEG-LS"
    if "pred_lorenzo" in name: return "Lorenzo"
    if "pred_avg" in name: return "PredAvg"
    if "pred_up" in name: return "PredUp"
    if "delta" in name: return "Delta"
    if "for" in name: return "FOR"
    if "rle" in name: return "RLE"
    return None

# Colors for logical families
_LOGICAL_COLORS = {
    "Delta":       "#2ca02c",
    "FOR":         "#d62728",
    "RLE":         "#9467bd",
    "DoubleDelta": "#17becf",
    "XorDelta":    "#bcbd22",
    "ByteShuffle": "#e377c2",
    "PredUp":      "#7f7f7f",
    "PredAvg":     "#8c564b",
    "Lorenzo":     "#1a9850",
    "JPEG-LS":     "#d73027",
}

_LOGICAL_SYMBOLS = {
    "Delta":       "triangle-up",
    "FOR":         "diamond",
    "RLE":         "triangle-down",
    "DoubleDelta": "star",
    "XorDelta":    "hexagon",
    "ByteShuffle": "pentagon",
    "PredUp":      "cross",
    "PredAvg":     "x",
    "Lorenzo":     "star-diamond",
    "JPEG-LS":     "star-triangle-up",
}

def classify(name):
    """Return (group, color, symbol) for a codec."""
    if name.startswith("[+]_"):
        inner = name[4:]
        lg = _logical_group(inner)
        if lg:
            return (f"{lg} + physical",
                    _LOGICAL_COLORS.get(lg, "gray"),
                    _LOGICAL_SYMBOLS.get(lg, "circle"))
    # Standalone physical
    if "direct" in name or "simdcomp" in name or "FastPFor" in name:
        return ("Standalone physical", "#ff7f0e", "square")
    # Standalone logical
    lg = _logical_group(name)
    if lg:
        return (f"Standalone logical", "#1f77b4", "circle")
    return ("Other", "gray", "circle")


def make_plot(ds):
    """Create a single Plotly figure for one dataset."""
    fig = go.Figure()

    # Group rows
    groups = {}
    for r in ds["rows"]:
        grp, color, symbol = classify(r["name"])
        groups.setdefault(grp, {"color": color, "symbol": symbol, "rows": []})
        groups[grp]["rows"].append(r)

    for grp, info in groups.items():
        xs = [r["ratio"] for r in info["rows"]]
        ys = [r["tdec_us"] for r in info["rows"]]
        x_err = [r["ratio_std"] for r in info["rows"]]
        y_err = [r["tdec_std_us"] for r in info["rows"]]
        labels = [short_name(r["name"]) for r in info["rows"]]
        fulls = [r["name"] for r in info["rows"]]
        cfs = [r["cfmean"] for r in info["rows"]]

        fig.add_trace(go.Scatter(
            x=xs, y=ys, mode="markers+text",
            name=grp,
            text=labels,
            textposition="top right",
            textfont=dict(size=9, color=info["color"]),
            marker=dict(size=10, color=info["color"], symbol=info["symbol"],
                        line=dict(width=1, color="black")),
            error_x=dict(type="data", array=x_err, visible=True,
                         color=info["color"], thickness=2, width=6),
            error_y=dict(type="data", array=y_err, visible=True,
                         color=info["color"], thickness=2, width=6),
            customdata=list(zip(fulls, cfs, x_err, y_err)),
            hovertemplate=(
                "<b>%{customdata[0]}</b><br>"
                "Compression ratio: %{x:.2f}x ± %{customdata[2]:.2f}<br>"
                "Compressed/original: %{customdata[1]:.3f}<br>"
                "Decompression: %{y:.1f} ± %{customdata[3]:.1f} µs<extra></extra>"
            ),
        ))

    fig.add_vline(x=1.0, line_dash="dash", line_color="gray", opacity=0.4)

    title = ds["file"].replace(".tif", "").replace(".TIF", "")
    fig.update_layout(
        title=f"{title}<br><sup>Decompression time vs Compression ratio (lower-right is better)</sup>",
        xaxis_title="Compression ratio (original / compressed)",
        yaxis_title="Decompression time (µs)",
        yaxis_type="log",
        template="plotly_white",
        width=950, height=620,
        legend=dict(x=0.01, y=0.99, bgcolor="rgba(255,255,255,0.8)"),
    )
    return fig


def main():
    import sys, os
    input_file = sys.argv[1] if len(sys.argv) > 1 else "comp_results.txt"
    output_dir = sys.argv[2] if len(sys.argv) > 2 else "plots"

    base = "/home/omar/compression-geospatial-git"
    input_path = input_file if os.path.isabs(input_file) else os.path.join(base, input_file)
    output_path = output_dir if os.path.isabs(output_dir) else os.path.join(base, output_dir)
    os.makedirs(output_path, exist_ok=True)

    datasets = parse_results(input_path)

    for ds in datasets:
        fig = make_plot(ds)
        safe = re.sub(r"[^a-zA-Z0-9_]", "_", ds["file"].replace(".tif", "").replace(".TIF", ""))
        outpath = os.path.join(output_path, f"bench_{safe}.html")
        fig.write_html(outpath)
        print(f"  {outpath}")

    # Also make a combined overview with subplots
    n = len(datasets)
    fig = make_subplots(rows=3, cols=3,
                        subplot_titles=[d["file"] for d in datasets] + [""] * (9 - n),
                        horizontal_spacing=0.06, vertical_spacing=0.1)

    legend_added = set()
    for idx, ds in enumerate(datasets):
        row = idx // 3 + 1
        col = idx % 3 + 1
        groups = {}
        for r in ds["rows"]:
            grp, color, symbol = classify(r["name"])
            groups.setdefault(grp, {"color": color, "symbol": symbol, "rows": []})
            groups[grp]["rows"].append(r)

        for grp, info in groups.items():
            xs = [r["ratio"] for r in info["rows"]]
            ys = [r["tdec_us"] for r in info["rows"]]
            x_err = [r["ratio_std"] for r in info["rows"]]
            y_err = [r["tdec_std_us"] for r in info["rows"]]
            fulls = [r["name"] for r in info["rows"]]
            cfs = [r["cfmean"] for r in info["rows"]]

            show = grp not in legend_added
            legend_added.add(grp)

            fig.add_trace(go.Scatter(
                x=xs, y=ys, mode="markers",
                name=grp, legendgroup=grp,
                showlegend=show,
                marker=dict(size=7, color=info["color"], symbol=info["symbol"],
                            line=dict(width=0.5, color="black")),
                error_x=dict(type="data", array=x_err, visible=True,
                             color=info["color"], thickness=2, width=4),
                error_y=dict(type="data", array=y_err, visible=True,
                             color=info["color"], thickness=2, width=4),
                customdata=list(zip(fulls, cfs, x_err, y_err)),
                hovertemplate=(
                    "<b>%{customdata[0]}</b><br>"
                    "Ratio: %{x:.2f}x ± %{customdata[2]:.2f} | "
                    "Dec: %{y:.1f} ± %{customdata[3]:.1f} µs<extra></extra>"
                ),
            ), row=row, col=col)

        fig.update_yaxes(type="log", row=row, col=col)

    fig.update_layout(
        title="All datasets — Decompression time vs Compression ratio",
        template="plotly_white",
        width=1400, height=1100,
        legend=dict(orientation="h", x=0.5, xanchor="center", y=1.02,
                    yanchor="bottom", bgcolor="rgba(255,255,255,0.8)"),
    )
    combined_path = os.path.join(output_path, "bench_all.html")
    fig.write_html(combined_path)
    print(f"  {combined_path}")


if __name__ == "__main__":
    main()
