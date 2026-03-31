#!/usr/bin/env bash
set -euo pipefail

path=190
row=031

prefix="gs://gcp-public-data-landsat/LC08/01/${path}/${row}/"

echo "Listing scenes..."
mapfile -t scenes < <(gsutil ls "${prefix}" | grep -v '\$folder\$')

# pick first 9 scenes (you can refine by cloud cover if needed)
count=0
for scene in "${scenes[@]}"; do
  echo "Checking $scene"

  b1=$(gsutil ls "${scene}*_B1.TIF" 2>/dev/null || true)

  if [ -n "$b1" ]; then
    echo "Downloading $b1"
    gsutil cp "$b1" .

    count=$((count+1))
    if [ "$count" -eq 9 ]; then
      break
    fi
  fi
done