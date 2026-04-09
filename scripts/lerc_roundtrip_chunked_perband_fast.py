#!/usr/bin/env python3

import os
import json
import math
import argparse
import numpy as np
from osgeo import gdal

gdal.UseExceptions()


def detect_layout_creation_options(ds):
    """
    Preserve the practical GTiff layout characteristics of the source:
    compression, interleave, tiling/striping, block size, predictor, nbits.
    """
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

    # Detect tiled vs striped from actual block layout.
    # Striped GTiff commonly shows Block = full_width x 1 (or full_width x rows_per_strip).
    # Tiled GTiff shows smaller repeating blocks, e.g. 512x512.
    is_striped = (blockx == xsize)

    if is_striped:
        creation.append("TILED=NO")
    else:
        creation.append("TILED=YES")
        creation.append(f"BLOCKXSIZE={blockx}")
        creation.append(f"BLOCKYSIZE={blocky}")

    return creation


def copy_metadata_and_band_properties(src_ds, dst_ds):
    """
    Copy dataset and band metadata/properties that gdal.Translate may not preserve exactly.
    We intentionally do not overwrite IMAGE_STRUCTURE metadata, because it should reflect
    the actual output file, not the source declaration.
    """
    src_domains = src_ds.GetMetadataDomainList() or []
    for domain in src_domains:
        if domain == "IMAGE_STRUCTURE":
            continue
        md = src_ds.GetMetadata(domain)
        if md:
            dst_ds.SetMetadata(md, domain)

    dst_ds.SetGeoTransform(src_ds.GetGeoTransform())
    dst_ds.SetProjection(src_ds.GetProjection())

    for b in range(1, src_ds.RasterCount + 1):
        sb = src_ds.GetRasterBand(b)
        db = dst_ds.GetRasterBand(b)

        # Per-band nodata
        nd = sb.GetNoDataValue()
        if nd is not None:
            db.SetNoDataValue(nd)

        # Common band properties
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

        # Per-band metadata domains
        band_domains = sb.GetMetadataDomainList() or []
        for domain in band_domains:
            if domain == "IMAGE_STRUCTURE":
                continue
            md = sb.GetMetadata(domain)
            if md:
                db.SetMetadata(md, domain)

    dst_ds.FlushCache()


def lerc_roundtrip(input_tif, output_tif, maxz, source_creation_opts, src_ds):
    """
    Step 1: source -> temporary LERC file
    Step 2: temporary LERC -> final output with preserved source layout/compression
    """
    temp_lerc = output_tif + ".lerc_tmp.tif"

    # Use source layout for temp too when practical, but swap compression to LERC.
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

    # Explicitly restore metadata/band properties/nodata after round-trip.
    out_ds = gdal.Open(output_tif, gdal.GA_Update)
    if out_ds is None:
        raise RuntimeError(f"Failed to open output for metadata copy: {output_tif}")
    copy_metadata_and_band_properties(src_ds, out_ds)
    out_ds = None

    os.remove(temp_lerc)


def choose_stats_chunk_shape(ds, band_index):
    """
    Read using windows aligned to the source block layout.
    For tiled rasters, this preserves good I/O behavior.
    For striped rasters, we batch multiple rows to reduce Python overhead.
    """
    band = ds.GetRasterBand(band_index)
    xsize = ds.RasterXSize
    ysize = ds.RasterYSize
    blockx, blocky = band.GetBlockSize()

    if blockx == xsize:
        # Striped: batch many rows together, but keep memory reasonable.
        # 512 or 1024 rows is a decent compromise.
        return xsize, min(max(blocky, 512), ysize)
    else:
        # Tiled: use multiples of native tile size to reduce loop overhead.
        mult = 4
        return min(blockx * mult, xsize), min(blocky * mult, ysize)


def compute_stats_per_band(orig_ds, recon_ds):
    """
    Efficient blockwise stats:
    - block-aligned windows
    - no expensive compaction copies like arr = arr[mask]
    - integer diff for integer rasters where possible
    """
    if (
        orig_ds.RasterXSize != recon_ds.RasterXSize
        or orig_ds.RasterYSize != recon_ds.RasterYSize
        or orig_ds.RasterCount != recon_ds.RasterCount
    ):
        raise RuntimeError("Original and reconstructed rasters are not shape-compatible")

    results = {}

    for b in range(1, orig_ds.RasterCount + 1):
        band_orig = orig_ds.GetRasterBand(b)
        band_recon = recon_ds.GetRasterBand(b)

        xsize = orig_ds.RasterXSize
        ysize = orig_ds.RasterYSize
        chunkx, chunky = choose_stats_chunk_shape(orig_ds, b)

        sum_sq = 0.0
        sum_abs = 0.0
        sum_diff = 0.0
        sum_orig = 0.0
        sum_recon = 0.0
        sum_orig_sq = 0.0
        max_abs = 0.0
        count = 0

        nodata = band_orig.GetNoDataValue()

        for y in range(0, ysize, chunky):
            rows = min(chunky, ysize - y)

            for x in range(0, xsize, chunkx):
                cols = min(chunkx, xsize - x)

                o = band_orig.ReadAsArray(x, y, cols, rows)
                r = band_recon.ReadAsArray(x, y, cols, rows)

                if o is None or r is None:
                    raise RuntimeError(f"ReadAsArray failed for band {b}, window x={x}, y={y}")

                if nodata is None:
                    mask = None
                    valid_count = o.size
                else:
                    mask = (o != nodata)
                    valid_count = int(mask.sum())

                if valid_count == 0:
                    continue

                # Use integer diff for integer rasters; float64 otherwise.
                if np.issubdtype(o.dtype, np.integer) and np.issubdtype(r.dtype, np.integer):
                    diff = r.astype(np.int64, copy=False) - o.astype(np.int64, copy=False)
                    o_acc = o.astype(np.float64, copy=False)
                    r_acc = r.astype(np.float64, copy=False)
                else:
                    o_acc = o.astype(np.float64, copy=False)
                    r_acc = r.astype(np.float64, copy=False)
                    diff = r_acc - o_acc

                if mask is None:
                    sum_diff += float(np.sum(diff, dtype=np.float64))
                    absdiff = np.abs(diff)
                    sum_abs += float(np.sum(absdiff, dtype=np.float64))
                    sum_sq += float(np.sum(diff * diff, dtype=np.float64))
                    max_abs = max(max_abs, float(np.max(absdiff)))
                    sum_orig += float(np.sum(o_acc, dtype=np.float64))
                    sum_recon += float(np.sum(r_acc, dtype=np.float64))
                    sum_orig_sq += float(np.sum(o_acc * o_acc, dtype=np.float64))
                    count += valid_count
                else:
                    sum_diff += float(np.sum(diff, where=mask, dtype=np.float64))
                    absdiff = np.abs(diff)
                    sum_abs += float(np.sum(absdiff, where=mask, dtype=np.float64))
                    sum_sq += float(np.sum(diff * diff, where=mask, dtype=np.float64))
                    sum_orig += float(np.sum(o_acc, where=mask, dtype=np.float64))
                    sum_recon += float(np.sum(r_acc, where=mask, dtype=np.float64))
                    sum_orig_sq += float(np.sum(o_acc * o_acc, where=mask, dtype=np.float64))

                    # max with mask still needs selection somewhere
                    chunk_max = float(np.max(absdiff[mask]))
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
        description="LERC round-trip denoising with preserved GTiff layout and efficient per-band stats"
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