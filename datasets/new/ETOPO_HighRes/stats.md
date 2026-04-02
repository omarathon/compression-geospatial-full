/maps/omsst2/diss/others/etopo_highres_quant/

| File | Size (px) | Type | NoData | Min | Max | Valid % |
|------|-----------|------|--------|-----|-----|---------|
| ETOPO_HighRes_nyc.tif       | 4800×4800 | Int32 | -2147483647 | — | — | 100 |
| ETOPO_HighRes_iowa.tif      | 4800×4800 | Int32 | -2147483647 | — | — | 100 |
| ETOPO_HighRes_himalayas.tif | 4800×4800 | Int32 | -2147483647 | — | — | 100 |
| ETOPO_HighRes_sahara.tif    | 4800×4800 | Int32 | -2147483647 | — | — | 100 |
| ETOPO_HighRes_amazon.tif    | 4800×4800 | Int32 | -2147483647 | — | — | 100 |
| ETOPO_HighRes_ocean.tif     | 4800×4800 | Int32 | -2147483647 | — | — | 100 |

Pixel size: 0.004167° × 0.004167° (~450 m at equator)
CRS: EPSG:4326 (WGS84 geographic)

Values are in millimetres (×1000 fixed-point quantization applied via gdal_calc.py).
Pre-quantization Float32 range example: nyc max ≈ 1932.8 m → stored as 1932800.
NOTE: if the quantization step was not applied, values will be in metres (Float32 range)
despite the Int32 type. Verify: a plausible mm value for the Himalayas peak would be
~8848000; a metres value would be ~8848. Check gdalinfo -stats.
