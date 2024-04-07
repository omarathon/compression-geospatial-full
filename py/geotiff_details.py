from osgeo import gdal
import sys
import argparse
import numpy as np
import pandas as pd

def print_geotiff_details(tiff_file):
    dataset = gdal.Open(tiff_file, gdal.GA_ReadOnly)
    if not dataset:
        print("Failed to open file:", tiff_file)
        sys.exit(1)

    width = dataset.RasterXSize
    height = dataset.RasterYSize

    band = dataset.GetRasterBand(1)
    data_type = gdal.GetDataTypeName(band.DataType)
    data = band.ReadAsArray()
    data_series = pd.Series(data.flatten())

    min_val = np.min(data)
    max_val = np.max(data)
    num_unique_values = data_series.nunique()

    print("GeoTIFF details:")
    print("File:", tiff_file)
    print("Dimensions: {} x {}".format(width, height))
    print("Data type:", data_type)
    print("Minimum value:", min_val)
    print("Maximum value:", max_val)
    print("Unique values:", num_unique_values)

    del dataset

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Print details of a GeoTIFF file")
    parser.add_argument("tiff_file", help="Path to the GeoTIFF file")
    args = parser.parse_args()
    
    print_geotiff_details(args.tiff_file)
