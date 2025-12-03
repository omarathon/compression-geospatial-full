CXX = g++
CXXFLAGS = -cpp -Wall -O1 '-msse4.1' '-mbmi2' -fopenmp -I./external -I./external/simdcomp/include -I./external/libmorton/include/libmorton  $(shell gdal-config --cflags)
LDFLAGS = -L./external/FastPFor/build -lFastPFOR -L./external/simdcomp -lsimdcomp

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
	$(CXX) $(CXXFLAGS) -o bench_pipeline bench_pipeline.cpp $(LIBS) $(LDFLAGS)

clean:
	rm -f test_comp test_remappings bench_comp bench_pipeline
