#!/usr/bin/env bash
# Ablation sweep C0..C4 for simdcomp_fused_delta_carry. Output → ablation_C{0..4}.txt.
set -e

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

    ./build/bench_pipeline "$TIF" -b 256 -n 500 -r 5 \
        --icodec "simdcomp_fused_delta_carry" \
        --acodec "simdcomp_fused_delta_carry" \
        --ordering default --itrans none --pattern linear \
        --atrans linearSumFused --normalize > "ablation_${name}.txt" 2>&1
}

run_level C0 "-DABLATE_ZIGZAG_CARRY -DABLATE_PREFIXSUM_CARRY -DABLATE_CARRY_ADD -DABLATE_BROADCAST_LANE15"
run_level C1 "-DABLATE_PREFIXSUM_CARRY -DABLATE_CARRY_ADD -DABLATE_BROADCAST_LANE15"
run_level C2 "-DABLATE_CARRY_ADD -DABLATE_BROADCAST_LANE15"
run_level C3 "-DABLATE_BROADCAST_LANE15"
run_level C4 ""

echo "done. results in ablation_C{0..4}.txt"
