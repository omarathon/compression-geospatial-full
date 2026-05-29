#!/usr/bin/env python3
"""Print per-TIF std and collection-mean std for a given collection name.

Usage:
    python3 query_index_std.py <index.json> <collection_name>
"""
import json, sys

index_path, collection_name = sys.argv[1], sys.argv[2]

with open(index_path) as f:
    raw = json.load(f)
index = raw.get("tifs", raw)

# Find collection in COLLECTIONS (same list as build_lossy_index.py)
DISS = "/maps/omsst2/diss"
import glob as glob_module

COLLECTIONS = [
    ("Fuieri_2014_Landsat8_Pano_B8",        [f"{DISS}/papers/rasterlite/landsat_pano/LC08_192029_20130702_B8.TIF"]),
    ("Fuieri_2014_srtm",                     [f"{DISS}/papers/rasterlite/SRTM_Italy/srtm_italy_exact/srtm_tiles/SRTM_Italy_final.tif"]),
    ("Fuieri_2019_ETOPO1",                   [f"{DISS}/papers/rasterlite/ETOPO1/ETOPO1_Ice_g_geotiff.tif"]),
    ("Zalipynis_2018_0_Landsat8_B4_B5_Mosaic",[f"{DISS}/papers/zalipynis/2018/landsat8_mosaic_B4.tif",
                                               f"{DISS}/papers/zalipynis/2018/landsat8_mosaic_B5.tif"]),
    ("Zalipynis_2018_1_Landsat8_B1",         [f"{DISS}/papers/zalipynis/2018_2/landsat/landsat8_path190_row031_stack.tif"]),
    ("Zalipynis_2019_Landsat8_B4_Mosaic",    [f"{DISS}/papers/zalipynis/2019_landsat_band4_mosaic/exact/landsat8_mosaic_B4.tif"]),
    ("other_sentinel2_B2",                   [f"{DISS}/others/sentinel2/Sentinel2_B02_*.tif"]),
    ("other_sentinel2_B3",                   [f"{DISS}/others/sentinel2/Sentinel2_B03_*.tif"]),
    ("other_sentinel2_B4",                   [f"{DISS}/others/sentinel2/Sentinel2_B04_*.tif"]),
    ("other_sentinel2_B8",                   [f"{DISS}/others/sentinel2/Sentinel2_B08_*.tif"]),
    ("Zaytar_2025_Sentinel2_B2",             [f"{DISS}/papers/zaytar/2025/final/split_bands/Sentinel_*_B2.tif"]),
    ("Zaytar_2025_Sentinel2_B3",             [f"{DISS}/papers/zaytar/2025/final/split_bands/Sentinel_*_B3.tif"]),
    ("Zaytar_2025_Sentinel2_B4",             [f"{DISS}/papers/zaytar/2025/final/split_bands/Sentinel_*_B4.tif"]),
    ("Zaytar_2025_Sentinel2_B8",             [f"{DISS}/papers/zaytar/2025/final/split_bands/Sentinel_*_B8.tif"]),
    ("other_landsat8_B1",                    [f"{DISS}/others/landsat8/Landsat8_B1_*.tif"]),
    ("other_landsat8_B2",                    [f"{DISS}/others/landsat8/Landsat8_B2_*.tif"]),
    ("other_landsat8_B3",                    [f"{DISS}/others/landsat8/Landsat8_B3_*.tif"]),
    ("other_landsat8_B4",                    [f"{DISS}/others/landsat8/Landsat8_B4_*.tif"]),
    ("other_landsat8_B5",                    [f"{DISS}/others/landsat8/Landsat8_B5_*.tif"]),
    ("other_landsat8_B8",                    [f"{DISS}/others/landsat8/Landsat8_B8_*.tif"]),
    ("other_srtm_highres",                   [f"{DISS}/others/srtm_highres/*.tif"]),
    ("other_worldclim",                      [f"{DISS}/others/worldclim/*.tif"]),
    ("other_worldcover_int16",               [f"{DISS}/others/worldcover_int16/*.tif"]),
    ("other_etopo1",                         [f"{DISS}/others/etopo1/*.tif"]),
    ("other_srtm",                           [f"{DISS}/others/srtm/*.tif"]),
    ("srtm_45_15",                           [f"{DISS}/srtm_45_15.tif"]),
    ("JRC_TMF",                              [f"{DISS}/JRC_TMF_AnnualChange_v1_1990_AFR_ID16_S10_E10.tif"]),
    ("slope_srtm",                           [f"{DISS}/slope-srtm_35_11.tif"]),
]

coll = next((globs for name, globs in COLLECTIONS if name == collection_name), None)
if coll is None:
    print(f"Collection '{collection_name}' not found.", file=sys.stderr)
    sys.exit(1)

tifs = []
for p in coll:
    matched = sorted(glob_module.glob(p))
    tifs.extend(matched if matched else [p])

stds = []
for tif in tifs:
    entry = index.get(tif, {})
    band = entry.get("1", {})
    std = band.get("std")
    print(f"  {f'{std:.1f}' if std is not None else 'N/A':>10}  {tif.split('/')[-1]}")
    if std is not None:
        stds.append(std)

print()
if stds:
    print(f"  {'mean std':>10}  {sum(stds)/len(stds):.1f}  (over {len(stds)} TIFs)")
else:
    print("  No std values found.")
