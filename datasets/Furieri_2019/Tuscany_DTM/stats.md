/maps/omsst2/diss/papers/rasterlite/tuscany_dtm/quantized_256.tif
/maps/omsst2/diss/papers/rasterlite/tuscany_dtm/quantized_2000.tif

quantization errors in quant_error_256.txt, quant_error_2000.txt:

quant_error_256.txt:
Absolute error (same units as DEM):
  min   = 0
  mean  = 0.00095915997933887
  max   = 0.001953125
  stddev= 0.00057344477200415
Percentage error (%):
  min   = 0
  mean  = 0.010959932487674
  max   = 100
  stddev= 0.60362736285031
  
quant_error_2000.txt:
Absolute error (same units as DEM):
  min   = 0
  mean  = 0.00011648761012538
  max   = 0.00028124999994361
  stddev= 7.566041100341e-05
Percentage error (%):
  min   = 0
  mean  = 0.0018591962535546
  max   = 100
  stddev= 0.24650533945628


omsst2@sherwood:/maps/omsst2/diss/papers/rasterlite/tuscany_dtm$ gdalinfo -stats quantized_256.tif
Driver: GTiff/GeoTIFF
Files: quantized_256.tif
Size is 21718, 23957
Origin = (1554651.989999999990687,4924890.740000000223517)
Pixel Size = (10.000000000000000,-10.000000000000000)
Image Structure Metadata:
  COMPRESSION=ZSTD
  INTERLEAVE=BAND
  PREDICTOR=2
Corner Coordinates:
Upper Left  ( 1554651.990, 4924890.740)
Lower Left  ( 1554651.990, 4685320.740)
Upper Right ( 1771831.990, 4924890.740)
Lower Right ( 1771831.990, 4685320.740)
Center      ( 1663241.990, 4805105.740)
Band 1 Block=256x256 Type=Int32, ColorInterp=Gray
  Minimum=-1379.000, Maximum=525727.000, Mean=91365.602, StdDev=78363.261
  NoData Value=-2147483648
  Metadata:
    STATISTICS_MINIMUM=-1379
    STATISTICS_MAXIMUM=525727
    STATISTICS_MEAN=91365.601939996
    STATISTICS_STDDEV=78363.261335925
    STATISTICS_VALID_PERCENT=44.38

omsst2@sherwood:/maps/omsst2/diss/papers/rasterlite/tuscany_dtm$ gdalinfo -stats quantized_2000.tif
Driver: GTiff/GeoTIFF
Files: quantized_2000.tif
Size is 21718, 23957
Origin = (1554651.989999999990687,4924890.740000000223517)
Pixel Size = (10.000000000000000,-10.000000000000000)
Image Structure Metadata:
  COMPRESSION=ZSTD
  INTERLEAVE=BAND
  PREDICTOR=2
Corner Coordinates:
Upper Left  ( 1554651.990, 4924890.740)
Lower Left  ( 1554651.990, 4685320.740)
Upper Right ( 1771831.990, 4924890.740)
Lower Right ( 1771831.990, 4685320.740)
Center      ( 1663241.990, 4805105.740)
Band 1 Block=256x256 Type=Int32, ColorInterp=Gray
  Minimum=-10771.000, Maximum=4107244.000, Mean=713793.766, StdDev=612212.979
  NoData Value=-2147483648
  Metadata:
    STATISTICS_MINIMUM=-10771
    STATISTICS_MAXIMUM=4107244
    STATISTICS_MEAN=713793.76559208
    STATISTICS_STDDEV=612212.97938962
    STATISTICS_VALID_PERCENT=44.38