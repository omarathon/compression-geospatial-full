https://www.gaia-gis.it/fossil/librasterlite2/wiki?name=benchmarks+%282019+update%29#:~:text=Test%20%239%20%2D%20Datagrid%20Raster%20Coverage

Quick assessment:
this test is based on the very popular ETOPO1 global relief model of Earth's surface published by [NOAA](https://www.ngdc.noaa.gov/mgg/global/global.html)
For this test ZSTD and DEFLATE are basicly on par with each other; each achieving better results for different areas.
LZ4 confirms to be fast but unable to score a good compression ratio.
LZMA continues to achieve an impressive compression ratio, but still a barely tolerable slowness during both compression and decompression.