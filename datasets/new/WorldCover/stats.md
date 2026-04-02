/maps/omsst2/diss/others/worldcover/

5 files: WorldCover_{region}_{tile_id}.tif

| File region | Size (px)   | Type | NoData | Classes present | Notes |
|-------------|-------------|------|--------|-----------------|-------|
| nyc         | 36000×36000 | Byte | 0 | 10,20,30,40,50,80 | urban, crops, water |
| iowa        | 36000×36000 | Byte | 0 | 10,30,40,80    | mostly crops/grassland |
| himalayas   | 36000×36000 | Byte | 0 | 10,20,30,60,70 | bare, shrub, snow |
| sahara      | 36000×36000 | Byte | 0 | 60             | nearly all class 60 (bare) |
| amazon      | 36000×36000 | Byte | 0 | 10,80,90       | mostly trees, water |

Pixel size: 0.000099° × 0.000099° (~10 m at equator)
CRS: EPSG:4326 (WGS84 geographic)

Tiles are 3°×3°. Values: 10=trees, 20=shrubland, 30=grassland, 40=cropland,
50=built-up, 60=bare/sparse, 70=snow/ice, 80=water, 90=herbaceous wetland,
95=mangroves, 100=moss/lichen.
Sahara collage appears near-black — expected, class 60 value (60) is dark grey.
