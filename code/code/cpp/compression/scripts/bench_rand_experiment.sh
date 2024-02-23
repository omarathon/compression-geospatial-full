#!/bin/bash

codec_names=("FastPFor_JustCopy" "custom_rle_vecavx" "FastPFor_SIMDBinaryPacking+VariableByte" "2ibench_interpolativeblock")
for codec in "${codec_names[@]}"; do
    ./random_access_comp /home/omar/diss/geotiffs/2656.tif 250 100000 1000000 $codec # 1 KB window, 100MB total uncompressed
    ./random_access_comp /home/omar/diss/geotiffs/2656.tif 1000 25000 1000000 $codec # 4 KB window, 100MB total uncompressed
    ./random_access_comp /home/omar/diss/geotiffs/2656.tif 10000 2500 100000 $codec # 40 KB window, 100MB total uncompressed
    ./random_access_comp /home/omar/diss/geotiffs/2656.tif 100000 250 100000 $codec # 400 KB window, 100MB total uncompressed
    ./random_access_comp /home/omar/diss/geotiffs/2656.tif 250000 100 10000 $codec # 1 MB window, 100MB total uncompressed
    ./random_access_comp /home/omar/diss/geotiffs/2656.tif 250000 500 10000 $codec # 1 MB window, 500MB total uncompressed
    ./random_access_comp /home/omar/diss/geotiffs/2656.tif 2500000 50 10000 $codec # 10 MB window, 500MB total uncompressed
done

# codec_names=("FastPFor_JustCopy" "DEFLATE" "FastPFor_SIMDFastPFor128+VariableByte" "FastPFor_SIMDFastPFor256+VariableByte" "FastPFor_SIMDBinaryPacking+VariableByte" "FastPFor_SIMDGroupSimple_RingBuf+VariableByte" "FastPFor_SIMDNewPFor<4,Simple16>+VariableByte" "FastPFor_SIMDGroupSimple+VariableByte" "FastPFor_FastBinaryPacking16+VariableByte" "FastPFor_FastBinaryPacking32+VariableByte" "FastPFor_SIMDPFor+VariableByte" "2ibench_interpolativeblock" "FastPFor_Simple8b")
codec_names=("FastPFor_JustCopy" "FastPFor_SIMDFastPFor128+VariableByte" "FastPFor_SIMDFastPFor256+VariableByte" "FastPFor_SIMDBinaryPacking+VariableByte" "FastPFor_SIMDGroupSimple_RingBuf+VariableByte" "FastPFor_SIMDNewPFor<4,Simple16>+VariableByte" "FastPFor_SIMDGroupSimple+VariableByte" "FastPFor_FastBinaryPacking16+VariableByte" "FastPFor_FastBinaryPacking32+VariableByte" "FastPFor_SIMDPFor+VariableByte" "2ibench_interpolativeblock" "FastPFor_Simple8b")
for codec in "${codec_names[@]}"; do
    ./random_access_comp /home/omar/diss/geotiffs/accessibility.tif 250 100000 1000000 $codec # 1 KB window, 100MB total uncompressed
    ./random_access_comp /home/omar/diss/geotiffs/accessibility.tif 1000 25000 1000000 $codec # 4 KB window, 100MB total uncompressed
    ./random_access_comp /home/omar/diss/geotiffs/accessibility.tif 10000 2500 100000 $codec # 40 KB window, 100MB total uncompressed
    ./random_access_comp /home/omar/diss/geotiffs/accessibility.tif 100000 250 100000 $codec # 400 KB window, 100MB total uncompressed
    ./random_access_comp /home/omar/diss/geotiffs/accessibility.tif 250000 100 10000 $codec # 1 MB window, 100MB total uncompressed
    ./random_access_comp /home/omar/diss/geotiffs/accessibility.tif 250000 500 10000 $codec # 1 MB window, 500MB total uncompressed
    ./random_access_comp /home/omar/diss/geotiffs/accessibility.tif 2500000 50 10000 $codec # 10 MB window, 500MB total uncompressed
done

# codec_names=("FastPFor_JustCopy" "DEFLATE" "custom_rle_vecavx" "FastPFor_SIMDBinaryPacking+VariableByte" "FastPFor_SIMDGroupSimple+VariableByte" "FastPFor_SIMDGroupSimple_RingBuf+VariableByte" "FastPFor_Simple8b")
codec_names=("FastPFor_JustCopy" "custom_rle_vecavx" "FastPFor_SIMDBinaryPacking+VariableByte" "FastPFor_SIMDGroupSimple+VariableByte" "FastPFor_SIMDGroupSimple_RingBuf+VariableByte" "FastPFor_Simple8b")
for codec in "${codec_names[@]}"; do
    ./random_access_comp /home/omar/diss/geotiffs/JRC_TMF_AnnualChange_v1_1990_AFR_ID16_S10_E10.tif 250 100000 1000000 $codec # 1 KB window, 100MB total uncompressed
    ./random_access_comp /home/omar/diss/geotiffs/JRC_TMF_AnnualChange_v1_1990_AFR_ID16_S10_E10.tif 1000 25000 1000000 $codec # 4 KB window, 100MB total uncompressed
    ./random_access_comp /home/omar/diss/geotiffs/JRC_TMF_AnnualChange_v1_1990_AFR_ID16_S10_E10.tif 10000 2500 100000 $codec # 40 KB window, 100MB total uncompressed
    ./random_access_comp /home/omar/diss/geotiffs/JRC_TMF_AnnualChange_v1_1990_AFR_ID16_S10_E10.tif 100000 250 100000 $codec # 400 KB window, 100MB total uncompressed
    ./random_access_comp /home/omar/diss/geotiffs/JRC_TMF_AnnualChange_v1_1990_AFR_ID16_S10_E10.tif 250000 100 10000 $codec # 1 MB window, 100MB total uncompressed
    ./random_access_comp /home/omar/diss/geotiffs/JRC_TMF_AnnualChange_v1_1990_AFR_ID16_S10_E10.tif 250000 500 10000 $codec # 1 MB window, 500MB total uncompressed
    ./random_access_comp /home/omar/diss/geotiffs/JRC_TMF_AnnualChange_v1_1990_AFR_ID16_S10_E10.tif 2500000 50 10000 $codec # 10 MB window, 500MB total uncompressed
