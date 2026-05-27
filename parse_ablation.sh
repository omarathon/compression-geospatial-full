#!/usr/bin/env bash
# Usage: ./parse_ablation.sh <dir>
DIR=${1:-.}

for f in "$DIR"/*.txt; do
    name=$(basename "$f" .txt)
    if [[ "$name" == "raw" ]]; then
        val=$(grep -o 'medtimetrans:[^,]*' "$f" | head -1 | cut -d: -f2)
    else
        val=$(grep -o 'medtimedec:[^,]*' "$f" | head -1 | cut -d: -f2)
    fi
    printf "%-30s %s\n" "$name" "$val"
done
