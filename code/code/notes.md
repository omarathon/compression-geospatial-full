/home/omar/diss/geotiffs/2656.tif

//wsl.localhost/Ubuntu/usr/lib/gcc/x86_64-linux-gnu/11/include
//wsl.localhost/Ubuntu/usr/local/include
//wsl.localhost/Ubuntu/usr/include/x86_64-linux-gnu
//wsl.localhost/Ubuntu/usr/include

export PATH="/home/omar/diss/software/WSL2-Linux-Kernel/tools/perf:$PATH"



for each file with a different data type:
	vary size of decompressing window for:
		(SIMD, random reads)
		(SIMD, linear sweep reads)
		(non-SIMD, random reads)
		(non-SIMD, random reads)
	random reads no compression
	linear sweep reads no compression


variables:
vary data type
vary size of decompressing window
vary SIMD vs non-SIMD
vary random reads vs linear sweep
vary compression algorithm

1000x1000 int16 raster:

	500 linear sweeps, int16 datatype
		no compression: 1.0892 +- 0.0153
		with compression, block size 48, non-simd: 1.02066 +- 0.00894 seconds (compression factor ~0.35)
		with compression, block size 48, simd: 0.89670 +- 0.00549 seconds

	500 random sweeps, int16 datatype
		no compression: 20.30 +- 2.17 seconds
		with compression, block size 48, non-simd: 9.288 +- 0.971 seconds (compression factor ~0.35)
		with compression, block size 48, simd: 7.260 +- 0.284 seconds

srtm_45_15.tif
6000x6000 int16 raster:
	50 linear sweeps, int16 datatype
		no compression: 5.195 +- 0.284 seconds
		with compression, block size 48, non-simd: 6.161 +- 0.178 seconds
		with compression, block size 48, simd: 5.213 +- 0.106 seconds
	50 random sweeps, int16 datatype
		no compression: 84.01 +- 2.30 seconds
		with compression, block size 48, non-simd: 47.244 +- 0.730 seconds
		with compression, block size 48, simd: 40.89 +- 1.76 seconds

    NOTE: compressed data is of size 6940540 bytes (~7MB) which fits into our 10MB cache.

    OBSERVATION: compressing the entire raster results in a much better compression factor than compressing a subset of the raster. likely because patterns are present across the raster as a whole.

ideas:
- current transformation is read-only. adapt for an actual transformation (re-mapping the data). probably need to recompress for this.
- add test that the operation on the compressed data is the same as on the uncompressed data
- explore other compression algorithms
- implement on compute cluster so can run experiments on more reliable machine, and longer experiments
- calibration step at start which determines how big of a subset of the raster to operate over (need to ensure the compressed form can fit in cache), and the block size of the windowed decompression (e.g. size of L1 cache, vary this and see which is best)

srtm_45_15.tif
linear sweep write:
    no compression: 7.114 +- 0.448 seconds
    compression (48k window): 14.50 +- 1.38 seconds
    compression simd (48k window): 12.951 +- 0.493
random sweep write:
    no compression: 106.33 +- 4.24 seconds
    compression non-simd (48k window):
    compression simd (48k window): 50.766 +- 0.453 seconds



JRC_TMF_AnnualChange_v1_1990_AFR_ID16_S10_E10.tif
linear sweep read:
    no compression: 44.85 +- 2.05 seconds
    compression (no simd, 128k window): 44.258 +- 0.514 seconds
linear sweep write: (3 reps in program & 10 in perf)
    no compression: 15.637 +- 0.393 seconds
    compression (no simd, 128k window): 31.57 +- 1.10 seconds
random sweep read:
    no compression:
    compression (no simd, 128k window):
random sweep write:
    no compression: 272.92 +- 16.08 seconds
    compression (no simd, 128k window): 99.76 +- 4.65 seconds




slope-srtm_35_11.tif, 10 reps in program & in perf:
    linear sweep read:
        no compression: 1.1105 +- 0.0145 seconds
        compression (no simd, 64k window): 1.8576 +- 0.0692 seconds
        compression (simd, 64k window): 1.5155 +- 0.0459 seconds
    random sweep read:
        no compression:
        compression (no simd, 64k window):
        compression (simd, 64k window):
    linear sweep write:
        no compression: 2.115 +- 0.154 seconds
        compression (no simd, 64k window): 4.076 +- 0.180 seconds
        compression (simd, 64k window): 3.3056 +- 0.0799 seconds
    random sweep write:
        no compression: 22.807 +- 0.501 seconds
        compression (no simd, 64k window):
        compression (simd, 64k window): 12.235 +- 0.539 seconds