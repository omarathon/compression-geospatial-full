#!/usr/bin/env bash
set -euo pipefail

command -v gsutil >/dev/null 2>&1 || {
  echo "Error: gsutil not found" >&2
  exit 1
}

declare -A DATE_BY_PATH=(
  [195]=20150713
  [196]=20150704
  [197]=20150711
  [198]=20150702
)

rows=(024 025 026 027)
paths=(195 196 197 198)

for path in "${paths[@]}"; do
  date="${DATE_BY_PATH[$path]}"

  for row in "${rows[@]}"; do
    prefix="gs://gcp-public-data-landsat/LC08/01/${path}/${row}/"

    echo "Searching ${path}/${row} for acquisition date ${date} ..."

    mapfile -t matches < <(
      gsutil ls "${prefix}**_B4.TIF" 2>/dev/null \
      | grep "${path}${row}_${date}_" \
      | grep -v '\$folder\$' \
      || true
    )

    if [ "${#matches[@]}" -eq 0 ]; then
      echo "WARNING: no B4 object found for path=${path} row=${row} date=${date}" >&2
      continue
    fi

    chosen=""
    for level in L1TP L1GT L1GS; do
      for m in "${matches[@]}"; do
        base="$(basename "$m")"
        if [[ "$base" == LC08_${level}_${path}${row}_${date}_*_B4.TIF ]]; then
          chosen="$m"
          break 2
        fi
      done
    done

    if [ -z "$chosen" ]; then
      chosen="${matches[0]}"
    fi

    echo "Copying $chosen"
    gsutil cp "$chosen" .
  done
done