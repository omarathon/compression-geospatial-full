#!/usr/bin/env bash
set -u
BENCH=./build/bench_comp

printf "%-55s  %10s  %10s\n" "file" "c0_cfmean" "c3_cfmean"
printf "%-55s  %10s  %10s\n" "----" "---------" "---------"

for tif in /maps/omsst2/diss/others/worldclim/*.tif; do
    out=$("$BENCH" "$tif" -b 256 -n 500 \
        --ordering default --max-nodata-pct 101 \
        --check-roundtrip --normalize 2>/dev/null)

    c0_cf=$(echo "$out" | awk -F'[,:]' '$2=="0" {print $6}')
    c3_cf=$(echo "$out" | awk -F'[,:]' '$2=="3" {print $6}')

    printf "%-55s  %10s  %10s\n" "$(basename "$tif")" "$c0_cf" "$c3_cf"
done
