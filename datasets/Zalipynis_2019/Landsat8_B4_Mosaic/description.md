https://dl.acm.org/doi/10.1109/IDAACS.2019.8924326
Evaluating Array DBMS Compression Techniques for Big Environmental Datasets

As a representative dataset, we selected Landsat 8 satellite scenes. These are one of the most popular satellite datasets to date: Amazon and Google provide Landsat data via their commercial Clouds [54, 55]. We considered a mosaic of 4×4 Landsat 8 scenes: band 4, paths 195–198, rows 24–27, 02–11 July 2015, ≈ 2 GB in total [56]. The average cloud cover for the scenes is 23.46%. All scenes were projected into UTM zone 31N. The mosaic results in a 2-d array shaped 24937 × 22855 both in SciDB and ChronosDB.

