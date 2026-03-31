/maps/omsst2/diss/papers/rasterlite/landsat_pano/LC08_192029_20130702_B8.TIF

omsst2@sherwood:/maps/omsst2/diss/papers/rasterlite/landsat_pano$ gdalinfo -stats LC08_192029_20130702_B8.TIF
Driver: GTiff/GeoTIFF
Files: LC08_192029_20130702_B8.TIF
       LC08_192029_20130702_B8.TIF.aux.xml
Size is 15301, 15541
Coordinate System is:
PROJCRS["WGS 84 / UTM zone 32N",
    BASEGEOGCRS["WGS 84",
        DATUM["World Geodetic System 1984",
            ELLIPSOID["WGS 84",6378137,298.257223563,
                LENGTHUNIT["metre",1]]],
        PRIMEM["Greenwich",0,
            ANGLEUNIT["degree",0.0174532925199433]],
        ID["EPSG",4326]],
    CONVERSION["UTM zone 32N",
        METHOD["Transverse Mercator",
            ID["EPSG",9807]],
        PARAMETER["Latitude of natural origin",0,
            ANGLEUNIT["degree",0.0174532925199433],
            ID["EPSG",8801]],
        PARAMETER["Longitude of natural origin",9,
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
        AREA["Between 6°E and 12°E, northern hemisphere between equator and 84°N, onshore and offshore. Algeria. Austria. Cameroon. Denmark. Equatorial Guinea. France. Gabon. Germany. Italy. Libya. Liechtenstein. Monaco. Netherlands. Niger. Nigeria. Norway. Sao Tome and Principe. Svalbard. Sweden. Switzerland. Tunisia. Vatican City State."],
        BBOX[0,6,84,12]],
    ID["EPSG",32632]]
Data axis to CRS axis mapping: 1,2
Origin = (587092.500000000000000,5058607.500000000000000)
Pixel Size = (15.000000000000000,-15.000000000000000)
Metadata:
  AREA_OR_POINT=Point
Image Structure Metadata:
  COMPRESSION=LZW
  INTERLEAVE=BAND
  PREDICTOR=2
Corner Coordinates:
Upper Left  (  587092.500, 5058607.500) ( 10d 7' 5.66"E, 45d40'31.97"N)
Lower Left  (  587092.500, 4825492.500) ( 10d 4'43.18"E, 43d34'38.32"N)
Upper Right (  816607.500, 5058607.500) ( 13d 3'37.37"E, 45d36'31.69"N)
Lower Right (  816607.500, 4825492.500) ( 12d55' 1.51"E, 43d30'54.89"N)
Center      (  701850.000, 4942050.000) ( 11d32'36.80"E, 44d36'12.50"N)
Band 1 Block=256x256 Type=UInt16, ColorInterp=Gray
  Min=0.000 Max=65535.000
  Minimum=0.000, Maximum=65535.000, Mean=6512.016, StdDev=4465.693
  Metadata:
    STATISTICS_MAXIMUM=65535
    STATISTICS_MEAN=6512.015598388
    STATISTICS_MINIMUM=0
    STATISTICS_STDDEV=4465.6930145131
    STATISTICS_VALID_PERCENT=100