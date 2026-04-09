#!/usr/bin/env python3

import os
import json
import math
import argparse
import numpy as np
from osgeo import gdal

gdal.UseExceptions()
gdal.SetCacheMax(1024 * 1024 * 1024)  # 1 GB


MAX_STATS_BLOCKS = 512
STATS_BLOCK_SIZE = 256


def detect_layout_creation_options(ds):
    md = ds.GetMetadata("IMAGE_STRUCTURE") or {}
    band1 = ds.GetRasterBand(1)
    xsize = ds.RasterXSize
    blockx, blocky = band1.GetBlockSize()

    creation = []

    compression = md.get("COMPRESSION")
    if compression:
        creation.append(f"COMPRESS={compression}")

    interleave = md.get("INTERLEAVE")
    if interleave:
        creation.append(f"INTERLEAVE={interleave}")

    predictor = md.get("PREDICTOR")
    if predictor:
        creation.append(f"PREDICTOR={predictor}")

    nbits = md.get("NBITS")
    if nbits:
        creation.append(f"NBITS={nbits}")

    is_striped = (blockx == xsize)

    if is_striped:
        creation.append("TILED=NO")
    else:
        creation.append("TILED=YES")
        creation.append(f"BLOCKXSIZE={blockx}")
        creation.append(f"BLOCKYSIZE={blocky}")

    return creation


def strip_statistics_metadata(obj):
    md = obj.GetMetadata() or {}
    for key in list(md.keys()):
        if key.startswith("STATISTICS_"):
            obj.SetMetadataItem(key, None)


def copy_metadata_and_band_properties(src_ds, dst_ds):
    src_domains = src_ds.GetMetadataDomainList() or []
    for domain in src_domains:
        if domain == "IMAGE_STRUCTURE":
            continue
        md = src_ds.GetMetadata(domain)
        if md:
            clean_md = {k: v for k, v in md.items() if not k.startswith("STATISTICS_")}
            if clean_md:
                dst_ds.SetMetadata(clean_md, domain)

    dst_ds.SetGeoTransform(src_ds.GetGeoTransform())
    dst_ds.SetProjection(src_ds.GetProjection())

    for b in range(1, src_ds.RasterCount + 1):
        sb = src_ds.GetRasterBand(b)
        db = dst_ds.GetRasterBand(b)

        nd = sb.GetNoDataValue()
        if nd is not None:
            db.SetNoDataValue(nd)

        db.SetColorInterpretation(sb.GetColorInterpretation())

        desc = sb.GetDescription()
        if desc:
            db.SetDescription(desc)

        unit = sb.GetUnitType()
        if unit:
            db.SetUnitType(unit)

        scale = sb.GetScale()
        if scale is not None:
            db.SetScale(scale)

        offset = sb.GetOffset()
        if offset is not None:
            db.SetOffset(offset)

        categories = sb.GetCategoryNames()
        if categories:
            db.SetCategoryNames(categories)

        color_table = sb.GetColorTable()
        if color_table is not None:
            db.SetColorTable(color_table)

        band_domains = sb.GetMetadataDomainList() or []
        for domain in band_domains:
            if domain == "IMAGE_STRUCTURE":
                continue
            md = sb.GetMetadata(domain)
            if md:
                clean_md = {k: v for k, v in md.items() if not k.startswith("STATISTICS_")}
                if clean_md:
                    db.SetMetadata(clean_md, domain)

        strip_statistics_metadata(db)

    strip_statistics_metadata(dst_ds)
    dst_ds.FlushCache()