done

# codec_names=("FastPFor_JustCopy" "DEFLATE" "FastPFor_SIMDBinaryPacking+VariableByte" "FastPFor_SIMDPFor+VariableByte" "FastPFor_SIMDFastPFor256+VariableByte" "2ibench_qmxblock" "FastPFor_SIMDFastPFor128+VariableByte" "FastPFor_VarIntG8IU" "custom_dict_packing_unvec")
codec_names=("FastPFor_JustCopy" "FastPFor_SIMDBinaryPacking+VariableByte" "FastPFor_SIMDPFor+VariableByte" "FastPFor_SIMDFastPFor256+VariableByte" "2ibench_qmxblock" "FastPFor_SIMDFastPFor128+VariableByte" "FastPFor_VarIntG8IU" "custom_dict_packing_unvec")
for codec in "${codec_names[@]}"; do
    ./random_access_comp /home/omar/diss/geotiffs/slope-srtm_35_11.tif 250 100000 1000000 $codec # 1 KB window, 100MB total uncompressed
    ./random_access_comp /home/omar/diss/geotiffs/slope-srtm_35_11.tif 1000 25000 1000000 $codec # 4 KB window, 100MB total uncompressed
    ./random_access_comp /home/omar/diss/geotiffs/slope-srtm_35_11.tif 10000 2500 100000 $codec # 40 KB window, 100MB total uncompressed
    ./random_access_comp /home/omar/diss/geotiffs/slope-srtm_35_11.tif 100000 250 100000 $codec # 400 KB window, 100MB total uncompressed
    ./random_access_comp /home/omar/diss/geotiffs/slope-srtm_35_11.tif 250000 100 10000 $codec # 1 MB window, 100MB total uncompressed
    ./random_access_comp /home/omar/diss/geotiffs/slope-srtm_35_11.tif 250000 500 10000 $codec # 1 MB window, 500MB total uncompressed
    ./random_access_comp /home/omar/diss/geotiffs/slope-srtm_35_11.tif 2500000 50 10000 $codec # 10 MB window, 500MB total uncompressed
done

# codec_names=("FastPFor_JustCopy" "DEFLATE" "FastPFor_SIMDFastPFor128+VariableByte" "FastPFor_FastBinaryPacking32+VariableByte" "FastPFor_SIMDBinaryPacking+VariableByte" "FastPFor_SIMDNewPFor<4,Simple16>+VariableByte" "2ibench_qmxblock_parallel" "FastPFor_SIMDGroupSimple+VariableByte" "FastPFor_SIMDFastPFor256+VariableByte" "FastPFor_SIMDGroupSimple_RingBuf+VariableByte" "FastPFor_VarIntG8IU")
codec_names=("FastPFor_JustCopy" "FastPFor_SIMDFastPFor128+VariableByte" "FastPFor_FastBinaryPacking32+VariableByte" "FastPFor_SIMDBinaryPacking+VariableByte" "FastPFor_SIMDNewPFor<4,Simple16>+VariableByte" "2ibench_qmxblock_parallel" "FastPFor_SIMDGroupSimple+VariableByte" "FastPFor_SIMDFastPFor256+VariableByte" "FastPFor_SIMDGroupSimple_RingBuf+VariableByte" "FastPFor_VarIntG8IU")
for codec in "${codec_names[@]}"; do
    ./random_access_comp /home/omar/diss/geotiffs/srtm_45_15.tif 250 100000 1000000 $codec # 1 KB window, 100MB total uncompressed
    ./random_access_comp /home/omar/diss/geotiffs/srtm_45_15.tif 1000 25000 1000000 $codec # 4 KB window, 100MB total uncompressed
    ./random_access_comp /home/omar/diss/geotiffs/srtm_45_15.tif 10000 2500 100000 $codec # 40 KB window, 100MB total uncompressed
    ./random_access_comp /home/omar/diss/geotiffs/srtm_45_15.tif 100000 250 100000 $codec # 400 KB window, 100MB total uncompressed
    ./random_access_comp /home/omar/diss/geotiffs/srtm_45_15.tif 250000 100 10000 $codec # 1 MB window, 100MB total uncompressed
    ./random_access_comp /home/omar/diss/geotiffs/srtm_45_15.tif 250000 500 10000 $codec # 1 MB window, 500MB total uncompressed
    ./random_access_comp /home/omar/diss/geotiffs/srtm_45_15.tif 2500000 50 10000 $codec # 10 MB window, 500MB total uncompressed
done