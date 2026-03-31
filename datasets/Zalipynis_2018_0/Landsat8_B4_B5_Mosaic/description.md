https://dl.acm.org/doi/10.14778/3231751.3231754
ChronosDB: distributed, file based, geospatial array DBMS

To make this section clearer, we form a dataset as a running example: two mosaics of 4 × 8 Landsat 8 scenes, bands 4 (visible red, red) and 5 (near-infrared, nir) [33], paths 191–198, rows 24–27, 01–15 July 2015, GeoTIFF. Both in ChronosDB and SciDB, the mosaics are two 24937 × 38673 arrays: red and nir. An array size is ≈ 1.4 GB in SciDB and ≈ 1.6 GB in ChronosDB. Each array has ≈ 23% of empty cells (NA): the mosaic areas not covered by the scenes, fig. 1.

