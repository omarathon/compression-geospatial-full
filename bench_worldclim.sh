#!/usr/bin/env bash
set -u
BENCH=./build/bench_comp

printf "%-55s  %8s  %8s  %8s  %8s\n" \
    "file" "c0_cf" "c0_bpi" "c3_cf" "c3_bpi"
printf "%-55s  %8s  %8s  %8s  %8s\n" \
    "----" "------" "------" "------" "------"

for tif in /maps/omsst2/diss/others/worldclim/*.tif; do
    out=$("$BENCH" "$tif" -b 256 -n 500 \
        --ordering default --max-nodata-pct 101 \
        --check-roundtrip --normalize 2>/dev/null)

    c0=$(echo "$out" | awk -F'[,:]' '$2=="0" {print $4, $8}')
    c3=$(echo "$out" | awk -F'[,:]' '$2=="3" {print $4, $8}')

    c0_cf=$(echo "$c0" | awk '{print $1}')
    c0_bpi=$(echo "$c0" | awk '{print $2}')
    c3_cf=$(echo "$c3" | awk '{print $1}')
    c3_bpi=$(echo "$c3" | awk '{print $2}')

    printf "%-55s  %8s  %8s  %8s  %8s\n" \
        "$(basename "$tif")" "$c0_cf" "$c0_bpi" "$c3_cf" "$c3_bpi"
done
