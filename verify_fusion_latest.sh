#!/bin/bash
# Verify fused codecs produce the same per-block TRACE sums as the unfused base.
# Usage: ./verify_fusion_latest.sh

set -u

TIFS=(
  /home/omar/diss/geotiffs/slope-srtm_35_11.tif
  /home/omar/diss/geotiffs/srtm_45_15.tif
  /home/omar/diss/geotiffs/JRC_TMF_AnnualChange_v1_1990_AFR_ID16_S10_E10.tif
  /home/omar/diss/geotiffs/2656.tif
)

COMMON_ARGS="-b 256 -n 500 -r 1 --itrans none --pattern linear --ordering default --trace-sums --normalize"

# tag         atrans          codec
# (tag is used for filenames + echo; codec is what gets passed as --icodec/--acodec)
RUNS=(
  "base       linearSum       custom_direct_access"
  # "simdcomp   linearSumFused  simdcomp_fused"
  # # "pfor_old         linearSumFused  FastPFor_fused_SIMDPFor+VariableByte"
  # "pfor_global_b    linearSumFused  FastPFor_fused_corrected_global_b_SIMDPFor+VariableByte"
  # "pfor_adaptive_b  linearSumFused  FastPFor_fused_corrected_adaptive_b_SIMDPFor+VariableByte"
  # # "simdcomp_dl linearSumFused simdcomp_fused_delta_local"
  # # "simdcomp_dc linearSumFused simdcomp_fused_delta_carry"
  # # "pfor_dl    linearSumFused  FastPFor_fused_corrected_delta_local_SIMDPFor+VariableByte"
  # # "pfor_dc    linearSumFused  FastPFor_fused_corrected_delta_carry_SIMDPFor+VariableByte"

  # # "simdcomp_fl linearSumFused simdcomp_fused_for_local"
  # "simdcomp_fg linearSumFused simdcomp_fused_for_global"
  # # "simdcomp_fh linearSumFused simdcomp_fused_for_hierarchical"

  # "pfor_fg_global_b   linearSumFused  FastPFor_fused_corrected_for_global_global_b"
  # "pfor_fg_adaptive_b linearSumFused  FastPFor_fused_corrected_for_global_adaptive_b"
  # "pfor_fg_adaptive_b_p32 linearSumFused  FastPFor_fused_corrected_for_global_adaptive_b_p32"
  # "pfor_fg_adaptive_b_p64 linearSumFused  FastPFor_fused_corrected_for_global_adaptive_b_p64"
  # "pfor_fg_adaptive_b_p128 linearSumFused  FastPFor_fused_corrected_for_global_adaptive_b_p128"

  # "simdcomp_fg_w128 linearSumFused simdcomp_fused_for_global_w128"
  # "simdcomp_fg_w64  linearSumFused simdcomp_fused_for_global_w64"
  # "simdcomp_fg_w32  linearSumFused simdcomp_fused_for_global_w32"

  # "pfor_fg_adaptive_b_w128 linearSumFused  FastPFor_fused_corrected_for_global_adaptive_b_w128"
  # "pfor_fg_adaptive_b_w64  linearSumFused  FastPFor_fused_corrected_for_global_adaptive_b_w64"
  # "pfor_fg_adaptive_b_w32  linearSumFused  FastPFor_fused_corrected_for_global_adaptive_b_w32"

  # "tpfor_pfor linearSumFused  TurboPFor_TurboPFor256"
  # "tpfor_pack linearSumFused  TurboPFor_TurboPack256"

  "TurboPFor_fused_128v16_sum   linearSumFused  TurboPFor_fused_128v16_sum"
  "TurboPFor_fused_256v16_sum   linearSumFused  TurboPFor_fused_256v16_sum"
)

run_one() {
  local tag=$1 atrans=$2 codec=$3
  echo ">>> $tag ($codec)"
  ./build/bench_pipeline "$TIF" $COMMON_ARGS \
    --icodec "$codec" --acodec "$codec" --atrans "$atrans" \
    > "out_${tag}.txt" 2>&1
  grep "^TRACE" "out_${tag}.txt" > "out_${tag}_trace.txt"
}

fail=0
for TIF in "${TIFS[@]}"; do
  echo
  echo "############################################################"
  echo "# TIF: $TIF"
  echo "############################################################"

  for entry in "${RUNS[@]}"; do
    # shellcheck disable=SC2086
    run_one $entry
  done

  echo
  echo "=== diffs vs base ($(basename "$TIF")) ==="
  for entry in "${RUNS[@]}"; do
    tag=${entry%% *}
    [ "$tag" = "base" ] && continue
    if diff -q "out_base_trace.txt" "out_${tag}_trace.txt" > /dev/null; then
      echo "  OK   $tag"
    else
      echo "  FAIL $tag"
      diff "out_base_trace.txt" "out_${tag}_trace.txt" | head -20
      fail=1
    fi
  done
done

exit $fail