def lerc_roundtrip(input_tif, output_tif, maxz, source_creation_opts, src_ds):
    temp_lerc = output_tif + ".lerc_tmp.tif"

    temp_creation = []
    for opt in source_creation_opts:
        if not opt.startswith("COMPRESS="):
            temp_creation.append(opt)
    temp_creation.append("COMPRESS=LERC")
    temp_creation.append(f"MAX_Z_ERROR={maxz}")

    gdal.Translate(
        temp_lerc,
        input_tif,
        format="GTiff",
        creationOptions=temp_creation,
    )

    gdal.Translate(
        output_tif,
        temp_lerc,
        format="GTiff",
        creationOptions=source_creation_opts,
    )

    out_ds = gdal.Open(output_tif, gdal.GA_Update)
    if out_ds is None:
        raise RuntimeError(f"Failed to open output for metadata copy: {output_tif}")
    copy_metadata_and_band_properties(src_ds, out_ds)
    out_ds = None

    os.remove(temp_lerc)


def generate_evenly_spaced_block_windows(xsize, ysize, block_size=256, max_blocks=512):
    nx = math.ceil(xsize / block_size)
    ny = math.ceil(ysize / block_size)
    total_blocks = nx * ny

    all_indices = np.arange(total_blocks, dtype=np.int64)
    if total_blocks <= max_blocks:
        chosen = all_indices
    else:
        chosen = np.linspace(0, total_blocks - 1, num=max_blocks, dtype=np.int64)

    windows = []
    for idx in chosen:
        by = idx // nx
        bx = idx % nx

        x = int(bx * block_size)
        y = int(by * block_size)
        cols = min(block_size, xsize - x)
        rows = min(block_size, ysize - y)

        windows.append((x, y, cols, rows))

    return windows, total_blocks


def compute_stats_per_band(orig_ds, recon_ds):
    if (
        orig_ds.RasterXSize != recon_ds.RasterXSize
        or orig_ds.RasterYSize != recon_ds.RasterYSize
        or orig_ds.RasterCount != recon_ds.RasterCount
    ):
        raise RuntimeError("Original and reconstructed rasters are not shape-compatible")

    xsize = orig_ds.RasterXSize
    ysize = orig_ds.RasterYSize
    windows, total_possible_blocks = generate_evenly_spaced_block_windows(
        xsize, ysize, block_size=STATS_BLOCK_SIZE, max_blocks=MAX_STATS_BLOCKS
    )

    results = {}

    for b in range(1, orig_ds.RasterCount + 1):
        band_orig = orig_ds.GetRasterBand(b)
        band_recon = recon_ds.GetRasterBand(b)
        nodata = band_orig.GetNoDataValue()

        sum_sq = 0.0
        sum_abs = 0.0
        sum_orig = 0.0
        sum_recon = 0.0
        sum_orig_sq = 0.0
        max_abs = 0.0
        count = 0

        for x, y, cols, rows in windows:
            o = band_orig.ReadAsArray(x, y, cols, rows)
            r = band_recon.ReadAsArray(x, y, cols, rows)

            if o is None or r is None:
                raise RuntimeError(f"ReadAsArray failed for band {b}, window x={x}, y={y}")

            if np.issubdtype(o.dtype, np.integer) and np.issubdtype(r.dtype, np.integer):
                diff = r.astype(np.int64, copy=False) - o.astype(np.int64, copy=False)
                o_acc = o.astype(np.float64, copy=False)
                r_acc = r.astype(np.float64, copy=False)
            else:
                o_acc = o.astype(np.float64, copy=False)
                r_acc = r.astype(np.float64, copy=False)
                diff = r_acc - o_acc

            if nodata is None:
                valid = np.ones(o.shape, dtype=bool)
            else:
                valid = (o != nodata)

            valid_count = int(valid.sum())
            if valid_count == 0:
                continue

            absdiff = np.abs(diff)

            sum_sq += float(np.sum(diff * diff, where=valid, dtype=np.float64))
            sum_abs += float(np.sum(absdiff, where=valid, dtype=np.float64))
            sum_orig += float(np.sum(o_acc, where=valid, dtype=np.float64))
            sum_recon += float(np.sum(r_acc, where=valid, dtype=np.float64))
            sum_orig_sq += float(np.sum(o_acc * o_acc, where=valid, dtype=np.float64))

            chunk_max = float(np.max(absdiff[valid]))
            if chunk_max > max_abs:
                max_abs = chunk_max

            count += valid_count

        if count == 0:
            results[str(b)] = {}
            continue

        mean_orig = sum_orig / count
        mean_recon = sum_recon / count
        mean_diff = mean_recon - mean_orig

        rmse = math.sqrt(sum_sq / count)
        mae = sum_abs / count

        var = (sum_orig_sq / count) - (mean_orig * mean_orig)
        if var < 0.0:
            var = 0.0
        std = math.sqrt(var)
        nrmse = None if std == 0.0 else (rmse / std)

        results[str(b)] = {
            "rmse": rmse,
            "mae": mae,
            "nrmse_std_original": nrmse,
            "mean_original": mean_orig,
            "mean_reconstructed": mean_recon,
            "mean_difference": mean_diff,
            "max_abs_error": max_abs,
            "pixel_count": int(count),
            "sampled_block_count": len(windows),
            "total_256x256_block_count": total_possible_blocks,
            "sample_block_size": STATS_BLOCK_SIZE,
        }

    return results


