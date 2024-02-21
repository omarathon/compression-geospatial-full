#!/bin/bash

codec_names=("FastPFor_JustCopy")
for codec in "${codec_names[@]}"; do
    ./random_access_comp /home/omar/diss/geotiffs/2656.tif 256 400 10000 $codec
    # ./random_access_comp /home/omar/diss/geotiffs/2656.tif 256 400 10000 [+]_custom_delta_vecavx+$codec
    # ./random_access_comp /home/omar/diss/geotiffs/2656.tif 256 400 10000 [+]_custom_delta_vecavx512+$codec
    # ./random_access_comp /home/omar/diss/geotiffs/2656.tif 256 400 10000 [+]_custom_for_vecavx512+$codec
    # ./random_access_comp /home/omar/diss/geotiffs/2656.tif 256 400 10000 [+]_custom_rle_vecavx+$codec
done

# codec_names=("FastPFor_JustCopy" "DEFLATE" "FastPFor_SIMDFastPFor128+VariableByte" "FastPFor_SIMDFastPFor256+VariableByte" "FastPFor_SIMDBinaryPacking+VariableByte" "FastPFor_SIMDGroupSimple_RingBuf+VariableByte" "FastPFor_SIMDNewPFor<4,Simple16>+VariableByte" "FastPFor_SIMDGroupSimple+VariableByte" "FastPFor_FastBinaryPacking16+VariableByte" "FastPFor_FastBinaryPacking32+VariableByte" "FastPFor_SIMDPFor+VariableByte" "2ibench_interpolativeblock" "FastPFor_Simple8b")
codec_names=("FastPFor_JustCopy")
for codec in "${codec_names[@]}"; do
    ./random_access_comp /home/omar/diss/geotiffs/accessibility.tif 256 400 10000 $codec
    # ./random_access_comp /home/omar/diss/geotiffs/accessibility.tif 256 400 10000 [+]_custom_delta_vecavx+$codec
    # ./random_access_comp /home/omar/diss/geotiffs/accessibility.tif 256 400 10000 [+]_custom_delta_vecavx512+$codec
    # ./random_access_comp /home/omar/diss/geotiffs/accessibility.tif 256 400 10000 [+]_custom_for_vecavx512+$codec
    # ./random_access_comp /home/omar/diss/geotiffs/accessibility.tif 256 400 10000 [+]_custom_rle_vecavx+$codec
done

# codec_names=("FastPFor_JustCopy" "DEFLATE" "custom_rle_vecavx" "FastPFor_SIMDBinaryPacking+VariableByte" "FastPFor_SIMDGroupSimple+VariableByte" "FastPFor_SIMDGroupSimple_RingBuf+VariableByte" "FastPFor_Simple8b")
codec_names=("FastPFor_JustCopy")
for codec in "${codec_names[@]}"; do
    ./random_access_comp /home/omar/diss/geotiffs/JRC_TMF_AnnualChange_v1_1990_AFR_ID16_S10_E10.tif 256 400 10000 $codec
    # ./random_access_comp /home/omar/diss/geotiffs/JRC_TMF_AnnualChange_v1_1990_AFR_ID16_S10_E10.tif 256 400 10000 [+]_custom_delta_vecavx+$codec
    # ./random_access_comp /home/omar/diss/geotiffs/JRC_TMF_AnnualChange_v1_1990_AFR_ID16_S10_E10.tif 256 400 10000 [+]_custom_delta_vecavx512+$codec
    # ./random_access_comp /home/omar/diss/geotiffs/JRC_TMF_AnnualChange_v1_1990_AFR_ID16_S10_E10.tif 256 400 10000 [+]_custom_for_vecavx512+$codec
    # ./random_access_comp /home/omar/diss/geotiffs/JRC_TMF_AnnualChange_v1_1990_AFR_ID16_S10_E10.tif 256 400 10000 [+]_custom_rle_vecavx+$codec
done

# codec_names=("FastPFor_JustCopy" "DEFLATE" "FastPFor_SIMDBinaryPacking+VariableByte" "FastPFor_SIMDPFor+VariableByte" "FastPFor_SIMDFastPFor256+VariableByte" "2ibench_qmxblock" "FastPFor_SIMDFastPFor128+VariableByte" "FastPFor_VarIntG8IU" "custom_dict_packing_unvec")
codec_names=("FastPFor_JustCopy")
for codec in "${codec_names[@]}"; do
    ./random_access_comp /home/omar/diss/geotiffs/slope-srtm_35_11.tif 256 400 10000 $codec
    # ./random_access_comp /home/omar/diss/geotiffs/slope-srtm_35_11.tif 256 400 10000 [+]_custom_delta_vecavx+$codec
    # ./random_access_comp /home/omar/diss/geotiffs/slope-srtm_35_11.tif 256 400 10000 [+]_custom_delta_vecavx512+$codec
    # ./random_access_comp /home/omar/diss/geotiffs/slope-srtm_35_11.tif 256 400 10000 [+]_custom_for_vecavx512+$codec
    # ./random_access_comp /home/omar/diss/geotiffs/slope-srtm_35_11.tif 256 400 10000 [+]_custom_rle_vecavx+$codec
done

# codec_names=("FastPFor_JustCopy" "DEFLATE" "FastPFor_SIMDFastPFor128+VariableByte" "FastPFor_FastBinaryPacking32+VariableByte" "FastPFor_SIMDBinaryPacking+VariableByte" "FastPFor_SIMDNewPFor<4,Simple16>+VariableByte" "2ibench_qmxblock_parallel" "FastPFor_SIMDGroupSimple+VariableByte" "FastPFor_SIMDFastPFor256+VariableByte" "FastPFor_SIMDGroupSimple_RingBuf+VariableByte" "FastPFor_VarIntG8IU")
codec_names=("FastPFor_JustCopy")
for codec in "${codec_names[@]}"; do
    ./random_access_comp /home/omar/diss/geotiffs/srtm_45_15.tif 256 400 10000 $codec
    # ./random_access_comp /home/omar/diss/geotiffs/srtm_45_15.tif 256 400 10000 [+]_custom_delta_vecavx+$codec
    # ./random_access_comp /home/omar/diss/geotiffs/srtm_45_15.tif 256 400 10000 [+]_custom_delta_vecavx512+$codec
    # ./random_access_comp /home/omar/diss/geotiffs/srtm_45_15.tif 256 400 10000 [+]_custom_for_vecavx512+$codec
    # ./random_access_comp /home/omar/diss/geotiffs/srtm_45_15.tif 256 400 10000 [+]_custom_rle_vecavx+$codec
done
