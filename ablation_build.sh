#!/usr/bin/env bash
# Build the simdcomp ablation matrix for the delta-local / delta-carry pipelines.
#
# Each variant produces a separate `libsimdcomp_<name>.a` in external/simdcomp/.
# To benchmark a variant: run `./ablation_use.sh <name>` to swap it into place,
# then `cmake --build build --target bench_pipeline`, then run your bench.
#
# Levels (cumulative — each adds one transform on top of the previous):
#   L0  aggregate only                                            (no zigzag, no prefix_sum)
#   L1  zigzag → aggregate                                        (no prefix_sum)
#   L2  zigzag → prefix_sum → aggregate                           (= full LOCAL)
#   C0  aggregate only                                            (carry pipeline, all off)
#   C1  zigzag → aggregate
#   C2  zigzag → prefix_sum → aggregate
#   C3  zigzag → prefix_sum → carry_add → aggregate               (no broadcast_lane15)
#   C4  full CARRY (zigzag → prefix_sum → carry_add → broadcast)  (= full CARRY)
#
# L2 == C4 ("full") is built once and aliased.
#
# Usage:
#   ./ablation_build.sh         # build all variants
#   ./ablation_build.sh -j N    # parallel-make with -jN inside each variant build

set -euo pipefail

PROJ_ROOT="$(cd "$(dirname "$0")" && pwd)"
SIMDCOMP="$PROJ_ROOT/external/simdcomp"
JOBS_FLAG="${1:--j1}"   # default single-threaded so make output stays readable
BASE_CFLAGS="-fPIC -std=c89 -O3 -march=native -Wall -Wextra -Wshadow"

if [[ ! -d "$SIMDCOMP" ]]; then
    echo "ERROR: $SIMDCOMP not found" >&2
    exit 1
fi

build_variant() {
    local name="$1"; shift
    local flags="$*"
    echo
    echo "=== variant ${name} (extra flags: ${flags:-<none>}) ==="
    (
        cd "$SIMDCOMP"
        make clean > /dev/null 2>&1 || true
        # Build only the static library + its objects to keep this fast.
        make $JOBS_FLAG CFLAGS="$BASE_CFLAGS $flags" libsimdcomp.a
        cp libsimdcomp.a "libsimdcomp_${name}.a"
    )
    echo "  → $SIMDCOMP/libsimdcomp_${name}.a"
}

# LOCAL pipeline levels (CARRY pipeline left at full — irrelevant for LOCAL codec)
build_variant L0 -DABLATE_ZIGZAG_LOCAL -DABLATE_PREFIXSUM_LOCAL
build_variant L1 -DABLATE_PREFIXSUM_LOCAL

# CARRY pipeline levels (LOCAL pipeline left at full — irrelevant for CARRY codec)
build_variant C0 -DABLATE_ZIGZAG_CARRY -DABLATE_PREFIXSUM_CARRY -DABLATE_CARRY_ADD -DABLATE_BROADCAST_LANE15
build_variant C1 -DABLATE_PREFIXSUM_CARRY -DABLATE_CARRY_ADD -DABLATE_BROADCAST_LANE15
build_variant C2 -DABLATE_CARRY_ADD -DABLATE_BROADCAST_LANE15
build_variant C3 -DABLATE_BROADCAST_LANE15

# Full build (no ablation) — serves as both L2 and C4
build_variant full

# Alias L2 and C4 to the full build so the use-script can address them by name.
ln -sf "libsimdcomp_full.a" "$SIMDCOMP/libsimdcomp_L2.a"
ln -sf "libsimdcomp_full.a" "$SIMDCOMP/libsimdcomp_C4.a"

# Restore the "full" library as the active libsimdcomp.a (default state).
cp "$SIMDCOMP/libsimdcomp_full.a" "$SIMDCOMP/libsimdcomp.a"

echo
echo "=== built variants ==="
ls -1 "$SIMDCOMP"/libsimdcomp_*.a
echo
echo "Active libsimdcomp.a is the FULL build."
echo "To switch: ./ablation_use.sh <L0|L1|L2|C0|C1|C2|C3|C4|full>"
