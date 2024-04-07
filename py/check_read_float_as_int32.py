from osgeo import gdal
import numpy as np
import argparse

def compare_float32_and_int32_reading(tiff_file):
    dataset = gdal.Open(tiff_file, gdal.GA_ReadOnly)
    if not dataset:
        print("Failed to open file:", tiff_file)
        return

    band = dataset.GetRasterBand(1)
    data_float32 = band.ReadAsArray(0, 0, band.XSize, band.YSize, buf_type=gdal.GDT_Float32)
    data_int32 = band.ReadAsArray(0, 0, band.XSize, band.YSize, buf_type=gdal.GDT_Int32)
    if np.array_equal(data_float32, data_int32):
        print("Reading as float32 and int32 yields the same data.")
    else:
        print("There is a difference between the data read as float32 and int32.")

    del dataset

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Compare float32 and int32 reading of GeoTIFF")
    parser.add_argument("tiff_file", help="Path to the GeoTIFF file")
    args = parser.parse_args()
    
    compare_float32_and_int32_reading(args.tiff_file)
