/maps/omsst2/diss/papers/rasterlite/SRTM_Italy/srtm_italy_exact/srtm_tiles/SRTM_Italy_final.tif

omsst2@sherwood:/maps/omsst2/diss/papers/rasterlite/SRTM_Italy/srtm_italy_exact/srtm_tiles$ gdalinfo -stats SRTM_Italy_final.tif
Driver: GTiff/GeoTIFF
Files: SRTM_Italy_final.tif
       SRTM_Italy_final.tif.aux.xml
Size is 18000, 18000
Coordinate System is:
GEOGCRS["WGS 84",
    ENSEMBLE["World Geodetic System 1984 ensemble",
        MEMBER["World Geodetic System 1984 (Transit)"],
        MEMBER["World Geodetic System 1984 (G730)"],
        MEMBER["World Geodetic System 1984 (G873)"],
        MEMBER["World Geodetic System 1984 (G1150)"],
        MEMBER["World Geodetic System 1984 (G1674)"],
        MEMBER["World Geodetic System 1984 (G1762)"],
        MEMBER["World Geodetic System 1984 (G2139)"],
        ELLIPSOID["WGS 84",6378137,298.257223563,
            LENGTHUNIT["metre",1]],
        ENSEMBLEACCURACY[2.0]],
    PRIMEM["Greenwich",0,
        ANGLEUNIT["degree",0.0174532925199433]],
    CS[ellipsoidal,2],
        AXIS["geodetic latitude (Lat)",north,
            ORDER[1],
            ANGLEUNIT["degree",0.0174532925199433]],
        AXIS["geodetic longitude (Lon)",east,
            ORDER[2],
            ANGLEUNIT["degree",0.0174532925199433]],
    USAGE[
        SCOPE["Horizontal component of 3D system."],
        AREA["World."],
        BBOX[-90,-180,90,180]],
    ID["EPSG",4326]]
Data axis to CRS axis mapping: 2,1
Origin = (5.000000000000000,50.000000000000000)
Pixel Size = (0.000833333333333,-0.000833333333333)
Metadata:
  AREA_OR_POINT=Area
Image Structure Metadata:
  INTERLEAVE=BAND
Corner Coordinates:
Upper Left  (   5.0000000,  50.0000000) (  5d 0' 0.00"E, 50d 0' 0.00"N)
Lower Left  (   5.0000000,  35.0000000) (  5d 0' 0.00"E, 35d 0' 0.00"N)
Upper Right (  20.0000000,  50.0000000) ( 20d 0' 0.00"E, 50d 0' 0.00"N)
Lower Right (  20.0000000,  35.0000000) ( 20d 0' 0.00"E, 35d 0' 0.00"N)
Center      (  12.5000000,  42.5000000) ( 12d30' 0.00"E, 42d30' 0.00"N)
Band 1 Block=18000x1 Type=Int16, ColorInterp=Gray
  Min=-121.000 Max=4783.000
  Minimum=-121.000, Maximum=4783.000, Mean=579.556, StdDev=542.487
  NoData Value=-9999
  Metadata:
    STATISTICS_MAXIMUM=4783
    STATISTICS_MEAN=579.55604404939
    STATISTICS_MINIMUM=-121
    STATISTICS_STDDEV=542.48712620365
    STATISTICS_VALID_PERCENT=56.06

