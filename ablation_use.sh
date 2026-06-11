#!/usr/bin/env bash
# Swap the active simdcomp static library to one of the ablation variants
# produced by ablation_build.sh, then rebuild bench_pipeline.
#
# After this script finishes, run your bench against ./build/bench_pipeline.
#
# Usage:
#   ./ablation_use.sh <L0|L1|L2|C0|C1|C2|C3|C4|full>
#   ./ablation_use.sh L0 --no-rebuild   # just swap; don't run cmake --build

set -euo pipefail

if [[ $# -lt 1 ]]; then
    echo "Usage: $0 <L0|L1|L2|C0|C1|C2|C3|C4|full> [--no-rebuild]" >&2
    exit 1
fi

variant="$1"
rebuild=1
if [[ "${2:-}" == "--no-rebuild" ]]; then
    rebuild=0
fi

PROJ_ROOT="$(cd "$(dirname "$0")" && pwd)"
SIMDCOMP="$PROJ_ROOT/external/simdcomp"
VARIANT_LIB="$SIMDCOMP/libsimdcomp_${variant}.a"
ACTIVE_LIB="$SIMDCOMP/libsimdcomp.a"

if [[ ! -e "$VARIANT_LIB" ]]; then
    echo "ERROR: variant library $VARIANT_LIB not found." >&2
    echo "Run ./ablation_build.sh first to produce all variants." >&2
    exit 1
fi

# Resolve symlinks so cp gets the real file (L2 and C4 are symlinks to full).
real_lib="$(readlink -f "$VARIANT_LIB")"
cp -f "$real_lib" "$ACTIVE_LIB"
# Touch so cmake sees the change and re-links.
touch "$ACTIVE_LIB"

echo "Active simdcomp ← $variant ($(basename "$real_lib"))"

if [[ $rebuild -eq 1 ]]; then
    if [[ ! -d "$PROJ_ROOT/build" ]]; then
        echo "Note: $PROJ_ROOT/build does not exist; run cmake to configure first." >&2
        exit 0
    fi
    echo
    echo "Rebuilding bench_pipeline …"
    cmake --build "$PROJ_ROOT/build" --target bench_pipeline
    echo
    echo "Run your bench against ./build/bench_pipeline (variant = $variant)."
fi
