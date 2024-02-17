from osgeo import gdal
import sys
import numpy as np
import argparse

def check_data_loss(tiff_file):
    # Open the dataset
    dataset = gdal.Open(tiff_file, gdal.GA_ReadOnly)
    if not dataset:
        print("Failed to open file:", tiff_file)
        sys.exit(1)

    band = dataset.GetRasterBand(1)
    data_type = gdal.GetDataTypeName(band.DataType)

    # Read the data
    data = band.ReadAsArray()
    
    # Check if data type is float32
    if data_type == 'Float32':
        # Cast data to int32
        int_data = data.astype(np.int32)

        # Compare original and cast data
        if np.array_equal(data, int_data):
            print("No data loss from casting.")
        else:
            print("Data loss detected in casting.")
    else:
        print(f"Data type is not Float32, but {data_type}. Cannot check for data loss in casting to int32.")

    # Cleanup
    del dataset

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Check for data loss when casting float32 GeoTIFF to int32")
    parser.add_argument("tiff_file", help="Path to the GeoTIFF file")
    args = parser.parse_args()
    
    check_data_loss(args.tiff_file)
