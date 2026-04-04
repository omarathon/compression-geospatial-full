#!/usr/bin/env bash
set -u

BENCH="/home/omsst2/diss/compression-geospatial-full/build/bench_comp"
OUTDIR="/scratch/omsst2/diss/comp_bench_2apr26"

mkdir -p "$OUTDIR"
shopt -s nullglob

NODATA_VALUES=(0 20 50 80 101)

run_tif() {
    local exp_name="$1"
    local tif="$2"
    local tif_idx="$3"
    local tif_log="$4"

    local nodata
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

    for nodata in "${NODATA_VALUES[@]}"; do
        {
            echo ">>> RUN START: tif=$tif | max-nodata-pct=$nodata | normalize=no"
            echo ">>> CMD: $BENCH \"$tif\" -b 256 -n 4000 --ordering default morton --max-nodata-pct $nodata --check-roundtrip"
        } >> "$tif_log"

        stdbuf -oL -eL \
            "$BENCH" "$tif" \
            -b 256 \
            -n 4000 \
            --ordering default morton \
            --max-nodata-pct "$nodata" \
            --check-roundtrip \
            >> "$tif_log" 2>&1
        rc=$?

        echo ">>> RUN END: rc=$rc" >> "$tif_log"

        if ((rc != 0)); then
            {
                echo
                echo "ERROR: command failed for tif=$tif nodata=$nodata normalize=no"
                echo "TIFF worker aborted at $(date -Is)"
            } >> "$tif_log"
            return "$rc"
        fi

        {
            echo
            echo ">>> RUN START: tif=$tif | max-nodata-pct=$nodata | normalize=yes"
            echo ">>> CMD: $BENCH \"$tif\" -b 256 -n 4000 --ordering default morton --max-nodata-pct $nodata --check-roundtrip --normalize"
        } >> "$tif_log"

        stdbuf -oL -eL \
            "$BENCH" "$tif" \
            -b 256 \
            -n 4000 \
            --ordering default morton \
            --max-nodata-pct "$nodata" \
            --check-roundtrip \
            --normalize \
            >> "$tif_log" 2>&1
        rc=$?

        echo ">>> RUN END: rc=$rc" >> "$tif_log"

        if ((rc != 0)); then
            {
                echo
                echo "ERROR: command failed for tif=$tif nodata=$nodata normalize=yes"
                echo "TIFF worker aborted at $(date -Is)"
            } >> "$tif_log"
            return "$rc"
        fi

        echo >> "$tif_log"
    done

    {
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
        {
            echo "============================================================"
            echo "Experiment: $exp_name"
            echo "Started: $(date -Is)"
            echo "ERROR: no TIFFs matched for experiment"
        } > "$final_out"
        return 1
    fi

    echo "Launching experiment: $exp_name (${#tif_paths[@]} TIFF workers)"

    local pids=()
    local tif
    local idx=0
    local tif_log

    for tif in "${tif_paths[@]}"; do
        idx=$((idx + 1))
        tif_log="$temp_dir/$(printf "%04d" "$idx").log"

        run_tif "$exp_name" "$tif" "$idx" "$tif_log" &
        pids+=($!)
    done

    local overall_rc=0
    local pid
    for pid in "${pids[@]}"; do
        if ! wait "$pid"; then
            overall_rc=1
        fi
    done

    {
        echo "============================================================"
        echo "Experiment: $exp_name"
        echo "Host: $(hostname)"
        echo "Started aggregation: $(date -Is)"
        echo "TIFF count: ${#tif_paths[@]}"
        echo "============================================================"
        echo
    } > "$final_out"

    local f
    for f in "$temp_dir"/*.log; do
        {
            echo
            echo "############################################################"
            echo "Per-TIFF log: $(basename "$f")"
            echo "############################################################"
            cat "$f"
        } >> "$final_out"
    done

    if ((overall_rc == 0)); then
        {
            echo
            echo "Finished: $(date -Is)"
            echo '$$$$$$$$FINISHED$$$$$$$$'
        } >> "$final_out"
    else
        {
            echo
            echo "Experiment failed: $(date -Is)"
            echo "One or more TIFF workers failed."
        } >> "$final_out"
    fi

    return "$overall_rc"
}

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

run_experiment "Fuieri_2019_Tuscany_DTM" \
    /maps/omsst2/diss/papers/rasterlite/tuscany_dtm/quantized_2000.tif &
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

run_experiment "Zaytar_2025_Sentinel2_B2_B3_B4_B8" \
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

run_experiment "other_worldclim" \
    /maps/omsst2/diss/others/worldclim/*.tif &
pids+=($!)

run_experiment "other_worldcover_int16" \
    /maps/omsst2/diss/others/worldcover_int16/*.tif &
pids+=($!)

run_experiment "other_etopo1" \
    /maps/omsst2/diss/others/etopo1/*.tif &
pids+=($!)

run_experiment "other_etopo_highres_quant" \
    /maps/omsst2/diss/others/etopo_highres_quant/*.tif &
pids+=($!)

run_experiment "other_landscan" \
    /maps/omsst2/diss/others/landscan/*.tif &
pids+=($!)

run_experiment "other_srtm" \
    /maps/omsst2/diss/others/srtm/*.tif &
pids+=($!)

run_experiment "other_worldcover" \
    /maps/omsst2/diss/others/worldcover/*.tif &
pids+=($!)

overall_rc=0
for pid in "${pids[@]}"; do
    if ! wait "$pid"; then
        overall_rc=1
    fi
done

exit "$overall_rc"