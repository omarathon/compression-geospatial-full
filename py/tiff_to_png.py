from osgeo import gdal
import sys

def convert_tiff_to_png(tiff_file, png_file):
    dataset = gdal.Open(tiff_file, gdal.GA_ReadOnly)
    if not dataset:
        print("Failed to open file:", tiff_file)
        sys.exit(1)

    driver = gdal.GetDriverByName('PNG')
    out_dataset = driver.CreateCopy(png_file, dataset, 0)

    del out_dataset
    del dataset
    print("File converted to PNG:", png_file)

if __name__ == "__main__":
    tiff_file = "/home/omar/diss/geotiffs/accessibility.tif"
    png_file = "/home/omar/diss/geotiffs/accessibility.png"
    convert_tiff_to_png(tiff_file, png_file)
