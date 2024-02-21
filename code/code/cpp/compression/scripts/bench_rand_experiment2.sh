#!/bin/bash

codec_names=("FastPFor_JustCopy" "Zstd_5" "Zstd_1" "TurboPFor_xor+TurboPack256" "FastPFor_SIMDBinaryPacking+VariableByte" "FastPFor_Simple8b" "FastPFor_SIMDGroupSimple+VariableByte")
for codec in "${codec_names[@]}"; do
    ./random_access_comp /home/omar/diss/geotiffs/2656.tif 256 400 10000 $codec
    ./random_access_comp /home/omar/diss/geotiffs/2656.tif 256 400 10000 [+]_custom_delta_vecavx+$codec
    # ./random_access_comp /home/omar/diss/geotiffs/2656.tif 256 400 10000 [+]_custom_delta_vecavx512+$codec
    # ./random_access_comp /home/omar/diss/geotiffs/2656.tif 256 400 10000 [+]_custom_for_vecavx512+$codec
    # ./random_access_comp /home/omar/diss/geotiffs/2656.tif 256 400 10000 [+]_custom_rle_vecavx+$codec
done

# codec_names=("FastPFor_JustCopy" "DEFLATE" "FastPFor_SIMDFastPFor128+VariableByte" "FastPFor_SIMDFastPFor256+VariableByte" "FastPFor_SIMDBinaryPacking+VariableByte" "FastPFor_SIMDGroupSimple_RingBuf+VariableByte" "FastPFor_SIMDNewPFor<4,Simple16>+VariableByte" "FastPFor_SIMDGroupSimple+VariableByte" "FastPFor_FastBinaryPacking16+VariableByte" "FastPFor_FastBinaryPacking32+VariableByte" "FastPFor_SIMDPFor+VariableByte" "2ibench_interpolativeblock" "FastPFor_Simple8b")
codec_names=("FastPFor_JustCopy" "Zstd_5" "TurboPFor_zzag/delta+TurboPFor128" "TurboPFor_Zigzag+TurboPFor256" "TurboPFor_xor+TurboPack256" "FastPFor_SIMDBinaryPacking+VariableByte" "FastPFor_SIMDFastPFor128+VariableByte")
for codec in "${codec_names[@]}"; do
    ./random_access_comp /home/omar/diss/geotiffs/accessibility.tif 256 400 10000 $codec
    ./random_access_comp /home/omar/diss/geotiffs/accessibility.tif 256 400 10000 [+]_custom_delta_vecavx+$codec
    # ./random_access_comp /home/omar/diss/geotiffs/accessibility.tif 256 400 10000 [+]_custom_delta_vecavx512+$codec
    # ./random_access_comp /home/omar/diss/geotiffs/accessibility.tif 256 400 10000 [+]_custom_for_vecavx512+$codec
    # ./random_access_comp /home/omar/diss/geotiffs/accessibility.tif 256 400 10000 [+]_custom_rle_vecavx+$codec
done

# codec_names=("FastPFor_JustCopy" "DEFLATE" "custom_rle_vecavx" "FastPFor_SIMDBinaryPacking+VariableByte" "FastPFor_SIMDGroupSimple+VariableByte" "FastPFor_SIMDGroupSimple_RingBuf+VariableByte" "FastPFor_Simple8b")
codec_names=("FastPFor_JustCopy" "Zstd_5" "Zstd_1" "Zstd_3" "Zstd_1" "LZ4" "TurboPFor_xor+TurboPack256" "TurboPFor_Zigzag+TurboPFor256" "FastPFor_SIMDBinaryPacking+VariableByte" "FastPFor_SIMDOPTPFor<4,Simple16>+VariableByte" "FastPFor_SIMDGroupSimple+VariableByte")
for codec in "${codec_names[@]}"; do
    ./random_access_comp /home/omar/diss/geotiffs/JRC_TMF_AnnualChange_v1_1990_AFR_ID16_S10_E10.tif 256 400 10000 $codec
    ./random_access_comp /home/omar/diss/geotiffs/JRC_TMF_AnnualChange_v1_1990_AFR_ID16_S10_E10.tif 256 400 10000 [+]_custom_delta_vecavx+$codec
    # ./random_access_comp /home/omar/diss/geotiffs/JRC_TMF_AnnualChange_v1_1990_AFR_ID16_S10_E10.tif 256 400 10000 [+]_custom_delta_vecavx512+$codec
    # ./random_access_comp /home/omar/diss/geotiffs/JRC_TMF_AnnualChange_v1_1990_AFR_ID16_S10_E10.tif 256 400 10000 [+]_custom_for_vecavx512+$codec
    # ./random_access_comp /home/omar/diss/geotiffs/JRC_TMF_AnnualChange_v1_1990_AFR_ID16_S10_E10.tif 256 400 10000 [+]_custom_rle_vecavx+$codec
