CXX = g++
CXXFLAGS = -cpp -Wall -O3 '-msse4.1' '-mavx512f' '-mbmi2' -fopenmp -I./external -I./external/MaskedVByte/include -I./external/streamvbyte/include -I./external/2i_bench/external -I./external/2i_bench/include -I./external/TurboPFor/include -I./external/FrameOfReference/include -I./external/simdcomp/include -I./external/libmorton/include/libmorton  $(shell gdal-config --cflags)
LDFLAGS = -L./external/FastPFor/build -lFastPFOR -L./external/MaskedVByte -lmaskedvbyte -L./external/streamvbyte/build -lstreamvbyte -L./external/TurboPFor -lic -lm -L./external/FrameOfReference/release -lFrameOfReference -L./external/simdcomp -lsimdcomp

LIBS=$(shell gdal-config --libs)
LIBS_DEFLATE = -lz
LIBS_LZMA = -llzma
LIBS_LZ4 = -llz4
LIBS_ZSTD = -lzstd

all: test_comp test_remappings bench_comp bench_pipeline

test_comp: test_comp.cpp
	$(CXX) $(CXXFLAGS) -o test_comp test_comp.cpp $(LDFLAGS) $(LIBS_DEFLATE) $(LIBS_LZMA) $(LIBS_LZ4) $(LIBS_ZSTD) -Wl,-rpath,./external/MaskedVByte

test_remappings: test_remappings.cpp
	$(CXX) $(CXXFLAGS) -o test_remappings test_remappings.cpp

bench_comp: bench_comp.cpp
	$(CXX) $(CXXFLAGS) -o bench_comp bench_comp.cpp $(LIBS) $(LDFLAGS) $(LIBS_DEFLATE) $(LIBS_LZMA) $(LIBS_LZ4) $(LIBS_ZSTD) -Wl,-rpath,./external/MaskedVByte

bench_pipeline: bench_pipeline.cpp
	$(CXX) $(CXXFLAGS) -o bench_pipeline bench_pipeline.cpp $(LIBS) $(LDFLAGS) $(LIBS_DEFLATE) $(LIBS_LZMA) $(LIBS_LZ4) $(LIBS_ZSTD) -Wl,-rpath,./external/MaskedVByte

clean:
	rm -f test_comp test_remappings bench_comp bench_pipeline
