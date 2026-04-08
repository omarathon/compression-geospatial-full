#!/usr/bin/env python3

import os
import json
import argparse
import subprocess
import numpy as np
from osgeo import gdal

gdal.UseExceptions()


def run_cmd(cmd):
    subprocess.check_call(cmd, shell=True)


def get_creation_options(ds):
    md = ds.GetMetadata("IMAGE_STRUCTURE")
    co = []

    if "COMPRESSION" in md:
        co.append(f"COMPRESS={md['COMPRESSION']}")

    if md.get("TILED", "NO") == "YES":
        co.append("TILED=YES")
        blockx, blocky = ds.GetRasterBand(1).GetBlockSize()
        co.append(f"BLOCKXSIZE={blockx}")
        co.append(f"BLOCKYSIZE={blocky}")

    if "INTERLEAVE" in md:
        co.append(f"INTERLEAVE={md['INTERLEAVE']}")

    return co


def compute_stats_per_band(orig_ds, recon_ds):
    xsize = orig_ds.RasterXSize
    ysize = orig_ds.RasterYSize
    bands = orig_ds.RasterCount

    block = 512
    results = {}

    for b in range(1, bands + 1):
        sum_sq = 0.0
        sum_abs = 0.0
        sum_diff = 0.0
        sum_orig = 0.0
        sum_recon = 0.0
        sum_orig_sq = 0.0
        max_abs = 0.0
        count = 0

        band_orig = orig_ds.GetRasterBand(b)
        band_recon = recon_ds.GetRasterBand(b)

        nodata = band_orig.GetNoDataValue()

        for y in range(0, ysize, block):
            rows = min(block, ysize - y)

            o = band_orig.ReadAsArray(0, y, xsize, rows).astype(np.float64)
            r = band_recon.ReadAsArray(0, y, xsize, rows).astype(np.float64)

            if nodata is not None:
                mask = (o != nodata)
            else:
                mask = np.ones_like(o, dtype=bool)

            o = o[mask]
            r = r[mask]

            if o.size == 0:
                continue

            diff = o - r

            sum_sq += np.sum(diff ** 2)
            sum_abs += np.sum(np.abs(diff))
            sum_diff += np.sum(diff)

            sum_orig += np.sum(o)
            sum_recon += np.sum(r)
            sum_orig_sq += np.sum(o ** 2)

            max_abs = max(max_abs, np.max(np.abs(diff)))
            count += o.size

        if count == 0:
            results[str(b)] = {}
            continue

        rmse = np.sqrt(sum_sq / count)
        mae = sum_abs / count
        mean_orig = sum_orig / count
        mean_recon = sum_recon / count
        mean_diff = mean_recon - mean_orig

        var = (sum_orig_sq / count) - (mean_orig ** 2)
        std = np.sqrt(max(var, 1e-12))
        nrmse = rmse / std

        results[str(b)] = {
            "rmse": rmse,
            "mae": mae,
            "nrmse_std_original": nrmse,
            "mean_original": mean_orig,
            "mean_reconstructed": mean_recon,
            "mean_difference": mean_diff,
            "max_abs_error": max_abs,
            "pixel_count": int(count),
        }

    return results


def lerc_roundtrip(input_tif, output_tif, maxz, creation_opts, ds):
    tmp_lerc = output_tif + ".lerc_tmp.tif"

    # Step 1: LERC encode
    cmd1 = (
        f"gdal_translate \"{input_tif}\" \"{tmp_lerc}\" "
        f"-co COMPRESS=LERC -co MAX_Z_ERROR={maxz}"
    )
    run_cmd(cmd1)

    # Step 2: Decode back to original compression
    co_str = " ".join([f"-co {c}" for c in creation_opts])

    # Preserve per-band nodata
    nodata_opts = []
    for b in range(1, ds.RasterCount + 1):
        nd = ds.GetRasterBand(b).GetNoDataValue()
        if nd is not None:
            nodata_opts.append(f"-a_nodata {nd}")

    nodata_str = " ".join(nodata_opts)

    cmd2 = (
        f"gdal_translate \"{tmp_lerc}\" \"{output_tif}\" "
        f"{co_str} {nodata_str}"
    )
    run_cmd(cmd2)

    os.remove(tmp_lerc)


def main():
    parser = argparse.ArgumentParser(description="LERC roundtrip denoising with per-band stats")
    parser.add_argument("input", help="Input GeoTIFF")
    parser.add_argument("--z-errors", nargs="+", type=int, required=True)
    parser.add_argument("--output-dir", default=".")
    parser.add_argument("--json-name", default="stats.json")

    args = parser.parse_args()

    os.makedirs(args.output_dir, exist_ok=True)

    ds = gdal.Open(args.input)
    if ds is None:
        raise RuntimeError("Failed to open input file")

    creation_opts = get_creation_options(ds)

    stats_all = {
        "input_file": os.path.abspath(args.input),
        "results": []
    }

    base = os.path.splitext(os.path.basename(args.input))[0]

    for z in args.z_errors:
        print(f"\n=== Processing MAX_Z_ERROR={z} ===")

        out_path = os.path.join(
            args.output_dir,
            f"{base}_Denoised_MaxZ{z}.tif"
        )

        # LERC roundtrip
        lerc_roundtrip(args.input, out_path, z, creation_opts, ds)

        # Compute stats
        recon_ds = gdal.Open(out_path)
        stats = compute_stats_per_band(ds, recon_ds)

        stats_all["results"].append({
            "output_file": os.path.abspath(out_path),
            "z_error": z,
            "bands": stats
        })

    # Save JSON
    json_path = os.path.join(args.output_dir, args.json_name)
    with open(json_path, "w") as f:
        json.dump(stats_all, f, indent=2)

    print(f"\n✅ Saved stats to {json_path}")


if __name__ == "__main__":
    main()