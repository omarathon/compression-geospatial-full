/maps/omsst2/diss/papers/zalipynis/2018/landsat8_mosaic_B4.tif
/maps/omsst2/diss/papers/zalipynis/2018/landsat8_mosaic_B5.tif

omsst2@sherwood:/maps/omsst2/diss/papers/zalipynis/2018$ gdalinfo -stats landsat8_mosaic_B4.tif
Driver: GTiff/GeoTIFF
Files: landsat8_mosaic_B4.tif
Size is 38666, 24931
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
Origin = (-53160.000000000000000,5878620.000000000000000)
Pixel Size = (30.000000000000000,-30.000000000000000)
Metadata:
  AREA_OR_POINT=Point
Image Structure Metadata:
  INTERLEAVE=BAND
Corner Coordinates:
Upper Left  (  -53160.000, 5878620.000) (  0d47'34.36"E, 52d46'21.56"N)
Lower Left  (  -53160.000, 5130690.000) (  1d50'30.89"E, 46d 6'18.40"N)
Upper Right ( 1106820.000, 5878620.000) ( 17d59'35.02"E, 52d42'54.86"N)
Lower Right ( 1106820.000, 5130690.000) ( 16d50'46.18"E, 46d 3'34.88"N)
Center      (  526830.000, 5504655.000) (  9d22'19.25"E, 49d41'37.73"N)
Band 1 Block=38666x1 Type=UInt16, ColorInterp=Gray
  Minimum=0.000, Maximum=65534.000, Mean=8144.908, StdDev=9224.792
  NoData Value=65535
  Metadata:
    STATISTICS_MINIMUM=0
    STATISTICS_MAXIMUM=65534
    STATISTICS_MEAN=8144.9076766457
    STATISTICS_STDDEV=9224.7918956502
    STATISTICS_VALID_PERCENT=85.04
omsst2@sherwood:/maps/omsst2/diss/papers/zalipynis/2018$ gdalinfo -stats landsat8_mosaic_B5.tif
Driver: GTiff/GeoTIFF
Files: landsat8_mosaic_B5.tif
Size is 38666, 24931
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
Origin = (-53160.000000000000000,5878620.000000000000000)
Pixel Size = (30.000000000000000,-30.000000000000000)
Metadata:
  AREA_OR_POINT=Point
Image Structure Metadata:
  INTERLEAVE=BAND
Corner Coordinates:
Upper Left  (  -53160.000, 5878620.000) (  0d47'34.36"E, 52d46'21.56"N)
Lower Left  (  -53160.000, 5130690.000) (  1d50'30.89"E, 46d 6'18.40"N)
Upper Right ( 1106820.000, 5878620.000) ( 17d59'35.02"E, 52d42'54.86"N)
Lower Right ( 1106820.000, 5130690.000) ( 16d50'46.18"E, 46d 3'34.88"N)
Center      (  526830.000, 5504655.000) (  9d22'19.25"E, 49d41'37.73"N)
Band 1 Block=38666x1 Type=UInt16, ColorInterp=Gray
  Minimum=0.000, Maximum=65534.000, Mean=13982.854, StdDev=12020.035
  NoData Value=65535
  Metadata:
    STATISTICS_MINIMUM=0
    STATISTICS_MAXIMUM=65534
    STATISTICS_MEAN=13982.85448294
    STATISTICS_STDDEV=12020.034985348
    STATISTICS_VALID_PERCENT=85.04