done

# codec_names=("FastPFor_JustCopy" "DEFLATE" "FastPFor_SIMDBinaryPacking+VariableByte" "FastPFor_SIMDPFor+VariableByte" "FastPFor_SIMDFastPFor256+VariableByte" "2ibench_qmxblock" "FastPFor_SIMDFastPFor128+VariableByte" "FastPFor_VarIntG8IU" "custom_dict_packing_unvec")
codec_names=("FastPFor_JustCopy" "FastPFor_SIMDBinaryPacking+VariableByte" "TurboPFor_Zigzag+TurboVSimple" "TurboPFor_Zigzag+TurboPFor256" "TurboPFor_Zigzag+TurboPack256" "TurboPFor_xor+TurboPack256" "FastPFor_SIMDBinaryPacking+VariableByte")
for codec in "${codec_names[@]}"; do
    ./random_access_comp /home/omar/diss/geotiffs/slope-srtm_35_11.tif 256 400 10000 $codec
    ./random_access_comp /home/omar/diss/geotiffs/slope-srtm_35_11.tif 256 400 10000 [+]_custom_delta_vecavx+$codec
    # ./random_access_comp /home/omar/diss/geotiffs/slope-srtm_35_11.tif 256 400 10000 [+]_custom_delta_vecavx512+$codec
    # ./random_access_comp /home/omar/diss/geotiffs/slope-srtm_35_11.tif 256 400 10000 [+]_custom_for_vecavx512+$codec
    # ./random_access_comp /home/omar/diss/geotiffs/slope-srtm_35_11.tif 256 400 10000 [+]_custom_rle_vecavx+$codec
done

# codec_names=("FastPFor_JustCopy" "DEFLATE" "FastPFor_SIMDFastPFor128+VariableByte" "FastPFor_FastBinaryPacking32+VariableByte" "FastPFor_SIMDBinaryPacking+VariableByte" "FastPFor_SIMDNewPFor<4,Simple16>+VariableByte" "2ibench_qmxblock_parallel" "FastPFor_SIMDGroupSimple+VariableByte" "FastPFor_SIMDFastPFor256+VariableByte" "FastPFor_SIMDGroupSimple_RingBuf+VariableByte" "FastPFor_VarIntG8IU")
codec_names=("FastPFor_JustCopy" "FastPFor_SIMDFastPFor128+VariableByte" "FastPFor_Simple8b" "FastPFor_BP32+VariableByte" "TurboPFor_Zigzag+TurboPFor256" "TurboPFor_xor+TurboPack256" "FastPFor_SIMDBinaryPacking+VariableByte" "TurboPFor_Zigzag+TurboPack256" "FastPFor_SIMDFastPFor128+VariableByte" "FastPFor_SIMDOPTPFor<4,Simple16>+VariableByte")
for codec in "${codec_names[@]}"; do
    ./random_access_comp /home/omar/diss/geotiffs/srtm_45_15.tif 256 400 10000 $codec
    ./random_access_comp /home/omar/diss/geotiffs/srtm_45_15.tif 256 400 10000 [+]_custom_delta_vecavx+$codec
    # ./random_access_comp /home/omar/diss/geotiffs/srtm_45_15.tif 256 400 10000 [+]_custom_delta_vecavx512+$codec
    # ./random_access_comp /home/omar/diss/geotiffs/srtm_45_15.tif 256 400 10000 [+]_custom_for_vecavx512+$codec
    # ./random_access_comp /home/omar/diss/geotiffs/srtm_45_15.tif 256 400 10000 [+]_custom_rle_vecavx+$codec
done
