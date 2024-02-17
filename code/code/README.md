cache info:

`getconf -a | grep CACHE`, `lscpu`

run gdal_program: 

`make && ./gdal_program`

cache locality: https://www.cs.cornell.edu/courses/cs3110/2012sp/lectures/lec25-locality/lec25.html






ALGORITHMS:

### Supports Accessing Any Random Element

These algorithms allow random access to specific blocks or segments of compressed data, but not necessarily to an individual element within these blocks without decompressing the entire block:

1. **TurboPFOR** ([C Implementation](https://github.com/powturbo/TurboPFor-Integer-Compression)): Supports block-wise random access.
2. **Brotli** ([C Implementation](https://github.com/google/brotli)): Can access compressed data in chunks, but individual elements require decompression of the chunk.
3. **Zstandard (Zstd)** ([C Implementation](https://github.com/facebook/zstd)): Supports decompressing from a specific point in the compressed data, but accessing a specific element still requires decompression from that point onward.
4. **DEFLATE** (Used in zlib): Allows random access within certain constraints but requires decompression from the start of a block or segment.

### Supports Accessing a Specific Element Directly

These algorithms allow direct random access to specific elements within the compressed data without needing to decompress large portions or blocks:

1. **Roaring Bitmaps** ([C Implementation](https://github.com/RoaringBitmap/CRoaring)): Designed for efficient random access to individual elements in sets of integers.
2. **Run-Length Encoding (RLE)**: Depending on implementation, can allow direct access to elements, especially in scenarios with repeated values.
3. **Dictionary Encoding**: Can support direct element access if implemented with a mapping that allows for random lookups.
4. **Delta Encoding**: Might allow direct access to elements in some implementations, especially in time-series data where deltas are consistently applied.
5. **Frame of Reference Encoding**: Can be designed to support direct access to elements based on a reference value.
6. **Prefix Compression**: In some cases, can allow direct access, especially in structured data like database indices.

### Limited or No Direct Support for Random Access

These algorithms generally do not support random access, and accessing any specific part of the data usually requires sequential decompression from a certain point:

1. **LZ4** ([C Implementation](https://github.com/lz4/lz4)): Optimized for sequential access; random access requires decompressing from a previous known point.
2. **Snappy** ([C Implementation](https://github.com/google/snappy)): Similar to LZ4, optimized for speed in sequential access.
3. **QuickLZ** ([C Implementation](https://www.quicklz.com/)): Focused on fast compression and decompression, but not designed for random access.
4. **LZF (LibFastLZ)** ([C Implementation](https://github.com/ning/compress)): Prioritizes speed, but does not inherently support random access.
5. **Bzip2** ([C Implementation](https://sourceware.org/bzip2/)): Typically requires sequential decompression.
6. **XZ/LZMA** ([C Implementation](https://tukaani.org/xz/)): Designed for high compression ratio, but not for random access.
7. **PFOR (Partitioned Frame of Reference)**: Generally requires decompressing blocks to access data.

---

### Categorized List of Compression Algorithms

#### Very High-Speed Compression

- **LZ4** ([C Implementation](https://github.com/lz4/lz4))
- **Snappy** ([C Implementation](https://github.com/google/snappy))
- **QuickLZ** ([C Implementation](https://www.quicklz.com/))
- **LZF (LibFastLZ)** ([C Implementation](https://github.com/ning/compress))
- **Roaring Bitmaps** ([C Implementation](https://github.com/RoaringBitmap/CRoaring))
- **TurboPFOR** ([C Implementation](https://github.com/powturbo/TurboPFor-Integer-Compression))

#### High-Speed Compression

- **Zstandard (Zstd)** ([C Implementation](https://github.com/facebook/zstd))
- **Brotli** ([C Implementation](https://github.com/google/brotli))
- **Dictionary Encoding**
- **Frame of Reference Encoding**

#### Moderate-Speed Compression

- **Bzip2** ([C Implementation](https://sourceware.org/bzip2/))
- **PFOR (Partitioned Frame of Reference)**
- **Run-Length Encoding (RLE)**
- **Delta Encoding**
- **Prefix Compression**

#### Slow-Speed Compression

- **XZ/LZMA** ([C Implementation](https://tukaani.org/xz/))

#### Supports Windowed Decompression

- **Zstandard (Zstd)**
- **Brotli**
- **TurboPFOR**

#### Limited Support for Windowed Decompression

- **LZ4**
- **Snappy**
- **QuickLZ**
- **LZF (LibFastLZ)**
- **Bzip2**
- **XZ/LZMA**
- **PFOR**

#### No Direct Support for Windowed Decompression

- **Roaring Bitmaps** (Not applicable for traditional compression)
- **Dictionary Encoding**
- **Frame of Reference Encoding**
- **Run-Length Encoding (RLE)**
- **Delta Encoding**
- **Prefix Compression**

---

### Speed (Fast to Slow)

1. **LZ4, Snappy, QuickLZ, LZF (LibFastLZ), LZJB**:
   - Very high speed.
   - Optimized for fast compression and decompression.

2. **Zstandard (Zstd), Brotli, LZRW, ShrinkIt (SHK)**:
   - Moderate to high speed.
   - Balance between speed and compression ratio.

3. **DEFLATE, Bzip2, XZ/LZMA, PPMd**:
   - Moderate to slow speed.
   - Generally slower due to higher compression ratio.

4. **Run-Length Encoding (RLE), Dictionary Encoding, Prefix Compression, Delta Encoding, Bit-Packing, Frame of Reference Encoding, Huffman Encoding**:
   - Speed varies, often fast but depends on data characteristics.

### Compression Ratio (High to Low)

1. **XZ/LZMA, Bzip2, PPMd**:
   - Very high compression ratio.
   - More suitable for compressing data with less concern for speed.

2. **Zstandard (Zstd), Brotli, DEFLATE**:
   - High compression ratio.
   - Good balance between compression ratio and speed.

3. **LZ4, Snappy, QuickLZ, LZF (LibFastLZ), LZJB, LZRW, ShrinkIt (SHK)**:
   - Moderate compression ratio.
   - Focus more on speed than maximum compression.

4. **Run-Length Encoding (RLE), Dictionary Encoding, Prefix Compression, Delta Encoding, Bit-Packing, Frame of Reference Encoding, Huffman Encoding**:
   - Compression ratio depends heavily on the data type and characteristics.

### Windowed Decompression Support

1. **Supports Windowed Decompression**:
   - **Zstandard (Zstd)**: Supports windowed decompression with certain configurations.
   - **Brotli**: Offers windowed decompression capabilities.

2. **Limited or Conditional Support**:
   - **DEFLATE, Bzip2, XZ/LZMA, PPMd**: Not inherently designed for windowed decompression, but can be adapted in some cases.
   - **Dictionary Encoding, Delta Encoding, Huffman Encoding**: Support depends on implementation and data structure.

3. **No Direct Support or Not Applicable**:
   - **LZ4, Snappy, QuickLZ, LZF (LibFastLZ), LZJB, LZRW, ShrinkIt (SHK)**: Generally not designed for windowed decompression.
   - **Run-Length Encoding (RLE), Prefix Compression, Bit-Packing, Frame of Reference Encoding**: Typically do not support windowed decompression directly.