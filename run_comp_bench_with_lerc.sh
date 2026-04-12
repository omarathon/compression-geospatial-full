#!/usr/bin/env bash
set -u

SCRIPT="python3 /home/omsst2/diss/compression-geospatial-full/scripts/lerc_roundtrip_chunked_perband_fast_windowed.py"
BENCH="/home/omsst2/diss/compression-geospatial-full/build/bench_comp"

OUTDIR="/scratch/omsst2/diss/lerc_bench_pipeline"
mkdir -p "$OUTDIR"
shopt -s nullglob

Z_ERRORS=(1 2 4 8 16 32 64)
NODATA_VALUES=(0 101)

run_tif() {
    local exp_name="$1"
    local tif="$2"
    local tif_idx="$3"
    local tif_log="$4"

    local rc=0

    {
        echo "============================================================"
        echo "Experiment: $exp_name"
        echo "TIFF index: $tif_idx"
        echo "TIFF: $tif"
        echo "Host: $(hostname)"
        echo "Started: $(date -Is)"
        echo "============================================================"
        echo
    } > "$tif_log"

    local work_dir="$OUTDIR/${exp_name}_$(printf "%04d" "$tif_idx")"
    mkdir -p "$work_dir"

    local base
    base=$(basename "$tif")
    base="${base%.*}"

    local z
    for z in "${Z_ERRORS[@]}"; do

        local out_tif="$work_dir/${base}_Z${z}.tif"

        echo ">>> Z=$z : generating TIFF" >> "$tif_log"

        # --- Generate single Z version ---
        stdbuf -oL -eL \
            $SCRIPT "$tif" \
            --z-errors "$z" \
            --output-dir "$work_dir" \
            --json-name "stats_Z${z}.json" \
            >> "$tif_log" 2>&1

        rc=$?
        if ((rc != 0)); then
            echo "ERROR: LERC generation failed (Z=$z)" >> "$tif_log"
            return "$rc"
        fi

        # Expected filename from script
        out_tif="$work_dir/${base}_Denoised_MaxZ${z}.tif"

        # --- Benchmark this TIFF ---
        local nodata
        for nodata in "${NODATA_VALUES[@]}"; do
            {
                echo
                echo ">>> BENCH START: tif=$out_tif | Z=$z | nodata=$nodata | normalize=yes"
            } >> "$tif_log"

            stdbuf -oL -eL \
                "$BENCH" "$out_tif" \
                -b 256 \
                -n 4000 \
                --ordering default morton \
                --max-nodata-pct "$nodata" \
                --check-roundtrip \
                --normalize \
                >> "$tif_log" 2>&1

            rc=$?
            echo ">>> RUN END rc=$rc" >> "$tif_log"
            if ((rc != 0)); then return "$rc"; fi
        done

        echo ">>> Deleting $out_tif" >> "$tif_log"
        rm -f "$out_tif"

    done

    {
        echo
        echo "TIFF worker finished: $(date -Is)"
        echo '$$$$$$$$TIFF_FINISHED$$$$$$$$'
    } >> "$tif_log"
}


run_experiment() {
    local exp_name="$1"
    shift
    local final_out="$OUTDIR/${exp_name}.txt"
    local temp_dir="$OUTDIR/.tmp_${exp_name}"

    mkdir -p "$temp_dir"

    local tif_paths=("$@")
    if ((${#tif_paths[@]} == 0)); then
        echo "ERROR: no TIFFs matched" > "$final_out"
        return 1
    fi

    echo "Launching experiment: $exp_name (${#tif_paths[@]} TIFF workers)"

    local pids=()
    local idx=0
    local tif

    for tif in "${tif_paths[@]}"; do
        idx=$((idx + 1))
        local tif_log="$temp_dir/$(printf "%04d" "$idx").log"

        run_tif "$exp_name" "$tif" "$idx" "$tif_log" &
        pids+=($!)
    done

    local overall_rc=0
    for pid in "${pids[@]}"; do
        if ! wait "$pid"; then overall_rc=1; fi
    done

    cat "$temp_dir"/*.log > "$final_out"

    return "$overall_rc"
}

# =========================
# EXPERIMENTS (unchanged)
# =========================

pids=()

run_experiment "Fuieri_2014_Landsat8_Pano_B8" \
    /maps/omsst2/diss/papers/rasterlite/landsat_pano/LC08_192029_20130702_B8.TIF &
pids+=($!)

run_experiment "Fuieri_2014_srtm" \
    /maps/omsst2/diss/papers/rasterlite/SRTM_Italy/srtm_italy_exact/srtm_tiles/SRTM_Italy_final.tif &
pids+=($!)

run_experiment "Fuieri_2019_ETOPO1" \
    /maps/omsst2/diss/papers/rasterlite/ETOPO1/ETOPO1_Ice_g_geotiff.tif &
pids+=($!)

run_experiment "Zalipynis_2018_0_Landsat8_B4_B5_Mosaic" \
    /maps/omsst2/diss/papers/zalipynis/2018/landsat8_mosaic_B4.tif \
    /maps/omsst2/diss/papers/zalipynis/2018/landsat8_mosaic_B5.tif &
pids+=($!)

run_experiment "Zalipynis_2018_1_Landsat8_B1" \
    /maps/omsst2/diss/papers/zalipynis/2018_2/landsat/landsat8_path190_row031_stack.tif &
pids+=($!)

run_experiment "Zalipynis_2019_Landsat8_B4_Mosaic" \
    /maps/omsst2/diss/papers/zalipynis/2019_landsat_band4_mosaic/exact/landsat8_mosaic_B4.tif &
pids+=($!)

run_experiment "Zaytar_2025_Sentinel2" \
    /maps/omsst2/diss/papers/zaytar/2025/final/*.tif &
pids+=($!)

run_experiment "other_landsat8" \
    /maps/omsst2/diss/others/landsat8/*.tif &
pids+=($!)

run_experiment "other_sentinel2" \
    /maps/omsst2/diss/others/sentinel2/*.tif &
pids+=($!)

run_experiment "other_srtm_highres" \
    /maps/omsst2/diss/others/srtm_highres/*.tif &
pids+=($!)

run_experiment "other_etopo1" \
    /maps/omsst2/diss/others/etopo1/*.tif &
pids+=($!)

run_experiment "other_etopo_highres_quant" \
    /maps/omsst2/diss/others/etopo_highres_quant/*.tif &
pids+=($!)

run_experiment "other_srtm" \
    /maps/omsst2/diss/others/srtm/*.tif &
pids+=($!)

overall_rc=0
for pid in "${pids[@]}"; do
    if ! wait "$pid"; then overall_rc=1; fi
done

exit "$overall_rc"