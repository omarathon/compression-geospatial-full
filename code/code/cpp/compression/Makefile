CXX = g++
# CXXFLAGS = -Wall -O2 '-msse4.1' '-mavx512f' -fopenmp -I./external -I./external/MaskedVByte/include -I./external/streamvbyte/include -I./external/2i_bench/external -I./external/2i_bench/include  $(shell gdal-config --cflags)
CXXFLAGS = -Wall -O2 '-msse4.1' '-mavx512f' -fopenmp -I./external -I./external/MaskedVByte/include -I./external/streamvbyte/include -I./external/2i_bench/external -I./external/2i_bench/include -I./external/TurboPFor/include -I./external/FrameOfReference/include -I./external/simdcomp/include -I./external/libmorton/include/libmorton  $(shell gdal-config --cflags)
# LDFLAGS = -L./external/FastPFor/build -lFastPFOR -L./external/MaskedVByte -lmaskedvbyte -L./external/2i_bench/build -lDS2ILib -L./external/2i_bench/build/external -lstreamvbyte
LDFLAGS = -L./external/FastPFor/build -lFastPFOR -L./external/MaskedVByte -lmaskedvbyte -L./external/streamvbyte/build -lstreamvbyte -L./external/TurboPFor -lic -lm -L./external/FrameOfReference/release -lFrameOfReference -L./external/simdcomp -lsimdcomp

# Individual libraries
LIBS=$(shell gdal-config --libs)
LIBS_DEFLATE = -lz
LIBS_LZMA = -llzma
LIBS_LZ4 = -llz4
LIBS_ZSTD = -lzstd

# Targets
all: deflate lzma lz4 zstd

bench_tiff_comp: bench_tiff_comp.cpp
	$(CXX) $(CXXFLAGS) -o bench_tiff_comp bench_tiff_comp.cpp $(LIBS) $(LDFLAGS) $(LIBS_DEFLATE) $(LIBS_LZMA) $(LIBS_LZ4) $(LIBS_ZSTD) -Wl,-rpath,./external/MaskedVByte

random_access_comp: random_access_comp.cpp
	$(CXX) $(CXXFLAGS) -o random_access_comp random_access_comp.cpp $(LIBS) $(LDFLAGS) $(LIBS_DEFLATE) $(LIBS_LZMA) $(LIBS_LZ4) $(LIBS_ZSTD) -Wl,-rpath,./external/MaskedVByte

bench: bench.cpp
	$(CXX) $(CXXFLAGS) -o bench bench.cpp $(LDFLAGS) $(LIBS_DEFLATE) $(LIBS_LZMA) $(LIBS_LZ4) $(LIBS_ZSTD) -Wl,-rpath,./external/MaskedVByte

deflate: deflate.cpp
	$(CXX) $(CXXFLAGS) -o deflate deflate.cpp $(LDFLAGS) $(LIBS_DEFLATE)

lzma: lzma.cpp
	$(CXX) $(CXXFLAGS) -o lzma lzma.cpp $(LDFLAGS) $(LIBS_LZMA)

lz4: lz4.cpp
	$(CXX) $(CXXFLAGS) -o lz4 lz4.cpp $(LDFLAGS) $(LIBS_LZ4)

zstd: zstd.cpp
	$(CXX) $(CXXFLAGS) -o zstd zstd.cpp $(LDFLAGS) $(LIBS_ZSTD)

fastpfor_example: fastpfor_example.cpp
	$(CXX) $(CXXFLAGS) -o fastpfor_example fastpfor_example.cpp $(LDFLAGS)

maskedvbyte_example: maskedvbyte_example.cpp
	$(CXX) $(CXXFLAGS) -o maskedvbyte_example maskedvbyte_example.cpp $(LDFLAGS) -Wl,-rpath,./external/MaskedVByte

clean:
	rm -f deflate lzma lz4 zstd fastpfor_example maskedvbyte_example
