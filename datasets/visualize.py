#!/usr/bin/env python3
"""Visualize 256x256 native-resolution crops from a GeoTIFF for corruption checking.

Produces:
  - A grid of evenly-spaced crops across the raster
  - Extra crops at nodata/valid-data boundaries (for mosaics)

Usage:
  python datasets/visualize.py <input.tif> <output_dir> [--grid 5] [--crop-size 256]
"""

import argparse
import os
import sys

import numpy as np
from osgeo import gdal

gdal.UseExceptions()

MAGENTA = np.array([255, 0, 255], dtype=np.uint8)


def percentile_stretch(arr, mask, lo=2, hi=98):
    """Stretch valid pixels to 0-255 using percentiles."""
    valid = arr[mask]
    if valid.size == 0:
        return np.zeros_like(arr, dtype=np.uint8)
    p_lo, p_hi = np.percentile(valid, [lo, hi])
    if p_lo == p_hi:
        p_hi = p_lo + 1
    stretched = np.clip((arr.astype(np.float64) - p_lo) / (p_hi - p_lo) * 255, 0, 255)
    return stretched.astype(np.uint8)


def render_crop(ds, xoff, yoff, size, nodata_vals):
    """Read a crop from the dataset and render it as an RGB uint8 array.

    Returns (H, W, 3) uint8 ndarray.
    """
    nbands = ds.RasterCount
    xsize = ds.RasterXSize
    ysize = ds.RasterYSize

    # Clamp to raster extent
    read_w = min(size, xsize - xoff)
    read_h = min(size, ysize - yoff)
    if read_w <= 0 or read_h <= 0:
        return None

    # Read all bands
    bands = []
    for b in range(1, nbands + 1):
        band = ds.GetRasterBand(b)
        arr = band.ReadAsArray(xoff, yoff, read_w, read_h).astype(np.float64)
        bands.append(arr)

    # Build combined nodata mask (True = valid)
    mask = np.ones((read_h, read_w), dtype=bool)
    for i, arr in enumerate(bands):
        nd = nodata_vals[i]
        if nd is not None:
            mask &= arr != nd

    if nbands == 1:
        # Grayscale -> RGB
        gray = percentile_stretch(bands[0], mask)
        img = np.stack([gray, gray, gray], axis=-1)
    elif nbands >= 3:
        # Use first 3 bands as RGB
        r = percentile_stretch(bands[0], mask)
        g = percentile_stretch(bands[1], mask)
        b = percentile_stretch(bands[2], mask)
        img = np.stack([r, g, b], axis=-1)
    else:
        # 2-band: show band 1 as grayscale
        gray = percentile_stretch(bands[0], mask)
        img = np.stack([gray, gray, gray], axis=-1)

    # Paint nodata pixels magenta
    img[~mask] = MAGENTA

    # Pad to full crop size if we hit the edge
    if read_w < size or read_h < size:
        padded = np.zeros((size, size, 3), dtype=np.uint8)
        padded[:, :] = MAGENTA
        padded[:read_h, :read_w] = img
        img = padded

    return img


def save_png(img, path):
    """Save an (H, W, 3) uint8 array as a PNG via GDAL."""
    h, w = img.shape[:2]
    mem_drv = gdal.GetDriverByName("MEM")
    mem_ds = mem_drv.Create("", w, h, 3, gdal.GDT_Byte)
    for b in range(3):
        mem_ds.GetRasterBand(b + 1).WriteArray(img[:, :, b])
    png_drv = gdal.GetDriverByName("PNG")
    png_drv.CreateCopy(path, mem_ds, 0)
    del mem_ds


