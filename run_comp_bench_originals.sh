#!/usr/bin/env bash
set -u

BENCH="/home/omsst2/diss/compression-geospatial-full/build/bench_comp"
OUTDIR="/scratch/omsst2/diss/comp_bench_originals_4apr"

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

run_experiment "JRC_TMF" \
    /maps/omsst2/diss/JRC_TMF_AnnualChange_v1_1990_AFR_ID16_S10_E10.tif &
pids+=($!)

run_experiment "JRC_TMF_int16" \
    /maps/omsst2/diss/JRC_TMF_AnnualChange_v1_1990_AFR_ID16_S10_E10_int16.tif &
pids+=($!)

run_experiment "slope-srtm_35_11" \
    /maps/omsst2/diss/slope-srtm_35_11.tif &
pids+=($!)

run_experiment "srtm_45_15" \
    /maps/omsst2/diss/srtm_45_15.tif &
pids+=($!)

run_experiment "accessibility" \
    /maps/omsst2/diss/accessibility.tif &
pids+=($!)

overall_rc=0
for pid in "${pids[@]}"; do
    if ! wait "$pid"; then
        overall_rc=1
    fi
done

exit "$overall_rc"