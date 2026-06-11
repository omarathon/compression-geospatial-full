#!/bin/bash

BENCH=/home/omsst2/diss/compression-geospatial-full/build/bench_comp
OUTDIR=/scratch/omsst2/diss/comp_bench_2apr26

mkdir -p "$OUTDIR"

NODATA_PCTS=(0 20 50 80 101)

# Run one experiment in a subshell (called as a background process).
# Args: experiment_name tif1 tif2 ...
# Globs must already be expanded by the caller.
run_experiment() {
    local name=$1
    shift
    local outfile="$OUTDIR/${name}.txt"

    (
        for tif in "$@"; do
            for pct in "${NODATA_PCTS[@]}"; do
                "$BENCH" "$tif" -b 256 -n 4000 \
                    --ordering default morton \
                    --max-nodata-pct "$pct" \
                    --check-roundtrip
                "$BENCH" "$tif" -b 256 -n 4000 \
                    --ordering default morton \
                    --max-nodata-pct "$pct" \
                    --check-roundtrip --normalize
            done
        done
        echo '$$$$$$$FINISHED$$$$$$$'
    ) > "$outfile" 2>&1 &
}

# ── Experiments ───────────────────────────────────────────────────────────────

run_experiment "Fuieri_2014_Landsat8_Pano_B8" \
    /maps/omsst2/diss/papers/rasterlite/landsat_pano/LC08_192029_20130702_B8.TIF

run_experiment "Fuieri_2014_srtm" \
    /maps/omsst2/diss/papers/rasterlite/SRTM_Italy/srtm_italy_exact/srtm_tiles/SRTM_Italy_final.tif

run_experiment "Fuieri_2019_ETOPO1" \
    /maps/omsst2/diss/papers/rasterlite/ETOPO1/ETOPO1_Ice_g_geotiff.tif

run_experiment "Fuieri_2019_Tuscany_DTM" \
    /maps/omsst2/diss/papers/rasterlite/tuscany_dtm/quantized_2000.tif

run_experiment "Zalipynis_2018_0_Landsat8_B4_B5_Mosaic" \
    /maps/omsst2/diss/papers/zalipynis/2018/landsat8_mosaic_B4.tif \
    /maps/omsst2/diss/papers/zalipynis/2018/landsat8_mosaic_B5.tif

run_experiment "Zalipynis_2018_1_Landsat8_B1" \
    /maps/omsst2/diss/papers/zalipynis/2018_2/landsat/landsat8_path190_row031_stack.tif

run_experiment "Zalipynis_2019_Landsat8_B4_Mosaic" \
    /maps/omsst2/diss/papers/zalipynis/2019_landsat_band4_mosaic/exact/landsat8_mosaic_B4.tif

run_experiment "Zaytar_2025_Sentinel2_B2_B3_B4_B8" \
    /maps/omsst2/diss/papers/zaytar/2025/final/*.tif

run_experiment "other_landsat8" \
    /maps/omsst2/diss/others/landsat8/*.tif

run_experiment "other_sentinel2" \
    /maps/omsst2/diss/others/sentinel2/*.tif

run_experiment "other_srtm_highres" \
    /maps/omsst2/diss/others/srtm_highres/*.tif

run_experiment "other_worldclim" \
    /maps/omsst2/diss/others/worldclim/*.tif

run_experiment "other_worldcover_int16" \
    /maps/omsst2/diss/others/worldcover_int16/*.tif

run_experiment "other_etopo1" \
    /maps/omsst2/diss/others/etopo1/*.tif

run_experiment "other_etopo_highres_quant" \
    /maps/omsst2/diss/others/etopo_highres_quant/*.tif

run_experiment "other_landscan" \
    /maps/omsst2/diss/others/landscan/*.tif

run_experiment "other_srtm" \
    /maps/omsst2/diss/others/srtm/*.tif

run_experiment "other_worldcover" \
    /maps/omsst2/diss/others/worldcover/*.tif

# ── Wait for all background jobs ──────────────────────────────────────────────
echo "All experiments launched. Waiting for completion..."
wait
echo "All experiments done."
