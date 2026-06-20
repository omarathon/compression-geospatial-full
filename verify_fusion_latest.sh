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

COMMON_ARGS="-b 256 -n 500 -r 1 --rs 0 --itrans none --pattern linear --ordering default --trace-sums --normalize"

# tag         atrans          codec
# (tag is used for filenames + echo; codec is what gets passed as --icodec/--acodec)

WS=(4 8 16 32 128 256)

RUNS=(
  "base                              linearSumSimd  custom_direct_access"
  "simdcomp_fused                    linearSumFused simdcomp_fused"
  "TurboPFor_fused_256v16_merge_unpack linearSumFused TurboPFor_fused_256v16_merge_unpack"
  "TurboPFor_fused_256v16_byte_unpack  linearSumFused TurboPFor_fused_256v16_byte_unpack"
)
for W in "${WS[@]}"; do
  RUNS+=(
    "simdcomp_fused_for_256_w${W}_sep_shuf_unpack  linearSumFused simdcomp_fused_for_256_w${W}_sep_shuf_unpack"
    "simdcomp_fused_for_256_w${W}_sep_unpack       linearSumFused simdcomp_fused_for_256_w${W}_sep_unpack"
    "TurboPFor_fused_for_256_w${W}_sep_unpack      linearSumFused TurboPFor_fused_for_256_w${W}_sep_unpack"
    "TurboPFor_fused_for_256_w${W}_sep_nobc_unpack linearSumFused TurboPFor_fused_for_256_w${W}_sep_nobc_unpack"
  )
done

run_one() {
  local tag=$1 atrans=$2 codec=$3
  echo ">>> $tag ($codec)"
  ./build_byteexc/bench_pipeline "$TIF" $COMMON_ARGS \
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
