#!/usr/bin/env python3
"""Verify bench_pipeline_2band NDVI results against GDAL ground truth."""
import argparse, subprocess, re
import numpy as np
from osgeo import gdal
gdal.UseExceptions()

def sample_block_offsets(bw, bh, bs, n):
    total = bw * bh
    n = min(n, total)
    interval = max(1, total // n)
    offsets = []
    idx = 0
    for _ in range(n):
        offsets.append(((idx % bw) * bs, (idx // bw) * bs))
        idx += interval
    return offsets

def read_block(band, x, y, bs, nodata):
    a = band.ReadAsArray(x, y, bs, bs).astype(np.uint16)
    if nodata is not None:
        a[a == np.uint16(int(nodata))] = 0   # replicate bench nodata→0
    return a

def ndvi_sum_block(a, b):
    af = a.astype(np.float32); bf = b.astype(np.float32)
    den = af + bf
    mask = den > 0
    return float(np.where(mask, (af - bf) / np.where(mask, den, 1.0), 0.0).sum())

def count_block_fxp(a, b, k1, k2):
    # vpmaddwd treats uint16 as signed int16; replicate with view
    a_s = a.view(np.int16).astype(np.int32)
    b_s = b.view(np.int16).astype(np.int32)
    return int(np.sum(a_s * k1 - b_s * k2 > 0))

def run_bench(bp, fA, fB, op, n, bs, threshold):
    cmd = [bp, fA, '--fileB', fB, '-b', str(bs), '-n', str(n),
           '-r', '3', '--rs', '1', '-X', '1',
           '--icodec', 'custom_direct_access', '--op', op,
           '--threshold', str(threshold)]
    out = subprocess.run(cmd, capture_output=True, text=True, timeout=300).stdout
    m = re.search(r'result:([-\d.eE+nan]+)', out)
    return float(m.group(1)) if m else None

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('fileA'); ap.add_argument('fileB')
    ap.add_argument('-n', type=int, required=True)
    ap.add_argument('-b', '--blocksize', type=int, default=256)
    ap.add_argument('--threshold', type=float, default=0.3)
    ap.add_argument('--bench', default=
        '/home/omsst2/diss/compression-geospatial-full/build/bench_pipeline_2band')
    args = ap.parse_args()

    dsA = gdal.Open(args.fileA); bndA = dsA.GetRasterBand(1)
    dsB = gdal.Open(args.fileB); bndB = dsB.GetRasterBand(1)
    ndA = bndA.GetNoDataValue(); ndB = bndB.GetNoDataValue()
    bs = args.blocksize
    bw = bndA.XSize // bs; bh = bndA.YSize // bs
    offsets = sample_block_offsets(bw, bh, bs, args.n)

    x = args.threshold; SCALE = 4096.0
    k1 = int(round((1.0 - x) * SCALE))
    k2 = int(round((1.0 + x) * SCALE))
    print(f"Raster {bndA.XSize}x{bndA.YSize}, {bw}x{bh}={bw*bh} blocks, "
          f"sampling {len(offsets)}, nodata A={ndA} B={ndB}")
    print(f"threshold={x}  K1={k1}  K2={k2}")

    ref_sum = 0.0; ref_count = 0
    for px, py in offsets:
        a = read_block(bndA, px, py, bs, ndA)
        b = read_block(bndB, px, py, bs, ndB)
        ref_sum   += ndvi_sum_block(a, b)
        ref_count += count_block_fxp(a, b, k1, k2)

    print(f"\nGDAL reference (nodata→0, float32 NDVI, fixed-point count):")
    print(f"  sum(NDVI)           = {ref_sum:.6f}")
    print(f"  count(NDVI>{x:.1f}) = {ref_count}")

    print(f"\nBench results (custom_direct_access):")
    max_reldiff = 0.0
    for op in ['ndvi_div', 'ndvi_rcp', 'ndvi_rcpraw']:
        v = run_bench(args.bench, args.fileA, args.fileB, op, args.n, bs, x)
        if v is None: print(f"  {op}: no result"); continue
        adiff = abs(v - ref_sum)
        rdiff = adiff / (abs(ref_sum) + 1e-12)
        max_reldiff = max(max_reldiff, rdiff)
        print(f"  {op:<14}: bench={v:.4f}  ref={ref_sum:.4f}  "
              f"abs_diff={adiff:.4f}  rel_diff={rdiff:.2e}")
    print(f"  max rel_diff (sum): {max_reldiff:.2e}")

    v = run_bench(args.bench, args.fileA, args.fileB, 'count', args.n, bs, x)
    if v is not None:
        bc = int(v)
        ok = "EXACT MATCH" if bc == ref_count else f"MISMATCH diff={bc - ref_count}"
        print(f"  count          : bench={bc}  ref={ref_count}  -> {ok}")

if __name__ == '__main__':
    main()
