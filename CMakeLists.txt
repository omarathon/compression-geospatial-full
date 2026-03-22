cmake_minimum_required(VERSION 3.20)
project(compression_geospatial CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ── Compiler flags ────────────────────────────────────────────────────────────
add_compile_options(
  -Wall
  -O3
  -msse4.1
  -mbmi2
)

# ── OpenMP ────────────────────────────────────────────────────────────────────
find_package(OpenMP REQUIRED)

# ── GDAL (system) ─────────────────────────────────────────────────────────────
find_package(PkgConfig REQUIRED)
pkg_check_modules(GDAL REQUIRED gdal)

# ── Pre-built external libraries (git submodules, already compiled) ───────────
set(EXT ${CMAKE_CURRENT_SOURCE_DIR}/external)

add_library(FastPFor STATIC IMPORTED)
set_target_properties(FastPFor PROPERTIES
  IMPORTED_LOCATION ${EXT}/FastPFor/build/libFastPFOR.a
  INTERFACE_INCLUDE_DIRECTORIES ${EXT}/FastPFor/headers
)


add_library(SimdComp STATIC IMPORTED)
set_target_properties(SimdComp PROPERTIES
  IMPORTED_LOCATION ${EXT}/simdcomp/libsimdcomp.a
  INTERFACE_INCLUDE_DIRECTORIES ${EXT}/simdcomp/include
)

# Header-only / include-path-only deps
set(EXTRA_INCLUDES
  ${CMAKE_CURRENT_SOURCE_DIR}/src
  ${CMAKE_CURRENT_SOURCE_DIR}/src/codecs/generic
  ${CMAKE_CURRENT_SOURCE_DIR}/src/codecs/int32
  ${CMAKE_CURRENT_SOURCE_DIR}/src/codecs/uint16
  ${EXT}
  ${EXT}/libmorton/include/libmorton
  ${EXT}/dictionary/src
)

# ── Common codec link deps ────────────────────────────────────────────────────
set(CODEC_LIBS
  FastPFor
  SimdComp
  OpenMP::OpenMP_CXX
  m
)

# ── GoogleTest (fetched at configure time) ────────────────────────────────────
include(FetchContent)
FetchContent_Declare(googletest
  GIT_REPOSITORY https://github.com/google/googletest.git
  GIT_TAG        v1.14.0
)
# Don't install gtest alongside this project
set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

# ── CLI11 (fetched at configure time) ─────────────────────────────────────────
FetchContent_Declare(CLI11
  GIT_REPOSITORY https://github.com/CLIUtils/CLI11.git
  GIT_TAG        v2.4.2
)
FetchContent_MakeAvailable(CLI11)

enable_testing()

# ── Helper to configure each benchmark target ─────────────────────────────────
function(configure_bench TARGET)
  target_include_directories(${TARGET} PRIVATE ${EXTRA_INCLUDES} ${GDAL_INCLUDE_DIRS})
  target_link_libraries(${TARGET} PRIVATE ${CODEC_LIBS} ${GDAL_LIBRARIES} CLI11::CLI11)
endfunction()

# ── Test executables ──────────────────────────────────────────────────────────
add_executable(test_comp tests/test_int32_codecs.cpp)
target_include_directories(test_comp PRIVATE ${EXTRA_INCLUDES})
target_link_libraries(test_comp PRIVATE ${CODEC_LIBS} GTest::gtest_main)


add_executable(test_remappings tests/test_remappings.cpp)
target_include_directories(test_remappings PRIVATE ${EXTRA_INCLUDES})
target_link_libraries(test_remappings PRIVATE GTest::gtest_main)

add_executable(test_bench_utils tests/test_bench_utils.cpp)
target_include_directories(test_bench_utils PRIVATE ${EXTRA_INCLUDES})
target_link_libraries(test_bench_utils PRIVATE ${CODEC_LIBS} GTest::gtest_main)


include(GoogleTest)
gtest_discover_tests(test_comp)
gtest_discover_tests(test_remappings)
gtest_discover_tests(test_bench_utils)

# ── Benchmark executables ─────────────────────────────────────────────────────
add_executable(bench_comp bench/bench_comp.cpp)
configure_bench(bench_comp)

add_executable(bench_pipeline bench/bench_pipeline.cpp)
configure_bench(bench_pipeline)
