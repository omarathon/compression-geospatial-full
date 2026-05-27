#!/usr/bin/env bash
# Ablation sweep C0..C4 for simdcomp_fused_delta_carry.
# Usage: ./ablation_bench_carry.sh [-n NUM_BLOCKS] [-o OUTDIR]
set -e

N=8000
OUTDIR=.

while [[ $# -gt 0 ]]; do
    case $1 in
        -n) N=$2; shift 2 ;;
        -o) OUTDIR=$2; shift 2 ;;
        *) echo "Unknown argument: $1"; exit 1 ;;
    esac
done

mkdir -p "$OUTDIR"

TIF=/maps/omsst2/diss/papers/zalipynis/2018_2/landsat/landsat8_path190_row031_stack.tif

run_level() {
    local name=$1
    local flags=$2
    echo ">>> $name (EXTRA_CFLAGS=$flags)"

    cd external/simdcomp
    make clean
    make EXTRA_CFLAGS="$flags"
    cd ../..

    cmake -B build -DCMAKE_BUILD_TYPE=Release
    cmake --build build --target bench_pipeline -j32

    ./build/bench_pipeline "$TIF" -b 256 -n "$N" -r 10 \
        --icodec "simdcomp_fused_delta_carry" \
        --acodec "simdcomp_fused_delta_carry" \
        --ordering default --itrans none --pattern linear \
        --atrans linearSumFused --normalize > "$OUTDIR/ablation_${name}.txt" 2>&1
}

run_others() {
     cd external/simdcomp
     make clean
     make
     cd ../..

     cmake -B build -DCMAKE_BUILD_TYPE=Release
     cmake --build build --target bench_pipeline -j32

     # raw
    ./build/bench_pipeline "$TIF" -b 256 -n "$N" -r 10 \
        --icodec "custom_direct_access" \
        --acodec "custom_direct_access" \
        --ordering default --itrans none --pattern linear \
        --atrans linearSumSimd --normalize > "$OUTDIR/raw.txt" 2>&1

     # no delta
    ./build/bench_pipeline "$TIF" -b 256 -n "$N" -r 10 \
        --icodec "simdcomp_fused" \
        --acodec "simdcomp_fused" \
        --ordering default --itrans none --pattern linear \
        --atrans linearSumFused --normalize > "$OUTDIR/no_delta.txt" 2>&1

    # global for
    ./build/bench_pipeline "$TIF" -b 256 -n "$N" -r 10 \
        --icodec "simdcomp_fused_for_global" \
        --acodec "simdcomp_fused_for_global" \
        --ordering default --itrans none --pattern linear \
        --atrans linearSumFused --normalize > "$OUTDIR/for_global.txt" 2>&1
}

run_others

run_level C0 "-DABLATE_ZIGZAG_CARRY -DABLATE_PREFIXSUM_CARRY -DABLATE_CARRY_ADD -DABLATE_BROADCAST_LANE15"
run_level C1 "-DABLATE_PREFIXSUM_CARRY -DABLATE_CARRY_ADD -DABLATE_BROADCAST_LANE15"
run_level C2 "-DABLATE_CARRY_ADD -DABLATE_BROADCAST_LANE15"
run_level C3 "-DABLATE_BROADCAST_LANE15"
run_level C4 ""

echo "done. results in $OUTDIR/ablation_C{0..4}.txt, $OUTDIR/no_delta.txt, $OUTDIR/raw.txt"