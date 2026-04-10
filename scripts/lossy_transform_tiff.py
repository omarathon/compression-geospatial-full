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


def create_output_dataset(output_tif, src_ds, creation_opts):
    driver = gdal.GetDriverByName("GTiff")
    out_ds = driver.Create(
        output_tif,
        src_ds.RasterXSize,
        src_ds.RasterYSize,
        src_ds.RasterCount,
        src_ds.GetRasterBand(1).DataType,
        options=creation_opts,
    )
    if out_ds is None:
        raise RuntimeError(f"Failed to create output dataset: {output_tif}")
    copy_metadata_and_band_properties(src_ds, out_ds)
    return out_ds


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


def median_filter_transform(output_tif, kernel_size, source_creation_opts, src_ds):
    try:
        from scipy.ndimage import median_filter
    except ImportError as e:
        raise RuntimeError(
            "Median filtering requires SciPy. Install it with: pip install scipy"
        ) from e

    if kernel_size <= 0 or kernel_size % 2 == 0:
        raise ValueError("--median-kernel must be a positive odd integer, e.g. 3 or 5")

    out_ds = create_output_dataset(output_tif, src_ds, source_creation_opts)

    for b in range(1, src_ds.RasterCount + 1):
        sb = src_ds.GetRasterBand(b)
        db = out_ds.GetRasterBand(b)

        arr = sb.ReadAsArray()
        if arr is None:
            raise RuntimeError(f"Failed to read band {b} for median filtering")

        nodata = sb.GetNoDataValue()
        nodata_mask = None
        if nodata is not None:
            nodata_mask = (arr == nodata)

        filtered = median_filter(arr, size=kernel_size, mode="nearest")

        if nodata_mask is not None:
            filtered = filtered.copy()
            filtered[nodata_mask] = nodata

        db.WriteArray(filtered)

    out_ds.FlushCache()
    out_ds = None


def bm3d_transform(output_tif, sigma, source_creation_opts, src_ds):
    try:
        import bm3d as bm3d_pkg
    except ImportError as e:
        raise RuntimeError(
            "BM3D requires the bm3d package. Install it with: pip install bm3d"
        ) from e

    if sigma <= 0:
        raise ValueError("--bm3d-sigma must be > 0")

    out_ds = create_output_dataset(output_tif, src_ds, source_creation_opts)

    for b in range(1, src_ds.RasterCount + 1):
        sb = src_ds.GetRasterBand(b)
        db = out_ds.GetRasterBand(b)

        arr = sb.ReadAsArray()
        if arr is None:
            raise RuntimeError(f"Failed to read band {b} for BM3D")

        nodata = sb.GetNoDataValue()
        nodata_mask = None
        if nodata is not None:
            nodata_mask = (arr == nodata)

        valid = arr if nodata is None else arr[~nodata_mask]
        if valid.size == 0:
            db.WriteArray(arr)
            continue

        vmin = float(valid.min())
        vmax = float(valid.max())
        scale = vmax - vmin
        if scale <= 0:
            den = arr.copy()
            if nodata_mask is not None:
                den[nodata_mask] = nodata
            db.WriteArray(den)
            continue

        arr_norm = (arr.astype(np.float32) - vmin) / scale
        sigma_norm = float(sigma) / scale

        den_norm = bm3d_pkg.bm3d(arr_norm, sigma_psd=sigma_norm)
        den = np.rint(den_norm * scale + vmin)

        if np.issubdtype(arr.dtype, np.integer):
            info = np.iinfo(arr.dtype)
            den = np.clip(den, info.min, info.max).astype(arr.dtype)
        else:
            den = den.astype(arr.dtype, copy=False)

        if nodata_mask is not None:
            den = den.copy()
            den[nodata_mask] = nodata

        db.WriteArray(den)

    out_ds.FlushCache()
    out_ds = None


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
        description="Raster transform (LERC or median or BM3D) with preserved GTiff layout and sampled per-band stats"
    )
    parser.add_argument("input", help="Input GeoTIFF")
    parser.add_argument("--output-dir", default=".")
    parser.add_argument("--json-name", default="stats.json")

    method_group = parser.add_mutually_exclusive_group(required=True)
    method_group.add_argument("--z-errors", nargs="+", type=float, help="LERC round-trip MaxZError values")
    method_group.add_argument("--median-kernel", type=int, help="Odd median kernel size, e.g. 3 or 5")
    method_group.add_argument("--bm3d-sigma", type=float, help="BM3D sigma in original pixel units")

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

    if args.z_errors is not None:
        for z in args.z_errors:
            print(f"\n=== Processing LERC MAX_Z_ERROR={z} ===")

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
                "method": "lerc",
                "parameter_name": "z_error",
                "parameter_value": z,
                "output_file": os.path.abspath(out_path),
                "output_summary": dataset_summary(recon_ds),
                "bands": stats
            })

            recon_ds = None

    elif args.median_kernel is not None:
        k = args.median_kernel
        print(f"\n=== Processing median filter kernel={k} ===")

        out_path = os.path.join(
            args.output_dir,
            f"{base}_MedianK{k}.tif"
        )

        median_filter_transform(out_path, k, creation_opts, src_ds)

        recon_ds = gdal.Open(out_path, gdal.GA_ReadOnly)
        if recon_ds is None:
            raise RuntimeError(f"Failed to open reconstructed output: {out_path}")

        stats = compute_stats_per_band(src_ds, recon_ds)

        stats_all["results"].append({
            "method": "median",
            "parameter_name": "kernel_size",
            "parameter_value": k,
            "output_file": os.path.abspath(out_path),
            "output_summary": dataset_summary(recon_ds),
            "bands": stats
        })

        recon_ds = None

    elif args.bm3d_sigma is not None:
        sigma = args.bm3d_sigma
        sigma_label = int(sigma) if float(sigma).is_integer() else str(sigma).replace(".", "p")
        print(f"\n=== Processing BM3D sigma={sigma} ===")

        out_path = os.path.join(
            args.output_dir,
            f"{base}_BM3DSigma{sigma_label}.tif"
        )

        bm3d_transform(out_path, sigma, creation_opts, src_ds)

        recon_ds = gdal.Open(out_path, gdal.GA_ReadOnly)
        if recon_ds is None:
            raise RuntimeError(f"Failed to open reconstructed output: {out_path}")

        stats = compute_stats_per_band(src_ds, recon_ds)

        stats_all["results"].append({
            "method": "bm3d",
            "parameter_name": "sigma",
            "parameter_value": sigma,
            "output_file": os.path.abspath(out_path),
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