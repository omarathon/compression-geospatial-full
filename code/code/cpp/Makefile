CXX=g++
CXXFLAGS=$(shell gdal-config --cflags)
LIBS=$(shell gdal-config --libs)
TPF_INCLUDE=-I../TurboPFor/include
TPF_LIB=-L../TurboPFor -lic -lm
PROGRAM_NAME=benchmark

all: $(PROGRAM_NAME)

$(PROGRAM_NAME): benchmark.cc
	$(CXX) $(CXXFLAGS) -o $(PROGRAM_NAME) benchmark.cc $(LIBS) $(TPF_INCLUDE) $(TPF_LIB)

clean:
	rm -f $(PROGRAM_NAME)
