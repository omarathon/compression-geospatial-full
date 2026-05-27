#!/usr/bin/env python3
"""
check_nodata_bug.py
Checks all benchmark TIFs for the bench_comp UInt16 nodata bug:
  triggered when GDAL type=UInt16 AND nodata >= 32768
  (cast to int16_t gives a negative value, wrongly triggers GDT_Int16 read path)
"""
import sys
import glob
from osgeo import gdal

gdal.UseExceptions()

GLOBS = [
    "/maps/omsst2/diss/papers/rasterlite/landsat_pano/LC08_192029_20130702_B8.TIF",
    "/maps/omsst2/diss/papers/rasterlite/SRTM_Italy/srtm_italy_exact/srtm_tiles/SRTM_Italy_final.tif",
    "/maps/omsst2/diss/papers/rasterlite/ETOPO1/ETOPO1_Ice_g_geotiff.tif",
    "/maps/omsst2/diss/papers/zalipynis/2018/landsat8_mosaic_B4.tif",
    "/maps/omsst2/diss/papers/zalipynis/2018/landsat8_mosaic_B5.tif",
    "/maps/omsst2/diss/papers/zalipynis/2018_2/landsat/landsat8_path190_row031_stack.tif",
    "/maps/omsst2/diss/papers/zalipynis/2019_landsat_band4_mosaic/exact/landsat8_mosaic_B4.tif",
    "/maps/omsst2/diss/papers/zaytar/2025/final/*.tif",
    "/maps/omsst2/diss/others/landsat8/*.tif",
    "/maps/omsst2/diss/others/sentinel2/*.tif",
    "/maps/omsst2/diss/others/srtm_highres/*.tif",
    "/maps/omsst2/diss/others/etopo1/*.tif",
    "/maps/omsst2/diss/others/etopo_highres_quant/*.tif",
    "/maps/omsst2/diss/others/srtm/*.tif",
]

n_affected = n_safe = n_skip = n_missing = 0

def check_file(path):
    global n_affected, n_safe, n_skip, n_missing
    try:
        ds = gdal.Open(path, gdal.GA_ReadOnly)
    except Exception as e:
        print(f"MISSING  {path}  ({e})")
        n_missing += 1
        return
    if ds is None:
        print(f"MISSING  {path}")
        n_missing += 1
        return

    for i in range(1, ds.RasterCount + 1):
        band = ds.GetRasterBand(i)
        dt = gdal.GetDataTypeName(band.DataType)   # e.g. "UInt16", "Int16", "Byte"

        nodata_val, has_nodata = band.GetNoDataValue(), band.GetNoDataValue() is not None
        # GetNoDataValue returns None when not set; some GDAL versions return (val, ok) — handle both
        if isinstance(nodata_val, tuple):
            nodata_val, has_nodata_flag = nodata_val
            has_nodata = bool(has_nodata_flag)

        tag = f"band {i}, {dt}"

        if dt != "UInt16":
            print(f"SAFE     [{tag}] {path}")
            n_safe += 1
            continue

        if not has_nodata or nodata_val is None:
            print(f"SKIP     [{tag}, no nodata] {path}")
            n_skip += 1
            continue

        nodata_int = int(nodata_val)
        if nodata_int >= 32768:
            print(f"AFFECTED [{tag}, nodata={nodata_int} (>=32768 → int16={nodata_int - 65536})] {path}")
            n_affected += 1
        else:
            print(f"SAFE     [{tag}, nodata={nodata_int} (<32768)] {path}")
            n_safe += 1

    ds = None

all_paths = []
for pattern in GLOBS:
    matches = sorted(glob.glob(pattern))
    if matches:
        all_paths.extend(matches)
    elif '*' not in pattern and '?' not in pattern:
        all_paths.append(pattern)   # non-glob: check existence in check_file

for path in all_paths:
    check_file(path)

print()
print("=" * 56)
print(f"AFFECTED (bench_comp results invalid): {n_affected} bands")
print(f"SAFE     (results trustworthy):        {n_safe} bands")
print(f"SKIP     (no nodata, always safe):     {n_skip} bands")
print(f"MISSING  files:                        {n_missing}")
print("=" * 56)
