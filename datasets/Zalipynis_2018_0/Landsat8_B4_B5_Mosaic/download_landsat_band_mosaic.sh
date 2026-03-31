#!/usr/bin/env bash
set -euo pipefail

command -v gsutil >/dev/null 2>&1 || {
  echo "Error: gsutil not found" >&2
  exit 1
}

# Usage:
#   ./download_landsat_band_mosaic.sh B4
#   ./download_landsat_band_mosaic.sh B5
BAND="${1:-B5}"

rows=(024 025 026 027)
paths=(191 192 193 194 195 196 197 198)

DATE_MIN=20150701
DATE_MAX=20150715

for path in "${paths[@]}"; do
  for row in "${rows[@]}"; do
    prefix="gs://gcp-public-data-landsat/LC08/01/${path}/${row}/"

    echo "Searching ${path}/${row} for ${BAND} between ${DATE_MIN} and ${DATE_MAX} ..."

    mapfile -t matches < <(
      gsutil ls "${prefix}**_${BAND}.TIF" 2>/dev/null \
      | grep -v '\$folder\$' \
      | while read -r obj; do
          base="$(basename "$obj")"
          # base like:
          # LC08_L1TP_191024_20150701_20170407_01_T1_B4.TIF
          acq_date="$(printf '%s\n' "$base" | awk -F_ '{print $4}')"
          if [[ "$acq_date" =~ ^[0-9]{8}$ ]] && \
             [[ "$acq_date" -ge "$DATE_MIN" ]] && \
             [[ "$acq_date" -le "$DATE_MAX" ]]; then
            printf '%s\n' "$obj"
          fi
        done
    )

    if [ "${#matches[@]}" -eq 0 ]; then
      echo "WARNING: no ${BAND} object found for path=${path} row=${row} in date window" >&2
      continue
    fi

    chosen=""
    # Prefer L1TP, then L1GT, then L1GS
    for level in L1TP L1GT L1GS; do
      for m in "${matches[@]}"; do
        base="$(basename "$m")"
        if [[ "$base" == LC08_${level}_${path}${row}_20*_${BAND}.TIF ]]; then
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