./bench_pipeline /home/omar/diss/geotiffs/srtm_45_15.tif 256 16000 3 "custom_direct_access" "custom_direct_access" "morton" "none" "linear" "linearSum"
# ./bench_pipeline /home/omar/diss/geotiffs/srtm_45_15.tif 256 24000 3 "custom_direct_access" "custom_direct_access" "morton" "none" "linear" "linearSum"
# ./bench_pipeline /home/omar/diss/geotiffs/srtm_45_15.tif 256 24000 3 "custom_direct_access" "custom_direct_access" "morton" "none" "linear" "linearSum"

./bench_pipeline /home/omar/diss/geotiffs/srtm_45_15.tif 256 16000 3 "simdcomp" "simdcomp" "morton" "none" "linear" "linearSum"
# ./bench_pipeline /home/omar/diss/geotiffs/srtm_45_15.tif 256 16000 3 "simdcomp" "simdcomp" "morton" "none" "linear" "linearSum"
# ./bench_pipeline /home/omar/diss/geotiffs/srtm_45_15.tif 256 16000 3 "simdcomp" "simdcomp" "morton" "none" "linear" "linearSum"

./bench_pipeline /home/omar/diss/geotiffs/JRC_TMF_AnnualChange_v1_1990_AFR_ID16_S10_E10.tif 256 16000 3 "custom_direct_access" "custom_direct_access" "morton" "none" "linear" "linearSum"
# ./bench_pipeline /home/omar/diss/geotiffs/JRC_TMF_AnnualChange_v1_1990_AFR_ID16_S10_E10.tif 256 16000 3 "custom_direct_access" "custom_direct_access" "morton" "none" "linear" "linearSum"
# ./bench_pipeline /home/omar/diss/geotiffs/JRC_TMF_AnnualChange_v1_1990_AFR_ID16_S10_E10.tif 256 16000 3 "custom_direct_access" "custom_direct_access" "morton" "none" "linear" "linearSum"

./bench_pipeline /home/omar/diss/geotiffs/JRC_TMF_AnnualChange_v1_1990_AFR_ID16_S10_E10.tif 256 16000 3 "simdcomp" "simdcomp" "morton" "none" "linear" "linearSum"
# ./bench_pipeline /home/omar/diss/geotiffs/JRC_TMF_AnnualChange_v1_1990_AFR_ID16_S10_E10.tif 256 16000 3 "simdcomp" "simdcomp" "morton" "none" "linear" "linearSum"
# ./bench_pipeline /home/omar/diss/geotiffs/JRC_TMF_AnnualChange_v1_1990_AFR_ID16_S10_E10.tif 256 16000 3 "simdcomp" "simdcomp" "morton" "none" "linear" "linearSum"


############


./bench_pipeline /home/omar/diss/geotiffs/accessibility.tif 64 256000 3 "custom_direct_access" "custom_direct_access" "morton" "none" "linear" "linearSum"
# ./bench_pipeline /home/omar/diss/geotiffs/accessibility.tif 64 64000 3 "custom_direct_access" "custom_direct_access" "morton" "none" "linear" "linearSum"
# ./bench_pipeline /home/omar/diss/geotiffs/accessibility.tif 64 64000 3 "custom_direct_access" "custom_direct_access" "morton" "none" "linear" "linearSum"

# ./bench_pipeline /home/omar/diss/geotiffs/accessibility.tif 64 64000 3 "FastPFor_SIMDPFor+VariableByte" "FastPFor_SIMDPFor+VariableByte" "morton" "none" "linear" "linearSum"
./bench_pipeline /home/omar/diss/geotiffs/accessibility.tif 64 256000 3 "simdcomp" "simdcomp" "morton" "none" "linear" "linearSum"
# ./bench_pipeline /home/omar/diss/geotiffs/accessibility.tif 64 64000 3 "FastPFor_SIMDPFor+VariableByte" "FastPFor_SIMDPFor+VariableByte" "morton" "none" "linear" "linearSum"
# ./bench_pipeline /home/omar/diss/geotiffs/accessibility.tif 64 64000 3 "FastPFor_SIMDPFor+VariableByte" "FastPFor_SIMDPFor+VariableByte" "morton" "none" "linear" "linearSum"

./bench_pipeline /home/omar/diss/geotiffs/slope-srtm_35_11.tif 256 16000 3 "custom_direct_access" "custom_direct_access" "morton" "none" "linear" "linearSum"
# ./bench_pipeline /home/omar/diss/geotiffs/slope-srtm_35_11.tif 256 16000 3 "custom_direct_access" "custom_direct_access" "morton" "none" "linear" "linearSum"
# ./bench_pipeline /home/omar/diss/geotiffs/slope-srtm_35_11.tif 256 16000 3 "custom_direct_access" "custom_direct_access" "morton" "none" "linear" "linearSum"

# ./bench_pipeline /home/omar/diss/geotiffs/slope-srtm_35_11.tif 256 4000 3 "FastPFor_SIMDPFor+VariableByte" "FastPFor_SIMDPFor+VariableByte" "morton" "none" "linear" "linearSum"
./bench_pipeline /home/omar/diss/geotiffs/slope-srtm_35_11.tif 256 16000 3 "simdcomp" "simdcomp" "morton" "none" "linear" "linearSum"
# ./bench_pipeline /home/omar/diss/geotiffs/slope-srtm_35_11.tif 256 16000 3 "FastPFor_SIMDPFor+VariableByte" "FastPFor_SIMDPFor+VariableByte" "morton" "none" "linear" "linearSum"
# ./bench_pipeline /home/omar/diss/geotiffs/slope-srtm_35_11.tif 256 16000 3 "FastPFor_SIMDPFor+VariableByte" "FastPFor_SIMDPFor+VariableByte" "morton" "none" "linear" "linearSum"