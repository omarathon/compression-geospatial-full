#!/usr/bin/env bash
set -euo pipefail

bench() {
    ./bench_pipeline /maps/omsst2/diss/accessibility.tif 64 256000 6 "FastPFor_SIMDPFor+VariableByte" "FastPFor_SIMDPFor+VariableByte" "morton" "none" "linear" "linearSum"
    ./bench_pipeline /maps/omsst2/diss/accessibility.tif 64 64000 6 "FastPFor_SIMDPFor+VariableByte" "FastPFor_SIMDPFor+VariableByte" "morton" "none" "linear" "linearSum"

    ./bench_pipeline /maps/omsst2/diss/slope-srtm_35_11.tif 256 16000 6 "FastPFor_SIMDPFor+VariableByte" "FastPFor_SIMDPFor+VariableByte" "morton" "none" "linear" "linearSum"
    ./bench_pipeline /maps/omsst2/diss/slope-srtm_35_11.tif 256 4000 6 "FastPFor_SIMDPFor+VariableByte" "FastPFor_SIMDPFor+VariableByte" "morton" "none" "linear" "linearSum"

    # ./bench_pipeline /maps/omsst2/diss/accessibility.tif 64 100 1 "FastPFor_SIMDPFor+VariableByte" "FastPFor_SIMDPFor+VariableByte" "morton" "none" "linear" "linearSum"
}

rebuild_fastpfor() {
  local hash="$1"

  if [[ -z "$hash" ]]; then
    echo "Usage: rebuild_fastpfor <commit-hash>"
    return 1
  fi

  echo "=== Switching FastPFor to commit: $hash ==="

  pushd external/FastPFor > /dev/null

  git fetch --all
  git checkout "$hash"

  rm -rf build

  mkdir -p build
  cd build

  cmake ..
  cmake --build .

  popd > /dev/null

  echo "=== Rebuilding bench_pipeline ==="

  make clean
  make bench_pipeline

  echo "=== Done ==="
}

benchmark_fastpfor_commit() {
  local hash="$1"

  if [[ -z "$hash" ]]; then
    echo "Usage: benchmark_fastpfor_commit <commit-hash>"
    return 1
  fi

  mkdir -p logs
  local logfile="logs/bench_${hash}.log"

  {
    log_header "$hash"

    echo
    echo "=== Rebuilding FastPFor ==="
    rebuild_fastpfor "$hash"

    echo
    echo "=== Running benchmarks ==="
    bench

    echo
    echo "=== Finished benchmarks for $hash ==="
  } 2>&1 | tee "$logfile"
}

log_header() {
  local hash="$1"
  echo "=============================================="
  echo "FastPFor commit : $hash"
  echo "Date            : $(date -Is)"
  echo "=============================================="
}

# approaches:

# * fully unpack (current, but without writing the corrected exceptions to memory): `fuse_full_unpack` c8faa7ad6c56bc3dfe7d339d2e21dd2f89e80dc0
# * unpack with block-local buffer: `fuse_unpack_local` 6cf18972edaec7905e99889db886618a8dea88ab
# * state-machine for decoding exceptions on the fly / no extra pass: `fuse_state_machine`
#   * unpack all 4 if have gap: branch `fuse_state_machine` 5d7faa9b882948b45e377dd6e91e3cdea990ae89
#   * unpack specific index if have gap `fuse_state_machine` 83e4ba743767bc1182f7429c2f6c40a6d1a04bc0
#   * patch SIMD vector with the extract and replace method 074ef97a717e8b34f55136eac1a76cbd5932c7ec
#   * patch SIMD vector by spilling to extract, then replace 5565359d9fae4e9cba59a3a7d98194fdf328b806

# * don't store, unpack individual exception gaps in extra pass (no prefetch) ba78217df1fd55432ae4e1ca8958980e1bf13181
# * don't store, unpack individual exception gaps in extra pass (1 locality prefetch) a4865b0cb2bf74bd7e75b25cefd63d7f58978ca4
# * try: don't store, unpack individual exception gaps in extra pass (0 locality prefetch) 60b821f071e5de126127727f5a6b982b1336b8c1

# precompute exceptions for branchless correction 53ddc96fd61d9ed09bc5846f104f8146a0e92f2b

for hash in \
  53ddc96fd61d9ed09bc5846f104f8146a0e92f2b
do
  benchmark_fastpfor_commit "$hash"
done

# for hash in \
#   ba78217df1fd55432ae4e1ca8958980e1bf13181 \
#   a4865b0cb2bf74bd7e75b25cefd63d7f58978ca4 \
#   60b821f071e5de126127727f5a6b982b1336b8c1
# do
#   benchmark_fastpfor_commit "$hash"
# done

# for hash in \
#   c8faa7ad6c56bc3dfe7d339d2e21dd2f89e80dc0 \
#   6cf18972edaec7905e99889db886618a8dea88ab \
#   5d7faa9b882948b45e377dd6e91e3cdea990ae89 \
#   83e4ba743767bc1182f7429c2f6c40a6d1a04bc0 \
#   074ef97a717e8b34f55136eac1a76cbd5932c7ec \
#   5565359d9fae4e9cba59a3a7d98194fdf328b806
# do
#   benchmark_fastpfor_commit "$hash"
# done

