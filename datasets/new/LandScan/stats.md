/maps/omsst2/diss/others/landscan/

| File | Size (px) | Type | NoData | Min | Max | Valid % |
|------|-----------|------|--------|-----|-----|---------|
| LandScan_nyc.tif       | 2400×2400 | Int32 | -2147483647 | 0 | — | ~96 |
| LandScan_iowa.tif      | 2400×2400 | Int32 | -2147483647 | 0 | — | ~96 |
| LandScan_himalayas.tif | 2400×2400 | Int32 | -2147483647 | 0 | — | ~99 |
| LandScan_sahara.tif    | 2400×2400 | Int32 | -2147483647 | 0 | — | ~99 |
| LandScan_amazon.tif    | 2400×2400 | Int32 | -2147483647 | 0 | — | ~99 |

Pixel size: 0.008333° × 0.008333° (~1 km at equator)
CRS: EPSG:4326 (WGS84 geographic)

Distribution is extremely sparse — most pixels are 0 (unpopulated areas).
NYC and Iowa have ~96% valid (minor boundary nodata from LandScan global extent).
Population counts are non-negative; nodata (-2147483647) marks outside-extent pixels.
NOTE: gdalinfo may report type as Int32 rather than UInt32 — functionally equivalent
since all values are non-negative. Both are 32-bit integer.
