#!/usr/bin/env bash
# check_nodata_bug.sh
# Scans all benchmark TIFs for the bench_comp UInt16 nodata bug:
#   triggered when dt=UInt16 AND nodata >= 32768 (cast to int16_t → negative)
# Prints AFFECTED / SAFE / SKIP (no nodata) for every band of every file.

shopt -s nullglob

TIFS=(
    /maps/omsst2/diss/papers/rasterlite/landsat_pano/LC08_192029_20130702_B8.TIF
    /maps/omsst2/diss/papers/rasterlite/SRTM_Italy/srtm_italy_exact/srtm_tiles/SRTM_Italy_final.tif
    /maps/omsst2/diss/papers/rasterlite/ETOPO1/ETOPO1_Ice_g_geotiff.tif
    /maps/omsst2/diss/papers/zalipynis/2018/landsat8_mosaic_B4.tif
    /maps/omsst2/diss/papers/zalipynis/2018/landsat8_mosaic_B5.tif
    /maps/omsst2/diss/papers/zalipynis/2018_2/landsat/landsat8_path190_row031_stack.tif
    /maps/omsst2/diss/papers/zalipynis/2019_landsat_band4_mosaic/exact/landsat8_mosaic_B4.tif
    /maps/omsst2/diss/papers/zaytar/2025/final/*.tif
    /maps/omsst2/diss/others/landsat8/*.tif
    /maps/omsst2/diss/others/sentinel2/*.tif
    /maps/omsst2/diss/others/srtm_highres/*.tif
    /maps/omsst2/diss/others/etopo1/*.tif
    /maps/omsst2/diss/others/etopo_highres_quant/*.tif
    /maps/omsst2/diss/others/srtm/*.tif
)

n_affected=0
n_safe=0
n_skip=0
n_missing=0

check_file() {
    local tif="$1"

    if [[ ! -f "$tif" ]]; then
        echo "MISSING  $tif"
        (( n_missing++ )) || true
        return
    fi

    # Parse gdalinfo once per file; extract per-band Type + NoData lines.
    # gdalinfo output groups bands as:
    #   Band N (...)
    #     Type=UInt16
    #     NoData Value=65535
    local info
    info=$(gdalinfo "$tif" 2>/dev/null) || { echo "GDALINFO_FAIL  $tif"; return; }

    # Walk bands: collect Type and NoData per band.
    local band_num=0
    local band_type=""
    local band_nodata=""
    local has_nodata=0

    while IFS= read -r line; do
        if [[ "$line" =~ ^Band[[:space:]]+([0-9]+) ]]; then
            # Flush previous band (if any)
            if (( band_num > 0 )); then
                _report_band "$tif" "$band_num" "$band_type" "$has_nodata" "$band_nodata"
            fi
            band_num="${BASH_REMATCH[1]}"
            band_type=""
            band_nodata=""
            has_nodata=0
        elif [[ "$line" =~ Type=([A-Za-z0-9]+) ]]; then
            band_type="${BASH_REMATCH[1]}"
        elif [[ "$line" =~ "NoData Value="([^[:space:]]+) ]]; then
            band_nodata="${BASH_REMATCH[1]}"
            has_nodata=1
        fi
    done <<< "$info"

    # Flush last band
    if (( band_num > 0 )); then
        _report_band "$tif" "$band_num" "$band_type" "$has_nodata" "$band_nodata"
    fi
}

_report_band() {
    local tif="$1" band="$2" dtype="$3" has_nodata="$4" nodata_str="$5"

    if [[ "$dtype" != "UInt16" ]]; then
        # Bug only applies to UInt16 path in bench_comp
        printf "SAFE     [band %s, %s] %s\n" "$band" "$dtype" "$tif"
        (( n_safe++ )) || true
        return
    fi

    if (( ! has_nodata )); then
        printf "SKIP     [band %s, UInt16, no-nodata] %s\n" "$band" "$tif"
        (( n_skip++ )) || true
        return
    fi

    # nodata_str may be a float like "65535" or "65535.0" or "-1"
    # Use awk for float→int conversion to avoid bash integer parse issues.
    local nodata_int
    nodata_int=$(awk "BEGIN { printf \"%d\", $nodata_str + 0 }" 2>/dev/null)

    if (( nodata_int >= 32768 )); then
        printf "AFFECTED [band %s, UInt16, nodata=%s (>=32768)] %s\n" \
               "$band" "$nodata_str" "$tif"
        (( n_affected++ )) || true
    else
        printf "SAFE     [band %s, UInt16, nodata=%s (<32768)] %s\n" \
               "$band" "$nodata_str" "$tif"
        (( n_safe++ )) || true
    fi
}

for tif in "${TIFS[@]}"; do
    check_file "$tif"
done

echo ""
echo "========================================"
echo "AFFECTED (bench_comp results invalid): $n_affected bands"
echo "SAFE (results trustworthy):            $n_safe bands"
echo "SKIP (no nodata, always safe):         $n_skip bands"
echo "MISSING files:                         $n_missing"
echo "========================================"
