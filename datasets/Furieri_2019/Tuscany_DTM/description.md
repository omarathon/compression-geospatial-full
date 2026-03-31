https://www.gaia-gis.it/fossil/librasterlite2/wiki?name=benchmarks+%282019+update%29#:~:text=Test%20%238%20%2D%20Datagrid%20Raster%20Coverage

Quick assessment:
this test is based on a huge ASCII Grid (DTM, 10m x 10m cell size).
The original dataset is the Orographic DTM 10x10 published by [Tuscany](http://www502.regione.toscana.it/geoscopio/cartoteca.html)
this specific test shows a slight superiority of ZSTD over DEFLATE; it's able to achieve a better compression ratio and it's faster during compression and decompression.
LZ4 confirms to be fast but unable to score a good compression ratio.
LZMA continues to achieve an impressive compression ratio, but still a barely tolerable slowness during both compression and decompression.
