# CMake generated Testfile for 
# Source directory: /home/omar/diss/yeet/cpp/compression/external/FastPFor
# Build directory: /home/omar/diss/yeet/cpp/compression/external/FastPFor/build
# 
# This file includes the relevant testing commands required for 
# testing this directory and lists subdirectories to be tested as well.
add_test(unit "unit")
set_tests_properties(unit PROPERTIES  _BACKTRACE_TRIPLES "/home/omar/diss/yeet/cpp/compression/external/FastPFor/CMakeLists.txt;209;add_test;/home/omar/diss/yeet/cpp/compression/external/FastPFor/CMakeLists.txt;0;")
add_test(FastPFOR_unittest "FastPFOR_unittest")
set_tests_properties(FastPFOR_unittest PROPERTIES  _BACKTRACE_TRIPLES "/home/omar/diss/yeet/cpp/compression/external/FastPFor/CMakeLists.txt;210;add_test;/home/omar/diss/yeet/cpp/compression/external/FastPFor/CMakeLists.txt;0;")
subdirs("googletest-build")
