/maps/omsst2/diss/papers/zalipynis/2018_2/landsat/landsat8_path190_row031_stack.tif

omsst2@sherwood:/maps/omsst2/diss/papers/zalipynis/2018_2/landsat$ gdalinfo -stats landsat8_path190_row031_stack.tif
Driver: GTiff/GeoTIFF
Files: landsat8_path190_row031_stack.tif
Size is 9051, 9069
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
Origin = (1250850.000000000000000,4814190.000000000000000)
Pixel Size = (30.000000000000000,-30.000000000000000)
Metadata:
  AREA_OR_POINT=Point
Image Structure Metadata:
  INTERLEAVE=BAND
Corner Coordinates:
Upper Left  ( 1250850.000, 4814190.000) ( 12d13'31.47"E, 43d 6'25.14"N)
Lower Left  ( 1250850.000, 4542120.000) ( 11d52'50.54"E, 40d41'13.43"N)
Upper Right ( 1522380.000, 4814190.000) ( 15d29'39.25"E, 42d47'34.79"N)
Lower Right ( 1522380.000, 4542120.000) ( 15d 2' 1.60"E, 40d23'54.19"N)
Center      ( 1386615.000, 4678155.000) ( 13d39'30.68"E, 41d45'28.36"N)
Band 1 Block=9051x1 Type=UInt16, ColorInterp=Gray
  Minimum=0.000, Maximum=54915.000, Mean=27042.974, StdDev=20443.366
  NoData Value=65535
  Metadata:
    STATISTICS_MINIMUM=0
    STATISTICS_MAXIMUM=54915
    STATISTICS_MEAN=27042.974191719
    STATISTICS_STDDEV=20443.366344539
    STATISTICS_VALID_PERCENT=78.3