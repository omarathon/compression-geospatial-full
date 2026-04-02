/maps/omsst2/diss/others/srtm_highres/

Files are individual NASADEM 1°×1° tiles, named SRTM_HighRes_{region}_{tile_id}.tif.
Each region has 1–4 tiles; tile IDs are NASADEM standard (e.g. n40w074).

| Region | Tiles | Tile size (px) | Type | NoData | Approx range (m) |
|--------|-------|----------------|------|--------|------------------|
| nyc       | ~1–2 | 3601×3601 | Int16 | -32768 | -12 to 1927 |
| iowa      | ~2   | 3601×3601 | Int16 | -32768 |  207 to  531 |
| himalayas | ~1–2 | 3601×3601 | Int16 | -32768 |  975 to 8752 |
| sahara    | ~1   | 3601×3601 | Int16 | -32768 |  320 to  866 |
| amazon    | ~1–2 | 3601×3601 | Int16 | -32768 |  -28 to  151 |

Pixel size: 0.000278° × 0.000278° (~30 m at equator)
CRS: EPSG:4326 (WGS84 geographic)

Tiles overlap slightly (shared edge pixels) — this is normal for NASADEM.
