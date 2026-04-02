#!/usr/bin/env python3
"""Create a collage PNG from a folder of crop PNGs.

Reads all PNGs in a directory, arranges them in a grid (using r/c indices
from filenames if available, otherwise row-major order), and writes a
single collage PNG with labels.

Usage:
  python collage.py <input_dir> [output.png]

If output is omitted, writes <input_dir>.png next to the input directory.
"""

import argparse
import os
import re
import sys
from PIL import Image, ImageDraw, ImageFont


def parse_grid_pos(filename):
    """Extract (row, col) from filename like ..._crop_r2_c3.png or ..._boundary_5.png"""
    m = re.search(r'_crop_r(\d+)_c(\d+)', filename)
    if m:
        return ('crop', int(m.group(1)), int(m.group(2)))
    m = re.search(r'_boundary_(\d+)', filename)
    if m:
        return ('boundary', int(m.group(1)), 0)
    return None


def collage(input_dir, output_path, padding=4, label_height=16):
    pngs = sorted(f for f in os.listdir(input_dir) if f.lower().endswith('.png'))
    if not pngs:
        print(f"No PNGs found in {input_dir}", file=sys.stderr)
        sys.exit(1)

    # Parse positions
    items = []
    for f in pngs:
        pos = parse_grid_pos(f)
        items.append((f, pos))

    # Separate crops (grid) and boundary images
    crops = [(f, p) for f, p in items if p and p[0] == 'crop']
    boundaries = [(f, p) for f, p in items if p and p[0] == 'boundary']
    others = [(f, p) for f, p in items if p is None]

    # Determine grid dimensions from crop indices
    if crops:
        max_r = max(p[1] for _, p in crops) + 1
        max_c = max(p[2] for _, p in crops) + 1
    else:
        # No grid info, lay out in a square-ish grid
        n = len(pngs)
        max_c = int(n ** 0.5 + 0.999)
        max_r = (n + max_c - 1) // max_c

    # Load one image to get tile size
    sample = Image.open(os.path.join(input_dir, pngs[0]))
    tw, th = sample.size

    cell_w = tw + padding
    cell_h = th + padding + label_height

    # Boundary images go in extra rows below the grid
    n_boundary_rows = 0
    if boundaries:
        n_boundary_rows = 1  # label row
        n_boundary_cols = max_c
        n_boundary_rows += (len(boundaries) + n_boundary_cols - 1) // n_boundary_cols

    total_rows = max_r + n_boundary_rows
    total_cols = max_c

    canvas_w = total_cols * cell_w + padding
    canvas_h = total_rows * cell_h + padding

    canvas = Image.new('RGB', (canvas_w, canvas_h), (40, 40, 40))
    draw = ImageDraw.Draw(canvas)

    try:
        font = ImageFont.truetype("/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf", 11)
    except (OSError, IOError):
        font = ImageFont.load_default()

    def paste_tile(img, row, col, label):
        x = padding + col * cell_w
        y = padding + row * cell_h
        canvas.paste(img, (x, y))
        draw.text((x, y + th + 2), label, fill=(200, 200, 200), font=font)

    # Place grid crops
    for f, p in crops:
        img = Image.open(os.path.join(input_dir, f))
        r, c = p[1], p[2]
        paste_tile(img, r, c, f"r{r}c{c}")

    # Place boundary crops below
    if boundaries:
        base_row = max_r
        for i, (f, p) in enumerate(boundaries):
            img = Image.open(os.path.join(input_dir, f))
            r = base_row + i // max_c
            c = i % max_c
            paste_tile(img, r, c, f"bnd{p[1]}")

    # Place any unrecognized files in remaining space
    if others:
        base_row = max_r + n_boundary_rows
        for i, (f, _) in enumerate(others):
            img = Image.open(os.path.join(input_dir, f))
            r = base_row + i // max_c
            c = i % max_c
            # Extend canvas if needed
            needed_h = (r + 1) * cell_h + padding
            if needed_h > canvas.height:
                new_canvas = Image.new('RGB', (canvas_w, needed_h), (40, 40, 40))
                new_canvas.paste(canvas, (0, 0))
                canvas = new_canvas
                draw = ImageDraw.Draw(canvas)
            paste_tile(img, r, c, f[:20])

    canvas.save(output_path)
    print(f"Collage: {canvas.width}x{canvas.height} -> {output_path}")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("input_dir", help="Directory containing crop PNGs")
    parser.add_argument("output", nargs="?", help="Output PNG path (default: <input_dir>.png)")
    parser.add_argument("--padding", type=int, default=4, help="Padding between tiles (default 4)")
    args = parser.parse_args()

    output = args.output
    if not output:
        output = args.input_dir.rstrip("/\\") + ".png"

    collage(args.input_dir, output, padding=args.padding)


if __name__ == "__main__":
    main()
