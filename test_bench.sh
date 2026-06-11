# ./build/bench_comp ~/diss/geotiffs/JRC_TMF_AnnualChange_v1_1990_AFR_ID16_S10_E10.tif -b 256 -n 10 --ordering default morton --max-nodata-pct 10 --check-roundtrip --normalize

# ./build/bench_comp ~/diss/geotiffs/accessibility.tif -b 256 -n 10 --ordering default morton --max-nodata-pct 10 --check-roundtrip --normalize

# ./build/bench_comp ~/diss/geotiffs/slope-srtm_35_11.tif -b 256 -n 10 --ordering default morton --max-nodata-pct 10 --check-roundtrip --normalize

# ./build/bench_comp ~/diss/geotiffs/srtm_45_15.tif -b 256 -n 10 --ordering default morton --max-nodata-pct 50 --check-roundtrip --normalize

./build/bench_comp ~/diss/geotiffs/JRC_TMF_AnnualChange_v1_1990_AFR_ID16_S10_E10.tif -b 256 -n 100 --ordering default morton --max-nodata-pct 10 --check-roundtrip

./build/bench_comp ~/diss/geotiffs/accessibility.tif -b 256 -n 100 --ordering default morton --max-nodata-pct 10 --check-roundtrip

./build/bench_comp ~/diss/geotiffs/slope-srtm_35_11.tif -b 256 -n 100 --ordering default morton --max-nodata-pct 10 --check-roundtrip

./build/bench_comp ~/diss/geotiffs/srtm_45_15.tif -b 256 -n 100 --ordering default morton --max-nodata-pct 50 --check-roundtrip

# flags: --normalize true,false (just put --normalize, no need for true/false after)
#        --max-nodata-pct 0,20,50,101
#        --ordering default,morton
