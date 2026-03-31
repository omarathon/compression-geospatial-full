/maps/omsst2/diss/papers/rasterlite/ETOPO1/ETOPO1_Ice_g_geotiff.tif

omsst2@sherwood:/maps/omsst2/diss/papers/rasterlite/ETOPO1$ gdalinfo -stats ETOPO1_Ice_g_geotiff.tif
Driver: GTiff/GeoTIFF
Files: ETOPO1_Ice_g_geotiff.tif
       ETOPO1_Ice_g_geotiff.tif.aux.xml
Size is 21601, 10801
Origin = (-180.008333333333326,90.008333333333340)
Pixel Size = (0.016666666666667,-0.016666666666667)
Metadata:
  NC_GLOBAL#Conventions=COARDS/CF-1.0
  NC_GLOBAL#title=ETOPO1_Ice_g_gmt4.grd
  NC_GLOBAL#history=grdreformat ETOPO1_Ice_g_gdal.grd ETOPO1_Ice_g_gmt4.grd=ni
  NC_GLOBAL#GMT_version=4.4.0
  NC_GLOBAL#node_offset=0
  z#long_name=z
  z#_FillValue=-2147483648
  z#actual_range=-10898, 8271
  x#long_name=Longitude
  x#actual_range=-180, 180
  x#units=degrees
  y#long_name=Latitude
  y#actual_range=-90, 90
  y#units=degrees
Image Structure Metadata:
  INTERLEAVE=BAND
Corner Coordinates:
Upper Left  (-180.0083333,  90.0083333)
Lower Left  (-180.0083333, -90.0083333)
Upper Right ( 180.0083333,  90.0083333)
Lower Right ( 180.0083333, -90.0083333)
Center      (   0.0000000,   0.0000000)
Band 1 Block=21601x1 Type=Int16, ColorInterp=Gray
  Min=-10898.000 Max=8271.000
  Minimum=-10898.000, Maximum=8271.000, Mean=-1892.353, StdDev=2650.140
  NoData Value=-2147483648
  Metadata:
    NETCDF_VARNAME=z
    STATISTICS_MAXIMUM=8271
    STATISTICS_MEAN=-1892.3528468299
    STATISTICS_MINIMUM=-10898
    STATISTICS_STDDEV=2650.1402766088
    STATISTICS_VALID_PERCENT=100