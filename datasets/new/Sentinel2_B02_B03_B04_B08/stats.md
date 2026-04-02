/maps/omsst2/diss/others/sentinel2/

20 files: Sentinel2_{B02,B03,B04,B08}_{nyc,iowa,himalayas,sahara,amazon}.tif

| Band | Resolution | Size (px) | Type | NoData | Value range |
|------|------------|-----------|------|--------|-------------|
| B02 (blue) | 10 m | 10980×10980 | UInt16 | 0 | 0–10000+ |
| B03 (green)| 10 m | 10980×10980 | UInt16 | 0 | 0–10000+ |
| B04 (red)  | 10 m | 10980×10980 | UInt16 | 0 | 0–10000+ |
| B08 (NIR)  | 10 m | 10980×10980 | UInt16 | 0 | 0–10000+ |

CRS: UTM (zone varies by region)
Projection: WGS 84 / UTM zone N or S (EPSG:326xx / 327xx)

Full Sentinel-2 tile (~110 km × 110 km). Values are surface reflectance × 10000
(i.e. 1000 = 10.0% reflectance). Values >10000 can occur over bright targets.
NoData (0) marks tile edges and masked pixels.