def grid_crops(ds, grid, crop_size, nodata_vals, out_dir, name):
    """Generate evenly-spaced crops across the raster."""
    xsize = ds.RasterXSize
    ysize = ds.RasterYSize

    # Compute grid positions: evenly spaced so crops span the full extent
    if grid <= 1:
        xs = [xsize // 2 - crop_size // 2]
        ys = [ysize // 2 - crop_size // 2]
    else:
        xs = np.linspace(0, xsize - crop_size, grid, dtype=int)
        ys = np.linspace(0, ysize - crop_size, grid, dtype=int)

    count = 0
    for ri, y in enumerate(ys):
        for ci, x in enumerate(xs):
            xoff = max(0, int(x))
            yoff = max(0, int(y))
            img = render_crop(ds, xoff, yoff, crop_size, nodata_vals)
            if img is None:
                continue
            fname = f"{name}_crop_r{ri}_c{ci}.png"
            fpath = os.path.join(out_dir, fname)
            save_png(img, fpath)
            count += 1
            print(f"  {fname}  (offset {xoff},{yoff})")

    return count


def boundary_crops(ds, crop_size, nodata_vals, out_dir, name, max_crops=10):
    """Find nodata/valid-data boundaries and place crops there."""
    xsize = ds.RasterXSize
    ysize = ds.RasterYSize

    # Check if any band has nodata
    has_nodata = any(nd is not None for nd in nodata_vals)
    if not has_nodata:
        print("  (no nodata value — skipping boundary detection)")
        return 0

    # Read a coarse version of band 1 to detect nodata edges
    scale = 64
    coarse_w = max(1, xsize // scale)
    coarse_h = max(1, ysize // scale)

    band = ds.GetRasterBand(1)
    coarse = band.ReadAsArray(0, 0, xsize, ysize, buf_xsize=coarse_w, buf_ysize=coarse_h)
    nd = nodata_vals[0]
    valid_mask = coarse != nd

    # If fully valid or fully nodata, no boundaries
    valid_frac = valid_mask.mean()
    if valid_frac == 0.0 or valid_frac == 1.0:
        print("  (no nodata boundaries found)")
        return 0

    # Find boundary pixels via gradient of the valid mask
    gy = np.diff(valid_mask.astype(np.int8), axis=0)
    gx = np.diff(valid_mask.astype(np.int8), axis=1)
    edge = np.zeros_like(valid_mask, dtype=bool)
    edge[:-1, :] |= gy != 0
    edge[:, :-1] |= gx != 0

    edge_ys, edge_xs = np.where(edge)
    if len(edge_ys) == 0:
        print("  (no nodata boundaries found)")
        return 0

    # Subsample boundary points evenly
    n = min(max_crops, len(edge_ys))
    indices = np.linspace(0, len(edge_ys) - 1, n, dtype=int)

    count = 0
    half = crop_size // 2
    for i, idx in enumerate(indices):
        # Map coarse coords back to full-res, center crop on boundary
        cx = int(edge_xs[idx]) * scale
        cy = int(edge_ys[idx]) * scale
        xoff = max(0, min(cx - half, xsize - crop_size))
        yoff = max(0, min(cy - half, ysize - crop_size))

        img = render_crop(ds, xoff, yoff, crop_size, nodata_vals)
        if img is None:
            continue
        fname = f"{name}_boundary_{i}.png"
        fpath = os.path.join(out_dir, fname)
        save_png(img, fpath)
        count += 1
        print(f"  {fname}  (offset {xoff},{yoff})")

    return count


def visualize(input_path, output_dir, grid=5, crop_size=256):
    ds = gdal.Open(input_path, gdal.GA_ReadOnly)
    if ds is None:
        print(f"ERROR: Cannot open {input_path}", file=sys.stderr)
        sys.exit(1)

    name = os.path.splitext(os.path.basename(input_path))[0]
    nbands = ds.RasterCount
    xsize = ds.RasterXSize
    ysize = ds.RasterYSize

    # Collect nodata values per band
    nodata_vals = []
    for b in range(1, nbands + 1):
        nd = ds.GetRasterBand(b).GetNoDataValue()
        nodata_vals.append(nd)

    os.makedirs(output_dir, exist_ok=True)

    print(f"{input_path}")
    print(f"  {xsize}x{ysize}, {nbands} band(s), nodata={nodata_vals}")

    # Grid crops
    print(f"  Grid crops ({grid}x{grid}):")
    n_grid = grid_crops(ds, grid, crop_size, nodata_vals, output_dir, name)

    # Boundary crops
    print(f"  Boundary crops:")
    n_boundary = boundary_crops(ds, crop_size, nodata_vals, output_dir, name)

    # For multi-band (4+), also dump extra bands as separate grayscale crops (center only)
    if nbands > 3:
        print(f"  Extra bands ({nbands - 3} bands, center crop):")
        cx = xsize // 2 - crop_size // 2
        cy = ysize // 2 - crop_size // 2
        for b in range(4, nbands + 1):
            band = ds.GetRasterBand(b)
            arr = band.ReadAsArray(cx, cy, crop_size, crop_size).astype(np.float64)
            nd = nodata_vals[b - 1]
            mask = np.ones_like(arr, dtype=bool)
            if nd is not None:
                mask = arr != nd
            gray = percentile_stretch(arr, mask)
            img = np.stack([gray, gray, gray], axis=-1)
            img[~mask] = MAGENTA
            fname = f"{name}_band{b}_center.png"
            fpath = os.path.join(output_dir, fname)
            save_png(img, fpath)
            print(f"  {fname}")

    total = n_grid + n_boundary
    print(f"  Done: {total} PNGs -> {output_dir}")
    del ds


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("input", help="Path to input GeoTIFF")
    parser.add_argument("output_dir", help="Directory to write PNGs into")
    parser.add_argument("--grid", type=int, default=5,
                        help="Grid dimension (NxN crops, default 5)")
    parser.add_argument("--crop-size", type=int, default=256,
                        help="Crop size in pixels (default 256)")
    args = parser.parse_args()
    visualize(args.input, args.output_dir, args.grid, args.crop_size)


if __name__ == "__main__":
    main()
