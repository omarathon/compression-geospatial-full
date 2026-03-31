/maps/omsst2/diss/papers/zalipynis/2019_landsat_band4_mosaic/exact/landsat8_mosaic_B4.tif

omsst2@sherwood:/maps/omsst2/diss/papers/zalipynis/2019_landsat_band4_mosaic/exact$ gdalinfo -stats landsat8_mosaic_B4.tif
Driver: GTiff/GeoTIFF
Files: landsat8_mosaic_B4.tif
Size is 22856, 24918
Coordinate System is:
PROJCRS["WGS 84 / UTM zone 31N",
    BASEGEOGCRS["WGS 84",
        DATUM["World Geodetic System 1984",
            ELLIPSOID["WGS 84",6378137,298.257223563,
                LENGTHUNIT["metre",1]]],
        PRIMEM["Greenwich",0,
            ANGLEUNIT["degree",0.0174532925199433]],
        ID["EPSG",4326]],
    CONVERSION["UTM zone 31N",
        METHOD["Transverse Mercator",
            ID["EPSG",9807]],
        PARAMETER["Latitude of natural origin",0,
            ANGLEUNIT["degree",0.0174532925199433],
            ID["EPSG",8801]],
        PARAMETER["Longitude of natural origin",3,
            ANGLEUNIT["degree",0.0174532925199433],
            ID["EPSG",8802]],
        PARAMETER["Scale factor at natural origin",0.9996,
            SCALEUNIT["unity",1],
            ID["EPSG",8805]],
        PARAMETER["False easting",500000,
            LENGTHUNIT["metre",1],
            ID["EPSG",8806]],
        PARAMETER["False northing",0,
            LENGTHUNIT["metre",1],
            ID["EPSG",8807]]],
    CS[Cartesian,2],
        AXIS["(E)",east,
            ORDER[1],
            LENGTHUNIT["metre",1]],
        AXIS["(N)",north,
            ORDER[2],
            LENGTHUNIT["metre",1]],
    USAGE[
        SCOPE["Navigation and medium accuracy spatial referencing."],
        AREA["Between 0°E and 6°E, northern hemisphere between equator and 84°N, onshore and offshore. Algeria. Andorra. Belgium. Benin. Burkina Faso. Denmark - North Sea. France. Germany - North Sea. Ghana. Luxembourg. Mali. Netherlands. Niger. Nigeria. Norway. Spain. Togo. United Kingdom (UK) - North Sea."],
        BBOX[0,0,84,6]],
    ID["EPSG",32631]]
Data axis to CRS axis mapping: 1,2
Origin = (408270.000000000000000,5877510.000000000000000)
Pixel Size = (30.000000000000000,-30.000000000000000)
Metadata:
  AREA_OR_POINT=Point
Image Structure Metadata:
  INTERLEAVE=BAND
Corner Coordinates:
Upper Left  (  408270.000, 5877510.000) (  1d37'54.74"E, 53d 2'21.24"N)
Lower Left  (  408270.000, 5129970.000) (  1d48'30.69"E, 46d19' 1.55"N)
Upper Right ( 1093950.000, 5877510.000) ( 11d48'10.09"E, 52d43'10.63"N)
Lower Right ( 1093950.000, 5129970.000) ( 10d40'49.39"E, 46d 3'52.32"N)
Center      (  751110.000, 5503740.000) (  6d28'40.26"E, 49d38' 2.13"N)
Band 1 Block=22856x1 Type=UInt16, ColorInterp=Gray
  Minimum=0.000, Maximum=65533.000, Mean=8497.283, StdDev=9681.182
  NoData Value=65535
  Metadata:
    STATISTICS_MINIMUM=0
    STATISTICS_MAXIMUM=65533
    STATISTICS_MEAN=8497.2826488685
    STATISTICS_STDDEV=9681.1816288539
    STATISTICS_VALID_PERCENT=81.53