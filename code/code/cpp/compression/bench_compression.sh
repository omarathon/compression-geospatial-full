source ./modules.sh

./bench_tiff_comp ../../../../tiffs/geotiffs/accessibility.tif 256 1 ',zigzag,morton' 'none' ',threshold,smoothAndShift,indexBasedClassification,valueBasedClassification,valueShift'