def dataset_summary(ds):
    md = ds.GetMetadata("IMAGE_STRUCTURE") or {}
    band1 = ds.GetRasterBand(1)
    blockx, blocky = band1.GetBlockSize()

    return {
        "width": ds.RasterXSize,
        "height": ds.RasterYSize,
        "band_count": ds.RasterCount,
        "projection_wkt_present": bool(ds.GetProjection()),
        "compression": md.get("COMPRESSION"),
        "interleave": md.get("INTERLEAVE"),
        "block_size": [blockx, blocky],
        "layout": "striped" if blockx == ds.RasterXSize else "tiled",
        "driver": ds.GetDriver().ShortName,
    }


def main():
    parser = argparse.ArgumentParser(
        description="LERC round-trip denoising with preserved GTiff layout and sampled per-band stats"
    )
    parser.add_argument("input", help="Input GeoTIFF")
    parser.add_argument("--z-errors", nargs="+", type=float, required=True)
    parser.add_argument("--output-dir", default=".")
    parser.add_argument("--json-name", default="stats.json")

    args = parser.parse_args()
    os.makedirs(args.output_dir, exist_ok=True)

    src_ds = gdal.Open(args.input, gdal.GA_ReadOnly)
    if src_ds is None:
        raise RuntimeError(f"Failed to open input file: {args.input}")

    creation_opts = detect_layout_creation_options(src_ds)

    stats_all = {
        "input_file": os.path.abspath(args.input),
        "input_summary": dataset_summary(src_ds),
        "stats_sampling": {
            "sample_block_size": STATS_BLOCK_SIZE,
            "max_sampled_blocks": MAX_STATS_BLOCKS,
            "gdal_cache_bytes": gdal.GetCacheMax(),
        },
        "results": []
    }

    base = os.path.splitext(os.path.basename(args.input))[0]

    for z in args.z_errors:
        print(f"\n=== Processing MAX_Z_ERROR={z} ===")

        z_label = int(z) if float(z).is_integer() else str(z).replace(".", "p")
        out_path = os.path.join(
            args.output_dir,
            f"{base}_Denoised_MaxZ{z_label}.tif"
        )

        lerc_roundtrip(args.input, out_path, z, creation_opts, src_ds)

        recon_ds = gdal.Open(out_path, gdal.GA_ReadOnly)
        if recon_ds is None:
            raise RuntimeError(f"Failed to open reconstructed output: {out_path}")

        stats = compute_stats_per_band(src_ds, recon_ds)

        stats_all["results"].append({
            "output_file": os.path.abspath(out_path),
            "z_error": z,
            "output_summary": dataset_summary(recon_ds),
            "bands": stats
        })

        recon_ds = None

    json_path = os.path.join(args.output_dir, args.json_name)
    with open(json_path, "w") as f:
        json.dump(stats_all, f, indent=2)

    print(f"\nSaved stats to {json_path}")


if __name__ == "__main__":
    main()