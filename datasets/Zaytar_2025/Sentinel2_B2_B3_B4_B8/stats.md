/maps/omsst2/diss/papers/zaytar/2025/final/*.tif

omsst2@sherwood:/maps/omsst2/diss/papers/zaytar/2025/final$ for f in *.tif; do
  gdalinfo -stats "$f"
done
Driver: GTiff/GeoTIFF
Files: S2A_MSIL2A_20240222T073931_R092_T36MZD_20240222T171849.tif
Size is 10980, 10980
Coordinate System is:
PROJCRS["WGS 84 / UTM zone 36S",
    BASEGEOGCRS["WGS 84",
        DATUM["World Geodetic System 1984",
            ELLIPSOID["WGS 84",6378137,298.257223563,
                LENGTHUNIT["metre",1]]],
        PRIMEM["Greenwich",0,
            ANGLEUNIT["degree",0.0174532925199433]],
        ID["EPSG",4326]],
    CONVERSION["UTM zone 36S",
        METHOD["Transverse Mercator",
            ID["EPSG",9807]],
        PARAMETER["Latitude of natural origin",0,
            ANGLEUNIT["degree",0.0174532925199433],
            ID["EPSG",8801]],
        PARAMETER["Longitude of natural origin",33,
            ANGLEUNIT["degree",0.0174532925199433],
            ID["EPSG",8802]],
        PARAMETER["Scale factor at natural origin",0.9996,
            SCALEUNIT["unity",1],
            ID["EPSG",8805]],
        PARAMETER["False easting",500000,
            LENGTHUNIT["metre",1],
            ID["EPSG",8806]],
        PARAMETER["False northing",10000000,
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
        AREA["Between 30°E and 36°E, southern hemisphere between 80°S and equator, onshore and offshore. Burundi. Eswatini (Swaziland). Kenya. Malawi. Mozambique. Rwanda. South Africa. Tanzania. Uganda. Zambia. Zimbabwe."],
        BBOX[-80,30,0,36]],
    ID["EPSG",32736]]
Data axis to CRS axis mapping: 1,2
Origin = (799980.000000000000000,9900040.000000000000000)
Pixel Size = (10.000000000000000,-10.000000000000000)
Metadata:
  AREA_OR_POINT=Area
Image Structure Metadata:
  LAYOUT=COG
  COMPRESSION=LERC_ZSTD
  LERC_VERSION=2.4
  INTERLEAVE=PIXEL
Corner Coordinates:
Upper Left  (  799980.000, 9900040.000) ( 35d41'42.64"E,  0d54'12.10"S)
Lower Left  (  799980.000, 9790240.000) ( 35d41'46.72"E,  1d53'44.32"S)
Upper Right (  909780.000, 9900040.000) ( 36d40'49.78"E,  0d54' 8.97"S)
Lower Right (  909780.000, 9790240.000) ( 36d40'55.34"E,  1d53'37.74"S)
Center      (  854880.000, 9845140.000) ( 36d11'18.62"E,  1d23'55.97"S)
Band 1 Block=512x512 Type=UInt16, ColorInterp=Gray
  Minimum=644.000, Maximum=12768.000, Mean=1554.625, StdDev=308.604
  NoData Value=0
  Overviews: 5490x5490, 2745x2745, 1373x1373, 687x687, 344x344
  Metadata:
    STATISTICS_MINIMUM=644
    STATISTICS_MAXIMUM=12768
    STATISTICS_MEAN=1554.6251464374
    STATISTICS_STDDEV=308.60370834725
    STATISTICS_VALID_PERCENT=82.18
Band 2 Block=512x512 Type=UInt16, ColorInterp=Undefined
  Minimum=1050.000, Maximum=12192.000, Mean=1865.261, StdDev=314.216
  NoData Value=0
  Overviews: 5490x5490, 2745x2745, 1373x1373, 687x687, 344x344
  Metadata:
    STATISTICS_MINIMUM=1050
    STATISTICS_MAXIMUM=12192
    STATISTICS_MEAN=1865.2606720254
    STATISTICS_STDDEV=314.21571791705
    STATISTICS_VALID_PERCENT=82.18
Band 3 Block=512x512 Type=UInt16, ColorInterp=Undefined
  Minimum=934.000, Maximum=14664.000, Mean=1949.769, StdDev=446.382
  NoData Value=0
  Overviews: 5490x5490, 2745x2745, 1373x1373, 687x687, 344x344
  Metadata:
    STATISTICS_MINIMUM=934
    STATISTICS_MAXIMUM=14664
    STATISTICS_MEAN=1949.7688702799
    STATISTICS_STDDEV=446.38160507779
    STATISTICS_VALID_PERCENT=82.18
Band 4 Block=512x512 Type=UInt16, ColorInterp=Undefined
  Minimum=1100.000, Maximum=16848.000, Mean=3689.796, StdDev=591.173
  NoData Value=0
  Overviews: 5490x5490, 2745x2745, 1373x1373, 687x687, 344x344
  Metadata:
    STATISTICS_MINIMUM=1100
    STATISTICS_MAXIMUM=16848
    STATISTICS_MEAN=3689.7958908296
    STATISTICS_STDDEV=591.17254628673
    STATISTICS_VALID_PERCENT=82.18
Driver: GTiff/GeoTIFF
Files: S2A_MSIL2A_20240303T073821_R092_T36MZD_20240303T123829.tif
Size is 10980, 10980
Coordinate System is:
PROJCRS["WGS 84 / UTM zone 36S",
    BASEGEOGCRS["WGS 84",
        DATUM["World Geodetic System 1984",
            ELLIPSOID["WGS 84",6378137,298.257223563,
                LENGTHUNIT["metre",1]]],
        PRIMEM["Greenwich",0,
            ANGLEUNIT["degree",0.0174532925199433]],
        ID["EPSG",4326]],
    CONVERSION["UTM zone 36S",
        METHOD["Transverse Mercator",
            ID["EPSG",9807]],
        PARAMETER["Latitude of natural origin",0,
            ANGLEUNIT["degree",0.0174532925199433],
            ID["EPSG",8801]],
        PARAMETER["Longitude of natural origin",33,
            ANGLEUNIT["degree",0.0174532925199433],
            ID["EPSG",8802]],
        PARAMETER["Scale factor at natural origin",0.9996,
            SCALEUNIT["unity",1],
            ID["EPSG",8805]],
        PARAMETER["False easting",500000,
            LENGTHUNIT["metre",1],
            ID["EPSG",8806]],
        PARAMETER["False northing",10000000,
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
        AREA["Between 30°E and 36°E, southern hemisphere between 80°S and equator, onshore and offshore. Burundi. Eswatini (Swaziland). Kenya. Malawi. Mozambique. Rwanda. South Africa. Tanzania. Uganda. Zambia. Zimbabwe."],
        BBOX[-80,30,0,36]],
    ID["EPSG",32736]]
Data axis to CRS axis mapping: 1,2
Origin = (799980.000000000000000,9900040.000000000000000)
Pixel Size = (10.000000000000000,-10.000000000000000)
Metadata:
  AREA_OR_POINT=Area
Image Structure Metadata:
  LAYOUT=COG
  COMPRESSION=LERC_ZSTD
  LERC_VERSION=2.4
  INTERLEAVE=PIXEL
Corner Coordinates:
Upper Left  (  799980.000, 9900040.000) ( 35d41'42.64"E,  0d54'12.10"S)
Lower Left  (  799980.000, 9790240.000) ( 35d41'46.72"E,  1d53'44.32"S)
Upper Right (  909780.000, 9900040.000) ( 36d40'49.78"E,  0d54' 8.97"S)
Lower Right (  909780.000, 9790240.000) ( 36d40'55.34"E,  1d53'37.74"S)
Center      (  854880.000, 9845140.000) ( 36d11'18.62"E,  1d23'55.97"S)
Band 1 Block=512x512 Type=UInt16, ColorInterp=Gray
  Minimum=746.000, Maximum=18192.000, Mean=1518.090, StdDev=304.587
  NoData Value=0
  Overviews: 5490x5490, 2745x2745, 1373x1373, 687x687, 344x344
  Metadata:
    STATISTICS_MINIMUM=746
    STATISTICS_MAXIMUM=18192
    STATISTICS_MEAN=1518.0903812399
    STATISTICS_STDDEV=304.58664176222
    STATISTICS_VALID_PERCENT=83.37
Band 2 Block=512x512 Type=UInt16, ColorInterp=Undefined
  Minimum=894.000, Maximum=18512.000, Mean=1837.574, StdDev=309.275
  NoData Value=0
  Overviews: 5490x5490, 2745x2745, 1373x1373, 687x687, 344x344
  Metadata:
    STATISTICS_MINIMUM=894
    STATISTICS_MAXIMUM=18512
    STATISTICS_MEAN=1837.5738267707
    STATISTICS_STDDEV=309.27473451403
    STATISTICS_VALID_PERCENT=83.37
Band 3 Block=512x512 Type=UInt16, ColorInterp=Undefined
  Minimum=755.000, Maximum=17808.000, Mean=1854.909, StdDev=422.098
  NoData Value=0
  Overviews: 5490x5490, 2745x2745, 1373x1373, 687x687, 344x344
  Metadata:
    STATISTICS_MINIMUM=755
    STATISTICS_MAXIMUM=17808
    STATISTICS_MEAN=1854.9093536376
    STATISTICS_STDDEV=422.09792942226
    STATISTICS_VALID_PERCENT=83.37
Band 4 Block=512x512 Type=UInt16, ColorInterp=Undefined
  Minimum=1076.000, Maximum=16912.000, Mean=3668.242, StdDev=599.335
  NoData Value=0
  Overviews: 5490x5490, 2745x2745, 1373x1373, 687x687, 344x344
  Metadata:
    STATISTICS_MINIMUM=1076
    STATISTICS_MAXIMUM=16912
    STATISTICS_MEAN=3668.2415024897
    STATISTICS_STDDEV=599.33487074995
    STATISTICS_VALID_PERCENT=83.37
Driver: GTiff/GeoTIFF
Files: S2A_MSIL2A_20240313T073711_R092_T36MZD_20240313T184208.tif
Size is 10980, 10980
Coordinate System is:
PROJCRS["WGS 84 / UTM zone 36S",
    BASEGEOGCRS["WGS 84",
        DATUM["World Geodetic System 1984",
            ELLIPSOID["WGS 84",6378137,298.257223563,
                LENGTHUNIT["metre",1]]],
        PRIMEM["Greenwich",0,
            ANGLEUNIT["degree",0.0174532925199433]],
        ID["EPSG",4326]],
    CONVERSION["UTM zone 36S",
        METHOD["Transverse Mercator",
            ID["EPSG",9807]],
        PARAMETER["Latitude of natural origin",0,
            ANGLEUNIT["degree",0.0174532925199433],
            ID["EPSG",8801]],
        PARAMETER["Longitude of natural origin",33,
            ANGLEUNIT["degree",0.0174532925199433],
            ID["EPSG",8802]],
        PARAMETER["Scale factor at natural origin",0.9996,
            SCALEUNIT["unity",1],
            ID["EPSG",8805]],
        PARAMETER["False easting",500000,
            LENGTHUNIT["metre",1],
            ID["EPSG",8806]],
        PARAMETER["False northing",10000000,
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
        AREA["Between 30°E and 36°E, southern hemisphere between 80°S and equator, onshore and offshore. Burundi. Eswatini (Swaziland). Kenya. Malawi. Mozambique. Rwanda. South Africa. Tanzania. Uganda. Zambia. Zimbabwe."],
        BBOX[-80,30,0,36]],
    ID["EPSG",32736]]
Data axis to CRS axis mapping: 1,2
Origin = (799980.000000000000000,9900040.000000000000000)
Pixel Size = (10.000000000000000,-10.000000000000000)
Metadata:
  AREA_OR_POINT=Area
Image Structure Metadata:
  LAYOUT=COG
  COMPRESSION=LERC_ZSTD
  LERC_VERSION=2.4
  INTERLEAVE=PIXEL
Corner Coordinates:
Upper Left  (  799980.000, 9900040.000) ( 35d41'42.64"E,  0d54'12.10"S)
Lower Left  (  799980.000, 9790240.000) ( 35d41'46.72"E,  1d53'44.32"S)
Upper Right (  909780.000, 9900040.000) ( 36d40'49.78"E,  0d54' 8.97"S)
Lower Right (  909780.000, 9790240.000) ( 36d40'55.34"E,  1d53'37.74"S)
Center      (  854880.000, 9845140.000) ( 36d11'18.62"E,  1d23'55.97"S)
Band 1 Block=512x512 Type=UInt16, ColorInterp=Gray
  Minimum=848.000, Maximum=19472.000, Mean=1598.922, StdDev=613.878
  NoData Value=0
  Overviews: 5490x5490, 2745x2745, 1373x1373, 687x687, 344x344
  Metadata:
    STATISTICS_MINIMUM=848
    STATISTICS_MAXIMUM=19472
    STATISTICS_MEAN=1598.9224168416
    STATISTICS_STDDEV=613.87797326705
    STATISTICS_VALID_PERCENT=83.44
Band 2 Block=512x512 Type=UInt16, ColorInterp=Undefined
  Minimum=999.000, Maximum=18400.000, Mean=1905.868, StdDev=579.276
  NoData Value=0
  Overviews: 5490x5490, 2745x2745, 1373x1373, 687x687, 344x344
  Metadata:
    STATISTICS_MINIMUM=999
    STATISTICS_MAXIMUM=18400
    STATISTICS_MEAN=1905.8683379146
    STATISTICS_STDDEV=579.27575396503
    STATISTICS_VALID_PERCENT=83.44
Band 3 Block=512x512 Type=UInt16, ColorInterp=Undefined
  Minimum=901.000, Maximum=17680.000, Mean=1955.149, StdDev=646.382
  NoData Value=0
  Overviews: 5490x5490, 2745x2745, 1373x1373, 687x687, 344x344
  Metadata:
    STATISTICS_MINIMUM=901
    STATISTICS_MAXIMUM=17680
    STATISTICS_MEAN=1955.1487740117
    STATISTICS_STDDEV=646.38191941936
    STATISTICS_VALID_PERCENT=83.44
Band 4 Block=512x512 Type=UInt16, ColorInterp=Undefined
  Minimum=1039.000, Maximum=16896.000, Mean=3779.197, StdDev=729.504
  NoData Value=0
  Overviews: 5490x5490, 2745x2745, 1373x1373, 687x687, 344x344
  Metadata:
    STATISTICS_MINIMUM=1039
    STATISTICS_MAXIMUM=16896
    STATISTICS_MEAN=3779.1971193717
    STATISTICS_STDDEV=729.50366862012
    STATISTICS_VALID_PERCENT=83.44
Driver: GTiff/GeoTIFF
Files: S2A_MSIL2A_20240909T073611_R092_T36MZD_20240909T122046.tif
Size is 10980, 10980
Coordinate System is:
PROJCRS["WGS 84 / UTM zone 36S",
    BASEGEOGCRS["WGS 84",
        DATUM["World Geodetic System 1984",
            ELLIPSOID["WGS 84",6378137,298.257223563,
                LENGTHUNIT["metre",1]]],
        PRIMEM["Greenwich",0,
            ANGLEUNIT["degree",0.0174532925199433]],
        ID["EPSG",4326]],
    CONVERSION["UTM zone 36S",
        METHOD["Transverse Mercator",
            ID["EPSG",9807]],
        PARAMETER["Latitude of natural origin",0,
            ANGLEUNIT["degree",0.0174532925199433],
            ID["EPSG",8801]],
        PARAMETER["Longitude of natural origin",33,
            ANGLEUNIT["degree",0.0174532925199433],
            ID["EPSG",8802]],
        PARAMETER["Scale factor at natural origin",0.9996,
            SCALEUNIT["unity",1],
            ID["EPSG",8805]],
        PARAMETER["False easting",500000,
            LENGTHUNIT["metre",1],
            ID["EPSG",8806]],
        PARAMETER["False northing",10000000,
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
        AREA["Between 30°E and 36°E, southern hemisphere between 80°S and equator, onshore and offshore. Burundi. Eswatini (Swaziland). Kenya. Malawi. Mozambique. Rwanda. South Africa. Tanzania. Uganda. Zambia. Zimbabwe."],
        BBOX[-80,30,0,36]],
    ID["EPSG",32736]]
Data axis to CRS axis mapping: 1,2
Origin = (799980.000000000000000,9900040.000000000000000)
Pixel Size = (10.000000000000000,-10.000000000000000)
Metadata:
  AREA_OR_POINT=Area
Image Structure Metadata:
  LAYOUT=COG
  COMPRESSION=LERC_ZSTD
  LERC_VERSION=2.4
  INTERLEAVE=PIXEL
Corner Coordinates:
Upper Left  (  799980.000, 9900040.000) ( 35d41'42.64"E,  0d54'12.10"S)
Lower Left  (  799980.000, 9790240.000) ( 35d41'46.72"E,  1d53'44.32"S)
Upper Right (  909780.000, 9900040.000) ( 36d40'49.78"E,  0d54' 8.97"S)
Lower Right (  909780.000, 9790240.000) ( 36d40'55.34"E,  1d53'37.74"S)
Center      (  854880.000, 9845140.000) ( 36d11'18.62"E,  1d23'55.97"S)
Band 1 Block=512x512 Type=UInt16, ColorInterp=Gray
  Minimum=794.000, Maximum=16272.000, Mean=1674.687, StdDev=312.994
  NoData Value=0
  Overviews: 5490x5490, 2745x2745, 1373x1373, 687x687
  Metadata:
    STATISTICS_MINIMUM=794
    STATISTICS_MAXIMUM=16272
    STATISTICS_MEAN=1674.687067411
    STATISTICS_STDDEV=312.99396656895
    STATISTICS_VALID_PERCENT=82.58
Band 2 Block=512x512 Type=UInt16, ColorInterp=Undefined
  Minimum=954.000, Maximum=16120.000, Mean=1972.122, StdDev=341.371
  NoData Value=0
  Overviews: 5490x5490, 2745x2745, 1373x1373, 687x687
  Metadata:
    STATISTICS_MINIMUM=954
    STATISTICS_MAXIMUM=16120
    STATISTICS_MEAN=1972.1222861169
    STATISTICS_STDDEV=341.37121405392
    STATISTICS_VALID_PERCENT=82.58
Band 3 Block=512x512 Type=UInt16, ColorInterp=Undefined
  Minimum=964.000, Maximum=16176.000, Mean=2226.399, StdDev=508.637
  NoData Value=0
  Overviews: 5490x5490, 2745x2745, 1373x1373, 687x687
  Metadata:
    STATISTICS_MINIMUM=964
    STATISTICS_MAXIMUM=16176
    STATISTICS_MEAN=2226.3987403177
    STATISTICS_STDDEV=508.63735432458
    STATISTICS_VALID_PERCENT=82.58
Band 4 Block=512x512 Type=UInt16, ColorInterp=Undefined
  Minimum=1070.000, Maximum=16496.000, Mean=3579.206, StdDev=503.739
  NoData Value=0
  Overviews: 5490x5490, 2745x2745, 1373x1373, 687x687
  Metadata:
    STATISTICS_MINIMUM=1070
    STATISTICS_MAXIMUM=16496
    STATISTICS_MEAN=3579.2060097825
    STATISTICS_STDDEV=503.73917412012
    STATISTICS_VALID_PERCENT=82.58
Driver: GTiff/GeoTIFF
Files: S2B_MSIL2A_20240207T074009_R092_T36MZD_20240207T115755.tif
Size is 10980, 10980
Coordinate System is:
PROJCRS["WGS 84 / UTM zone 36S",
    BASEGEOGCRS["WGS 84",
        DATUM["World Geodetic System 1984",
            ELLIPSOID["WGS 84",6378137,298.257223563,
                LENGTHUNIT["metre",1]]],
        PRIMEM["Greenwich",0,
            ANGLEUNIT["degree",0.0174532925199433]],
        ID["EPSG",4326]],
    CONVERSION["UTM zone 36S",
        METHOD["Transverse Mercator",
            ID["EPSG",9807]],
        PARAMETER["Latitude of natural origin",0,
            ANGLEUNIT["degree",0.0174532925199433],
            ID["EPSG",8801]],
        PARAMETER["Longitude of natural origin",33,
            ANGLEUNIT["degree",0.0174532925199433],
            ID["EPSG",8802]],
        PARAMETER["Scale factor at natural origin",0.9996,
            SCALEUNIT["unity",1],
            ID["EPSG",8805]],
        PARAMETER["False easting",500000,
            LENGTHUNIT["metre",1],
            ID["EPSG",8806]],
        PARAMETER["False northing",10000000,
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
        AREA["Between 30°E and 36°E, southern hemisphere between 80°S and equator, onshore and offshore. Burundi. Eswatini (Swaziland). Kenya. Malawi. Mozambique. Rwanda. South Africa. Tanzania. Uganda. Zambia. Zimbabwe."],
        BBOX[-80,30,0,36]],
    ID["EPSG",32736]]
Data axis to CRS axis mapping: 1,2
Origin = (799980.000000000000000,9900040.000000000000000)
Pixel Size = (10.000000000000000,-10.000000000000000)
Metadata:
  AREA_OR_POINT=Area
Image Structure Metadata:
  LAYOUT=COG
  COMPRESSION=LERC_ZSTD
  LERC_VERSION=2.4
  INTERLEAVE=PIXEL
Corner Coordinates:
Upper Left  (  799980.000, 9900040.000) ( 35d41'42.64"E,  0d54'12.10"S)
Lower Left  (  799980.000, 9790240.000) ( 35d41'46.72"E,  1d53'44.32"S)
Upper Right (  909780.000, 9900040.000) ( 36d40'49.78"E,  0d54' 8.97"S)
Lower Right (  909780.000, 9790240.000) ( 36d40'55.34"E,  1d53'37.74"S)
Center      (  854880.000, 9845140.000) ( 36d11'18.62"E,  1d23'55.97"S)
Band 1 Block=512x512 Type=UInt16, ColorInterp=Gray
  Minimum=1.000, Maximum=18848.000, Mean=1628.276, StdDev=818.329
  NoData Value=0
  Overviews: 5490x5490, 2745x2745, 1373x1373, 687x687, 344x344
  Metadata:
    STATISTICS_MINIMUM=1
    STATISTICS_MAXIMUM=18848
    STATISTICS_MEAN=1628.2758631664
    STATISTICS_STDDEV=818.32944385872
    STATISTICS_VALID_PERCENT=82.1
Band 2 Block=512x512 Type=UInt16, ColorInterp=Undefined
  Minimum=686.000, Maximum=18016.000, Mean=1894.889, StdDev=765.881
  NoData Value=0
  Overviews: 5490x5490, 2745x2745, 1373x1373, 687x687, 344x344
  Metadata:
    STATISTICS_MINIMUM=686
    STATISTICS_MAXIMUM=18016
    STATISTICS_MEAN=1894.8893580089
    STATISTICS_STDDEV=765.88065286274
    STATISTICS_VALID_PERCENT=82.1
Band 3 Block=512x512 Type=UInt16, ColorInterp=Undefined
  Minimum=412.000, Maximum=17440.000, Mean=1870.147, StdDev=799.019
  NoData Value=0
  Overviews: 5490x5490, 2745x2745, 1373x1373, 687x687, 344x344
  Metadata:
    STATISTICS_MINIMUM=412
    STATISTICS_MAXIMUM=17440
    STATISTICS_MEAN=1870.1474612682
    STATISTICS_STDDEV=799.01856865241
    STATISTICS_VALID_PERCENT=82.1
Band 4 Block=512x512 Type=UInt16, ColorInterp=Undefined
  Minimum=582.000, Maximum=16752.000, Mean=3845.893, StdDev=833.768
  NoData Value=0
  Overviews: 5490x5490, 2745x2745, 1373x1373, 687x687, 344x344
  Metadata:
    STATISTICS_MINIMUM=582
    STATISTICS_MAXIMUM=16752
    STATISTICS_MEAN=3845.8927675136
    STATISTICS_STDDEV=833.76760284161
    STATISTICS_VALID_PERCENT=82.1
Driver: GTiff/GeoTIFF
Files: S2B_MSIL2A_20240217T074009_R092_T36MZD_20240217T113805.tif
Size is 10980, 10980
Coordinate System is:
PROJCRS["WGS 84 / UTM zone 36S",
    BASEGEOGCRS["WGS 84",
        DATUM["World Geodetic System 1984",
            ELLIPSOID["WGS 84",6378137,298.257223563,
                LENGTHUNIT["metre",1]]],
        PRIMEM["Greenwich",0,
            ANGLEUNIT["degree",0.0174532925199433]],
        ID["EPSG",4326]],
    CONVERSION["UTM zone 36S",
        METHOD["Transverse Mercator",
            ID["EPSG",9807]],
        PARAMETER["Latitude of natural origin",0,
            ANGLEUNIT["degree",0.0174532925199433],
            ID["EPSG",8801]],
        PARAMETER["Longitude of natural origin",33,
            ANGLEUNIT["degree",0.0174532925199433],
            ID["EPSG",8802]],
        PARAMETER["Scale factor at natural origin",0.9996,
            SCALEUNIT["unity",1],
            ID["EPSG",8805]],
        PARAMETER["False easting",500000,
            LENGTHUNIT["metre",1],
            ID["EPSG",8806]],
        PARAMETER["False northing",10000000,
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
        AREA["Between 30°E and 36°E, southern hemisphere between 80°S and equator, onshore and offshore. Burundi. Eswatini (Swaziland). Kenya. Malawi. Mozambique. Rwanda. South Africa. Tanzania. Uganda. Zambia. Zimbabwe."],
        BBOX[-80,30,0,36]],
    ID["EPSG",32736]]
Data axis to CRS axis mapping: 1,2
Origin = (799980.000000000000000,9900040.000000000000000)
Pixel Size = (10.000000000000000,-10.000000000000000)
Metadata:
  AREA_OR_POINT=Area
Image Structure Metadata:
  LAYOUT=COG
  COMPRESSION=LERC_ZSTD
  LERC_VERSION=2.4
  INTERLEAVE=PIXEL
Corner Coordinates:
Upper Left  (  799980.000, 9900040.000) ( 35d41'42.64"E,  0d54'12.10"S)
Lower Left  (  799980.000, 9790240.000) ( 35d41'46.72"E,  1d53'44.32"S)
Upper Right (  909780.000, 9900040.000) ( 36d40'49.78"E,  0d54' 8.97"S)
Lower Right (  909780.000, 9790240.000) ( 36d40'55.34"E,  1d53'37.74"S)
Center      (  854880.000, 9845140.000) ( 36d11'18.62"E,  1d23'55.97"S)
Band 1 Block=512x512 Type=UInt16, ColorInterp=Gray
  Minimum=1.000, Maximum=17616.000, Mean=1587.841, StdDev=343.685
  NoData Value=0
  Overviews: 5490x5490, 2745x2745, 1373x1373, 687x687, 344x344
  Metadata:
    STATISTICS_MINIMUM=1
    STATISTICS_MAXIMUM=17616
    STATISTICS_MEAN=1587.8407275012
    STATISTICS_STDDEV=343.68515573305
    STATISTICS_VALID_PERCENT=82.87
Band 2 Block=512x512 Type=UInt16, ColorInterp=Undefined
  Minimum=1055.000, Maximum=17760.000, Mean=1863.807, StdDev=346.894
  NoData Value=0
  Overviews: 5490x5490, 2745x2745, 1373x1373, 687x687, 344x344
  Metadata:
    STATISTICS_MINIMUM=1055
    STATISTICS_MAXIMUM=17760
    STATISTICS_MEAN=1863.8069325
    STATISTICS_STDDEV=346.89404060533
    STATISTICS_VALID_PERCENT=82.87
Band 3 Block=512x512 Type=UInt16, ColorInterp=Undefined
  Minimum=312.000, Maximum=17792.000, Mean=1921.333, StdDev=466.307
  NoData Value=0
  Overviews: 5490x5490, 2745x2745, 1373x1373, 687x687, 344x344
  Metadata:
    STATISTICS_MINIMUM=312
    STATISTICS_MAXIMUM=17792
    STATISTICS_MEAN=1921.3333547921
    STATISTICS_STDDEV=466.3065641515
    STATISTICS_VALID_PERCENT=82.87
Band 4 Block=512x512 Type=UInt16, ColorInterp=Undefined
  Minimum=1074.000, Maximum=16960.000, Mean=3767.142, StdDev=624.381
  NoData Value=0
  Overviews: 5490x5490, 2745x2745, 1373x1373, 687x687, 344x344
  Metadata:
    STATISTICS_MINIMUM=1074
    STATISTICS_MAXIMUM=16960
    STATISTICS_MEAN=3767.1415608361
    STATISTICS_STDDEV=624.38065213934
    STATISTICS_VALID_PERCENT=82.87
Driver: GTiff/GeoTIFF
Files: S2B_MSIL2A_20240805T073619_R092_T36MZD_20240805T120513.tif
Size is 10980, 10980
Coordinate System is:
PROJCRS["WGS 84 / UTM zone 36S",
    BASEGEOGCRS["WGS 84",
        DATUM["World Geodetic System 1984",
            ELLIPSOID["WGS 84",6378137,298.257223563,
                LENGTHUNIT["metre",1]]],
        PRIMEM["Greenwich",0,
            ANGLEUNIT["degree",0.0174532925199433]],
        ID["EPSG",4326]],
    CONVERSION["UTM zone 36S",
        METHOD["Transverse Mercator",
            ID["EPSG",9807]],
        PARAMETER["Latitude of natural origin",0,
            ANGLEUNIT["degree",0.0174532925199433],
            ID["EPSG",8801]],
        PARAMETER["Longitude of natural origin",33,
            ANGLEUNIT["degree",0.0174532925199433],
            ID["EPSG",8802]],
        PARAMETER["Scale factor at natural origin",0.9996,
            SCALEUNIT["unity",1],
            ID["EPSG",8805]],
        PARAMETER["False easting",500000,
            LENGTHUNIT["metre",1],
            ID["EPSG",8806]],
        PARAMETER["False northing",10000000,
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
        AREA["Between 30°E and 36°E, southern hemisphere between 80°S and equator, onshore and offshore. Burundi. Eswatini (Swaziland). Kenya. Malawi. Mozambique. Rwanda. South Africa. Tanzania. Uganda. Zambia. Zimbabwe."],
        BBOX[-80,30,0,36]],
    ID["EPSG",32736]]
Data axis to CRS axis mapping: 1,2
Origin = (799980.000000000000000,9900040.000000000000000)
Pixel Size = (10.000000000000000,-10.000000000000000)
Metadata:
  AREA_OR_POINT=Area
Image Structure Metadata:
  LAYOUT=COG
  COMPRESSION=LERC_ZSTD
  LERC_VERSION=2.4
  INTERLEAVE=PIXEL
Corner Coordinates:
Upper Left  (  799980.000, 9900040.000) ( 35d41'42.64"E,  0d54'12.10"S)
Lower Left  (  799980.000, 9790240.000) ( 35d41'46.72"E,  1d53'44.32"S)
Upper Right (  909780.000, 9900040.000) ( 36d40'49.78"E,  0d54' 8.97"S)
Lower Right (  909780.000, 9790240.000) ( 36d40'55.34"E,  1d53'37.74"S)
Center      (  854880.000, 9845140.000) ( 36d11'18.62"E,  1d23'55.97"S)
Band 1 Block=512x512 Type=UInt16, ColorInterp=Gray
  Minimum=1.000, Maximum=14896.000, Mean=1563.454, StdDev=213.212
  NoData Value=0
  Overviews: 5490x5490, 2745x2745, 1373x1373, 687x687
  Metadata:
    STATISTICS_MINIMUM=1
    STATISTICS_MAXIMUM=14896
    STATISTICS_MEAN=1563.4536322604
    STATISTICS_STDDEV=213.21167930921
    STATISTICS_VALID_PERCENT=82.31
Band 2 Block=512x512 Type=UInt16, ColorInterp=Undefined
  Minimum=843.000, Maximum=15128.000, Mean=1793.265, StdDev=243.022
  NoData Value=0
  Overviews: 5490x5490, 2745x2745, 1373x1373, 687x687
  Metadata:
    STATISTICS_MINIMUM=843
    STATISTICS_MAXIMUM=15128
    STATISTICS_MEAN=1793.2650712355
    STATISTICS_STDDEV=243.0215869281
    STATISTICS_VALID_PERCENT=82.31
Band 3 Block=512x512 Type=UInt16, ColorInterp=Undefined
  Minimum=631.000, Maximum=15288.000, Mean=1885.323, StdDev=397.655
  NoData Value=0
  Overviews: 5490x5490, 2745x2745, 1373x1373, 687x687
  Metadata:
    STATISTICS_MINIMUM=631
    STATISTICS_MAXIMUM=15288
    STATISTICS_MEAN=1885.3227928439
    STATISTICS_STDDEV=397.65500137695
    STATISTICS_VALID_PERCENT=82.31
Band 4 Block=512x512 Type=UInt16, ColorInterp=Undefined
  Minimum=684.000, Maximum=16768.000, Mean=3472.658, StdDev=497.164
  NoData Value=0
  Overviews: 5490x5490, 2745x2745, 1373x1373, 687x687
  Metadata:
    STATISTICS_MINIMUM=684
    STATISTICS_MAXIMUM=16768
    STATISTICS_MEAN=3472.6581907661
    STATISTICS_STDDEV=497.16428226657
    STATISTICS_VALID_PERCENT=82.31
Driver: GTiff/GeoTIFF
Files: S2B_MSIL2A_20240805T073619_R092_T37MBU_20240805T120513.tif
Size is 10980, 10980
Coordinate System is:
PROJCRS["WGS 84 / UTM zone 37S",
    BASEGEOGCRS["WGS 84",
        DATUM["World Geodetic System 1984",
            ELLIPSOID["WGS 84",6378137,298.257223563,
                LENGTHUNIT["metre",1]]],
        PRIMEM["Greenwich",0,
            ANGLEUNIT["degree",0.0174532925199433]],
        ID["EPSG",4326]],
    CONVERSION["UTM zone 37S",
        METHOD["Transverse Mercator",
            ID["EPSG",9807]],
        PARAMETER["Latitude of natural origin",0,
            ANGLEUNIT["degree",0.0174532925199433],
            ID["EPSG",8801]],
        PARAMETER["Longitude of natural origin",39,
            ANGLEUNIT["degree",0.0174532925199433],
            ID["EPSG",8802]],
        PARAMETER["Scale factor at natural origin",0.9996,
            SCALEUNIT["unity",1],
            ID["EPSG",8805]],
        PARAMETER["False easting",500000,
            LENGTHUNIT["metre",1],
            ID["EPSG",8806]],
        PARAMETER["False northing",10000000,
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
        AREA["Between 36°E and 42°E, southern hemisphere between 80°S and equator, onshore and offshore. Kenya. Mozambique. Tanzania."],
        BBOX[-80,36,0,42]],
    ID["EPSG",32737]]
Data axis to CRS axis mapping: 1,2
Origin = (199980.000000000000000,9900040.000000000000000)
Pixel Size = (10.000000000000000,-10.000000000000000)
Metadata:
  AREA_OR_POINT=Area
Image Structure Metadata:
  LAYOUT=COG
  COMPRESSION=LERC_ZSTD
  LERC_VERSION=2.4
  INTERLEAVE=PIXEL
Corner Coordinates:
Upper Left  (  199980.000, 9900040.000) ( 36d18'16.07"E,  0d54'12.10"S)
Lower Left  (  199980.000, 9790240.000) ( 36d18'11.99"E,  1d53'44.32"S)
Upper Right (  309780.000, 9900040.000) ( 37d17'26.11"E,  0d54'14.27"S)
Lower Right (  309780.000, 9790240.000) ( 37d17'23.51"E,  1d53'48.87"S)
Center      (  254880.000, 9845140.000) ( 36d47'49.42"E,  1d24' 0.08"S)
Band 1 Block=512x512 Type=UInt16, ColorInterp=Gray
  Minimum=12.000, Maximum=19264.000, Mean=1571.461, StdDev=273.635
  NoData Value=0
  Overviews: 5490x5490, 2745x2745, 1373x1373, 687x687
  Metadata:
    STATISTICS_MINIMUM=12
    STATISTICS_MAXIMUM=19264
    STATISTICS_MEAN=1571.4609512752
    STATISTICS_STDDEV=273.63535605878
    STATISTICS_VALID_PERCENT=99.999
Band 2 Block=512x512 Type=UInt16, ColorInterp=Undefined
  Minimum=383.000, Maximum=18320.000, Mean=1775.729, StdDev=284.602
  NoData Value=0
  Overviews: 5490x5490, 2745x2745, 1373x1373, 687x687
  Metadata:
    STATISTICS_MINIMUM=383
    STATISTICS_MAXIMUM=18320
    STATISTICS_MEAN=1775.7285188402
    STATISTICS_STDDEV=284.6023614779
    STATISTICS_VALID_PERCENT=99.999
Band 3 Block=512x512 Type=UInt16, ColorInterp=Undefined
  Minimum=675.000, Maximum=17664.000, Mean=1860.777, StdDev=386.559
  NoData Value=0
  Overviews: 5490x5490, 2745x2745, 1373x1373, 687x687
  Metadata:
    STATISTICS_MINIMUM=675
    STATISTICS_MAXIMUM=17664
    STATISTICS_MEAN=1860.7770162081
    STATISTICS_STDDEV=386.55897234011
    STATISTICS_VALID_PERCENT=99.999
Band 4 Block=512x512 Type=UInt16, ColorInterp=Undefined
  Minimum=1015.000, Maximum=16977.000, Mean=3401.336, StdDev=500.650
  NoData Value=0
  Overviews: 5490x5490, 2745x2745, 1373x1373, 687x687
  Metadata:
    STATISTICS_MINIMUM=1015
    STATISTICS_MAXIMUM=16977
    STATISTICS_MEAN=3401.3359466956
    STATISTICS_STDDEV=500.65039346782
    STATISTICS_VALID_PERCENT=100
Driver: GTiff/GeoTIFF
Files: S2B_MSIL2A_20241014T073759_R092_T36MZD_20241014T102258.tif
Size is 10980, 10980
Coordinate System is:
PROJCRS["WGS 84 / UTM zone 36S",
    BASEGEOGCRS["WGS 84",
        DATUM["World Geodetic System 1984",
            ELLIPSOID["WGS 84",6378137,298.257223563,
                LENGTHUNIT["metre",1]]],
        PRIMEM["Greenwich",0,
            ANGLEUNIT["degree",0.0174532925199433]],
        ID["EPSG",4326]],
    CONVERSION["UTM zone 36S",
        METHOD["Transverse Mercator",
            ID["EPSG",9807]],
        PARAMETER["Latitude of natural origin",0,
            ANGLEUNIT["degree",0.0174532925199433],
            ID["EPSG",8801]],
        PARAMETER["Longitude of natural origin",33,
            ANGLEUNIT["degree",0.0174532925199433],
            ID["EPSG",8802]],
        PARAMETER["Scale factor at natural origin",0.9996,
            SCALEUNIT["unity",1],
            ID["EPSG",8805]],
        PARAMETER["False easting",500000,
            LENGTHUNIT["metre",1],
            ID["EPSG",8806]],
        PARAMETER["False northing",10000000,
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
        AREA["Between 30°E and 36°E, southern hemisphere between 80°S and equator, onshore and offshore. Burundi. Eswatini (Swaziland). Kenya. Malawi. Mozambique. Rwanda. South Africa. Tanzania. Uganda. Zambia. Zimbabwe."],
        BBOX[-80,30,0,36]],
    ID["EPSG",32736]]
Data axis to CRS axis mapping: 1,2
Origin = (799980.000000000000000,9900040.000000000000000)
Pixel Size = (10.000000000000000,-10.000000000000000)
Metadata:
  AREA_OR_POINT=Area
Image Structure Metadata:
  LAYOUT=COG
  COMPRESSION=LERC_ZSTD
  LERC_VERSION=2.4
  INTERLEAVE=PIXEL
Corner Coordinates:
Upper Left  (  799980.000, 9900040.000) ( 35d41'42.64"E,  0d54'12.10"S)
Lower Left  (  799980.000, 9790240.000) ( 35d41'46.72"E,  1d53'44.32"S)
Upper Right (  909780.000, 9900040.000) ( 36d40'49.78"E,  0d54' 8.97"S)
Lower Right (  909780.000, 9790240.000) ( 36d40'55.34"E,  1d53'37.74"S)
Center      (  854880.000, 9845140.000) ( 36d11'18.62"E,  1d23'55.97"S)
Band 1 Block=512x512 Type=UInt16, ColorInterp=Gray
  Minimum=112.000, Maximum=14144.000, Mean=1762.425, StdDev=340.040
  NoData Value=0
  Overviews: 5490x5490, 2745x2745, 1373x1373, 687x687
  Metadata:
    STATISTICS_MINIMUM=112
    STATISTICS_MAXIMUM=14144
    STATISTICS_MEAN=1762.4250560058
    STATISTICS_STDDEV=340.03985931983
    STATISTICS_VALID_PERCENT=82.75
Band 2 Block=512x512 Type=UInt16, ColorInterp=Undefined
  Minimum=986.000, Maximum=16480.000, Mean=2029.992, StdDev=363.836
  NoData Value=0
  Overviews: 5490x5490, 2745x2745, 1373x1373, 687x687
  Metadata:
    STATISTICS_MINIMUM=986
    STATISTICS_MAXIMUM=16480
    STATISTICS_MEAN=2029.9915600032
    STATISTICS_STDDEV=363.83633758365
    STATISTICS_VALID_PERCENT=82.75
Band 3 Block=512x512 Type=UInt16, ColorInterp=Undefined
  Minimum=635.000, Maximum=17200.000, Mean=2275.200, StdDev=512.262
  NoData Value=0
  Overviews: 5490x5490, 2745x2745, 1373x1373, 687x687
  Metadata:
    STATISTICS_MINIMUM=635
    STATISTICS_MAXIMUM=17200
    STATISTICS_MEAN=2275.1998137547
    STATISTICS_STDDEV=512.26155047341
    STATISTICS_VALID_PERCENT=82.75
Band 4 Block=512x512 Type=UInt16, ColorInterp=Undefined
  Minimum=1057.000, Maximum=16512.000, Mean=3567.400, StdDev=529.214
  NoData Value=0
  Overviews: 5490x5490, 2745x2745, 1373x1373, 687x687
  Metadata:
    STATISTICS_MINIMUM=1057
    STATISTICS_MAXIMUM=16512
    STATISTICS_MEAN=3567.4003772297
    STATISTICS_STDDEV=529.21372672785
    STATISTICS_VALID_PERCENT=82.75
Driver: GTiff/GeoTIFF
Files: S2B_MSIL2A_20241014T073759_R092_T37MBU_20241014T102258.tif
Size is 10980, 10980
Coordinate System is:
PROJCRS["WGS 84 / UTM zone 37S",
    BASEGEOGCRS["WGS 84",
        DATUM["World Geodetic System 1984",
            ELLIPSOID["WGS 84",6378137,298.257223563,
                LENGTHUNIT["metre",1]]],
        PRIMEM["Greenwich",0,
            ANGLEUNIT["degree",0.0174532925199433]],
        ID["EPSG",4326]],
    CONVERSION["UTM zone 37S",
        METHOD["Transverse Mercator",
            ID["EPSG",9807]],
        PARAMETER["Latitude of natural origin",0,
            ANGLEUNIT["degree",0.0174532925199433],
            ID["EPSG",8801]],
        PARAMETER["Longitude of natural origin",39,
            ANGLEUNIT["degree",0.0174532925199433],
            ID["EPSG",8802]],
        PARAMETER["Scale factor at natural origin",0.9996,
            SCALEUNIT["unity",1],
            ID["EPSG",8805]],
        PARAMETER["False easting",500000,
            LENGTHUNIT["metre",1],
            ID["EPSG",8806]],
        PARAMETER["False northing",10000000,
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
        AREA["Between 36°E and 42°E, southern hemisphere between 80°S and equator, onshore and offshore. Kenya. Mozambique. Tanzania."],
        BBOX[-80,36,0,42]],
    ID["EPSG",32737]]
Data axis to CRS axis mapping: 1,2
Origin = (199980.000000000000000,9900040.000000000000000)
Pixel Size = (10.000000000000000,-10.000000000000000)
Metadata:
  AREA_OR_POINT=Area
Image Structure Metadata:
  LAYOUT=COG
  COMPRESSION=LERC_ZSTD
  LERC_VERSION=2.4
  INTERLEAVE=PIXEL
Corner Coordinates:
Upper Left  (  199980.000, 9900040.000) ( 36d18'16.07"E,  0d54'12.10"S)
Lower Left  (  199980.000, 9790240.000) ( 36d18'11.99"E,  1d53'44.32"S)
Upper Right (  309780.000, 9900040.000) ( 37d17'26.11"E,  0d54'14.27"S)
Lower Right (  309780.000, 9790240.000) ( 37d17'23.51"E,  1d53'48.87"S)
Center      (  254880.000, 9845140.000) ( 36d47'49.42"E,  1d24' 0.08"S)
Band 1 Block=512x512 Type=UInt16, ColorInterp=Gray
  Minimum=148.000, Maximum=19008.000, Mean=1729.175, StdDev=369.936
  NoData Value=0
  Overviews: 5490x5490, 2745x2745, 1373x1373, 687x687
  Metadata:
    STATISTICS_MINIMUM=148
    STATISTICS_MAXIMUM=19008
    STATISTICS_MEAN=1729.175299576
    STATISTICS_STDDEV=369.93563082195
    STATISTICS_VALID_PERCENT=100
Band 2 Block=512x512 Type=UInt16, ColorInterp=Undefined
  Minimum=618.000, Maximum=18112.000, Mean=1967.192, StdDev=376.943
  NoData Value=0
  Overviews: 5490x5490, 2745x2745, 1373x1373, 687x687
  Metadata:
    STATISTICS_MINIMUM=618
    STATISTICS_MAXIMUM=18112
    STATISTICS_MEAN=1967.1917682921
    STATISTICS_STDDEV=376.94259741521
    STATISTICS_VALID_PERCENT=100
Band 3 Block=512x512 Type=UInt16, ColorInterp=Undefined
  Minimum=893.000, Maximum=17456.000, Mean=2196.439, StdDev=486.943
  NoData Value=0
  Overviews: 5490x5490, 2745x2745, 1373x1373, 687x687
  Metadata:
    STATISTICS_MINIMUM=893
    STATISTICS_MAXIMUM=17456
    STATISTICS_MEAN=2196.4393209379
    STATISTICS_STDDEV=486.94324823049
    STATISTICS_VALID_PERCENT=100
Band 4 Block=512x512 Type=UInt16, ColorInterp=Undefined
  Minimum=1009.000, Maximum=16848.000, Mean=3430.167, StdDev=556.247
  NoData Value=0
  Overviews: 5490x5490, 2745x2745, 1373x1373, 687x687
  Metadata:
    STATISTICS_MINIMUM=1009
    STATISTICS_MAXIMUM=16848
    STATISTICS_MEAN=3430.1671767346
    STATISTICS_STDDEV=556.2470963179
    STATISTICS_VALID_PERCENT=100