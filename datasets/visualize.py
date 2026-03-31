#!/usr/bin/env python3
"""Visualize 256x256 native-resolution crops from a GeoTIFF as a single collage PNG.

Produces one collage image with:
  - A grid of evenly-spaced crops across the raster
  - Extra crops at nodata/valid-data boundaries (for mosaics)

Usage:
  python3 visualize.py <input.tif> <output.png> [--grid 5] [--crop-size 256]
"""

import argparse
import os
import sys

import numpy as np
from osgeo import gdal

gdal.UseExceptions()

MAGENTA = np.array([255, 0, 255], dtype=np.uint8)
BG_COLOR = np.array([40, 40, 40], dtype=np.uint8)
LABEL_HEIGHT = 18
PADDING = 4


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

    Returns (H, W, 3) uint8 ndarray or None.
    """
    nbands = ds.RasterCount
    xsize = ds.RasterXSize
    ysize = ds.RasterYSize

    read_w = min(size, xsize - xoff)
    read_h = min(size, ysize - yoff)
    if read_w <= 0 or read_h <= 0:
        return None

    bands = []
    for b in range(1, nbands + 1):
        band = ds.GetRasterBand(b)
        arr = band.ReadAsArray(xoff, yoff, read_w, read_h).astype(np.float64)
        bands.append(arr)

    mask = np.ones((read_h, read_w), dtype=bool)
    for i, arr in enumerate(bands):
        nd = nodata_vals[i]
        if nd is not None:
            mask &= arr != nd

    if nbands == 1:
        gray = percentile_stretch(bands[0], mask)
        img = np.stack([gray, gray, gray], axis=-1)
    elif nbands >= 3:
        r = percentile_stretch(bands[0], mask)
        g = percentile_stretch(bands[1], mask)
        b = percentile_stretch(bands[2], mask)
        img = np.stack([r, g, b], axis=-1)
    else:
        gray = percentile_stretch(bands[0], mask)
        img = np.stack([gray, gray, gray], axis=-1)

    img[~mask] = MAGENTA

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


def draw_text(canvas, x, y, text):
    """Draw simple white text onto the canvas using a minimal built-in font."""
    # 3x5 pixel font for digits, lowercase, and a few symbols
    glyphs = {
        '0': ["111","101","101","101","111"], '1': ["010","110","010","010","111"],
        '2': ["111","001","111","100","111"], '3': ["111","001","111","001","111"],
        '4': ["101","101","111","001","001"], '5': ["111","100","111","001","111"],
        '6': ["111","100","111","101","111"], '7': ["111","001","001","001","001"],
        '8': ["111","101","111","101","111"], '9': ["111","101","111","001","111"],
        'r': ["000","011","100","100","100"], 'c': ["000","011","100","100","011"],
        'b': ["100","100","111","101","111"], 'n': ["000","110","101","101","101"],
        'd': ["001","001","111","101","111"],
    }
    cx = x
    h, w = canvas.shape[:2]
    for ch in text:
        g = glyphs.get(ch)
        if g is None:
            cx += 4
            continue
        for gy, row in enumerate(g):
            for gx, px in enumerate(row):
                if px == '1':
                    py, px2 = y + gy, cx + gx
                    if 0 <= py < h and 0 <= px2 < w:
                        canvas[py, px2] = [200, 200, 200]
        cx += 4


def compute_grid_positions(ds, grid, crop_size):
    """Return list of (row, col, xoff, yoff, label) for the grid crops."""
    xsize = ds.RasterXSize
    ysize = ds.RasterYSize
    if grid <= 1:
        xs = [xsize // 2 - crop_size // 2]
        ys = [ysize // 2 - crop_size // 2]
    else:
        xs = np.linspace(0, xsize - crop_size, grid, dtype=int)
        ys = np.linspace(0, ysize - crop_size, grid, dtype=int)

    positions = []
    for ri, yp in enumerate(ys):
        for ci, xp in enumerate(xs):
            xoff = max(0, int(xp))
            yoff = max(0, int(yp))
            positions.append((ri, ci, xoff, yoff, f"{xoff},{yoff}"))
    return positions


def compute_boundary_positions(ds, crop_size, nodata_vals, grid_cols, max_crops=10):
    """Return list of (row, col, xoff, yoff, label) for boundary crops.

    Row indices start at 0 (caller offsets them below the grid).
    """
    xsize = ds.RasterXSize
    ysize = ds.RasterYSize

    has_nodata = any(nd is not None for nd in nodata_vals)
    if not has_nodata:
        print("  (no nodata value — skipping boundary detection)")
        return []

    scale = 64
    coarse_w = max(1, xsize // scale)
    coarse_h = max(1, ysize // scale)

    band = ds.GetRasterBand(1)
    coarse = band.ReadAsArray(0, 0, xsize, ysize, buf_xsize=coarse_w, buf_ysize=coarse_h)
    nd = nodata_vals[0]
    valid_mask = coarse != nd

    valid_frac = valid_mask.mean()
    if valid_frac == 0.0 or valid_frac == 1.0:
        print("  (no nodata boundaries found)")
        return []

    gy = np.diff(valid_mask.astype(np.int8), axis=0)
    gx = np.diff(valid_mask.astype(np.int8), axis=1)
    edge = np.zeros_like(valid_mask, dtype=bool)
    edge[:-1, :] |= gy != 0
    edge[:, :-1] |= gx != 0

    edge_ys, edge_xs = np.where(edge)
    if len(edge_ys) == 0:
        print("  (no nodata boundaries found)")
        return []

    n = min(max_crops, len(edge_ys))
    indices = np.linspace(0, len(edge_ys) - 1, n, dtype=int)

    half = crop_size // 2
    positions = []
    for i, idx in enumerate(indices):
        cx = int(edge_xs[idx]) * scale
        cy = int(edge_ys[idx]) * scale
        xoff = max(0, min(cx - half, xsize - crop_size))
        yoff = max(0, min(cy - half, ysize - crop_size))
        r = i // grid_cols
        c = i % grid_cols
        positions.append((r, c, xoff, yoff, f"{xoff},{yoff}"))

    return positions


def visualize(input_path, output_path, grid=5, crop_size=256):
    ds = gdal.Open(input_path, gdal.GA_ReadOnly)
    if ds is None:
        print(f"ERROR: Cannot open {input_path}", file=sys.stderr)
        sys.exit(1)

    name = os.path.splitext(os.path.basename(input_path))[0]
    nbands = ds.RasterCount
    xsize = ds.RasterXSize
    ysize = ds.RasterYSize

    nodata_vals = []
    for b in range(1, nbands + 1):
        nd = ds.GetRasterBand(b).GetNoDataValue()
        nodata_vals.append(nd)

    print(f"{input_path}")
    print(f"  {xsize}x{ysize}, {nbands} band(s), nodata={nodata_vals}")

    # Compute all crop positions
    print(f"  Grid crops ({grid}x{grid}):")
    grid_pos = compute_grid_positions(ds, grid, crop_size)

    print(f"  Boundary crops:")
    boundary_pos = compute_boundary_positions(ds, crop_size, nodata_vals, grid)

    # Layout: grid rows, then boundary rows below
    grid_rows = grid
    boundary_rows = 0
    if boundary_pos:
        boundary_rows = max(r for r, c, *_ in boundary_pos) + 1
    total_rows = grid_rows + boundary_rows
    total_cols = grid

    cell_w = crop_size + PADDING
    cell_h = crop_size + PADDING + LABEL_HEIGHT
    canvas_w = total_cols * cell_w + PADDING
    canvas_h = total_rows * cell_h + PADDING

    canvas = np.zeros((canvas_h, canvas_w, 3), dtype=np.uint8)
    canvas[:, :] = BG_COLOR

    def paste(img, row, col, label):
        x = PADDING + col * cell_w
        y = PADDING + row * cell_h
        canvas[y:y + crop_size, x:x + crop_size] = img
        draw_text(canvas, x + 2, y + crop_size + 4, label)

    # Render and paste grid crops
    for ri, ci, xoff, yoff, label in grid_pos:
        img = render_crop(ds, xoff, yoff, crop_size, nodata_vals)
        if img is None:
            continue
        paste(img, ri, ci, label)
        print(f"    {label}  (offset {xoff},{yoff})")

    # Render and paste boundary crops
    for ri, ci, xoff, yoff, label in boundary_pos:
        img = render_crop(ds, xoff, yoff, crop_size, nodata_vals)
        if img is None:
            continue
        paste(img, grid_rows + ri, ci, label)
        print(f"    {label}  (offset {xoff},{yoff})")

    # For 4+ band files, add extra band crops in another row
    extra_row = total_rows
    if nbands > 3:
        print(f"  Extra bands ({nbands - 3} bands, center crop):")
        cx = xsize // 2 - crop_size // 2
        cy = ysize // 2 - crop_size // 2
        n_extra = nbands - 3
        extra_rows_needed = (n_extra + total_cols - 1) // total_cols
        # Extend canvas
        new_h = canvas_h + extra_rows_needed * cell_h
        new_canvas = np.zeros((new_h, canvas_w, 3), dtype=np.uint8)
        new_canvas[:, :] = BG_COLOR
        new_canvas[:canvas_h, :, :] = canvas
        canvas = new_canvas
        canvas_h = new_h

        for bi, b in enumerate(range(4, nbands + 1)):
            band = ds.GetRasterBand(b)
            arr = band.ReadAsArray(cx, cy, crop_size, crop_size).astype(np.float64)
            nd = nodata_vals[b - 1]
            mask = np.ones_like(arr, dtype=bool)
            if nd is not None:
                mask = arr != nd
            gray = percentile_stretch(arr, mask)
            img = np.stack([gray, gray, gray], axis=-1)
            img[~mask] = MAGENTA
            r = extra_row + bi // total_cols
            c = bi % total_cols
            label = f"b{b}"
            paste(img, r, c, label)
            print(f"    band {b}")

    # Save collage
    os.makedirs(os.path.dirname(output_path) or ".", exist_ok=True)
    save_png(canvas, output_path)
    print(f"  Collage: {canvas_w}x{canvas_h} -> {output_path}")
    del ds


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("input", help="Path to input GeoTIFF")
    parser.add_argument("output", help="Output PNG path")
    parser.add_argument("--grid", type=int, default=5,
                        help="Grid dimension (NxN crops, default 5)")
    parser.add_argument("--crop-size", type=int, default=256,
                        help="Crop size in pixels (default 256)")
    args = parser.parse_args()
    visualize(args.input, args.output, args.grid, args.crop_size)


if __name__ == "__main__":
    main()
