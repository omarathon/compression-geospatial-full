wget https://www.ngdc.noaa.gov/mgg/global/relief/ETOPO1/data/ice_surface/grid_registered/georeferenced_tiff/ETOPO1_Ice_g_geotiff.zip
unzip ETOPO1_Ice_g_geotiff.zip

fix invalid NoData value:

gdal_edit.py -a_nodata -32768 ETOPO1_Ice_g_geotiff.tif