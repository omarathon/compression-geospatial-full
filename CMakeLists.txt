cmake_minimum_required(VERSION 3.20)
project(compression_geospatial CXX C)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ── Compiler flags ────────────────────────────────────────────────────────────
add_compile_options(
  -Wall
  -O3
  -msse4.1
  -mbmi2
  -mavx2
  -mfma
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

add_library(TurboPFor STATIC IMPORTED)
set_target_properties(TurboPFor PROPERTIES
  IMPORTED_LOCATION ${EXT}/TurboPFor/libic.a
  INTERFACE_INCLUDE_DIRECTORIES ${EXT}/TurboPFor/include
)

# Fused-sum decode prototype: compiled from a TurboPFor lib/ source so it can
# use the internal vertical-bitunpack macros + the _shuffle_16 table.
# vp4d256v16_fused.c is the 256-width 16-bit PFor variant; it reuses simdcomp's
# AVX2 16-lane kernels (simdpack_u16 / simdunpack_u16[_corrected]).
add_library(TurboPForFused STATIC
  ${EXT}/TurboPFor/lib/vp4d_fused.c
  ${EXT}/TurboPFor/lib/vp4d256v16_fused.c
  ${EXT}/TurboPFor/lib/vp4d256v16_for_fused.c)
target_include_directories(TurboPForFused PRIVATE
  ${EXT}/TurboPFor/lib ${EXT}/TurboPFor/include
  ${EXT}/simdcomp/include ${EXT}/simdcomp/src)
target_compile_options(TurboPForFused PRIVATE -O3 -mavx2)
# Diagnostic: skip ALL PFor exception decode work (bitunpack16 of excess, the
# sum_excess/pshufb merge) and just walk the stream + unpack the low-bit plane.
# Sums are WRONG (exceptions ignored) — timing-only, to isolate the exception
# cost vs the bitpack-unpack cost. Default OFF = full, correct decode.
option(PFOR_SKIP_EXC "TurboPForFused: skip PFor exception decode (timing-only)" OFF)
if(PFOR_SKIP_EXC)
  target_compile_definitions(TurboPForFused PRIVATE PFOR_SKIP_EXC)
endif()
# Real (correct) variant: store BITMAP excess as fixed-width bytes (uint8 if
# bxe<=8, else uint16) instead of bitpack16, and SIMD-sum them (no bitunpack16,
# no scalar bit-extraction). Trades a few bits/exception of CR for a branchless
# parallel excess sum. Measures how close to the SKIP floor a correct decoder gets.
option(PFOR_BYTE_EXC "TurboPForFused: byte-aligned excess (correct, SIMD-summable)" OFF)
if(PFOR_BYTE_EXC)
  target_compile_definitions(TurboPForFused PRIVATE PFOR_BYTE_EXC)
endif()
target_link_libraries(TurboPForFused PUBLIC TurboPFor SimdComp)

# 128-bit (SSE) simdcomp u16 pack/unpack — generated minimal variant, built as
# its own lib (-O3 -march=native, matching libsimdcomp.a) so the plain unpack is
# dispatched/non-inlined exactly like the 256-bit kernel. Used by the
# simdcomp_fused_128 codec for an apples-to-apples 128-vs-256 width benchmark.
add_library(SimdCompU16W128 STATIC
  ${EXT}/simdcomp/src/simdbitpacking_u16_w128.c
  ${EXT}/simdcomp/src/simdbitpacking_u16_w128_corrected.c
  ${EXT}/simdcomp/src/simdbitpacking_u16_w128_nobc.c)
target_include_directories(SimdCompU16W128 PRIVATE ${EXT}/simdcomp/include)
target_compile_options(SimdCompU16W128 PRIVATE -O3 -march=native -mno-avx512f)

# 256-bit (AVX2) simdcomp u16 FoR kernels — generated minimal + FoR-corrected
# variant, built as its own lib (suffixed _w256 symbols) so it coexists with
# canonical libsimdcomp.a. Powers the SimdCompFusedForCodecU16_256 codecs
# (regular + hierarchical). Mirrors the SimdCompU16W128 setup exactly.
add_library(SimdCompU16W256 STATIC
  ${EXT}/simdcomp/src/simdbitpacking_u16_w256.c
  ${EXT}/simdcomp/src/simdbitpacking_u16_w256_corrected.c
  ${EXT}/simdcomp/src/simdbitpacking_u16_w256_nobc.c)
target_include_directories(SimdCompU16W256 PRIVATE ${EXT}/simdcomp/include)
target_compile_options(SimdCompU16W256 PRIVATE -O3 -march=native -mno-avx512f)

# Two-band lock-step fused NDVI kernels (independent b: 289 per-(bA,bB) kernels).
# Separate lib; additive — does not touch any single-band generated source.
add_library(Ndvi2Band STATIC ${EXT}/simdcomp/src/ndvi2band_w256.cpp)
target_compile_options(Ndvi2Band PRIVATE -O3 -march=native -mno-avx512f)
set_target_properties(Ndvi2Band PROPERTIES CXX_STANDARD 20 CXX_STANDARD_REQUIRED ON)

# Header-only / include-path-only deps
set(EXTRA_INCLUDES
  ${CMAKE_CURRENT_SOURCE_DIR}/src
  ${CMAKE_CURRENT_SOURCE_DIR}/src/codecs/generic
  ${CMAKE_CURRENT_SOURCE_DIR}/src/codecs/int32
  ${CMAKE_CURRENT_SOURCE_DIR}/src/codecs/uint16
  ${CMAKE_CURRENT_SOURCE_DIR}/src/codecs/uint8
  ${EXT}
  ${EXT}/TurboPFor/include
  ${EXT}/libmorton/include/libmorton
)

# ── System compression libraries ─────────────────────────────────────────────
pkg_check_modules(OpenJPEG REQUIRED libopenjp2)

# ── Common codec link deps ────────────────────────────────────────────────────
set(CODEC_LIBS
  TurboPForFused
  SimdCompU16W128
  SimdCompU16W256
  FastPFor
  SimdComp
  TurboPFor
  OpenMP::OpenMP_CXX
  m
  z        # zlib  (Deflate)
  lz4      # LZ4
  zstd     # Zstd
  lzma     # LZMA/XZ
  png      # libpng
  ${OpenJPEG_LIBRARIES}  # JPEG 2000
)

list(APPEND EXTRA_INCLUDES ${OpenJPEG_INCLUDE_DIRS})

# ── Optional: CharLS (install libcharls-dev to enable) ────────────────────────
find_library(CHARLS_LIB NAMES charls)
find_path(CHARLS_INCLUDE NAMES charls/charls.h)
if(CHARLS_LIB AND CHARLS_INCLUDE)
  list(APPEND CODEC_LIBS ${CHARLS_LIB})
  list(APPEND EXTRA_INCLUDES ${CHARLS_INCLUDE})
  add_compile_definitions(HAVE_CHARLS)
  message(STATUS "CharLS found: ${CHARLS_LIB}")
else()
  message(STATUS "CharLS not found - install libcharls-dev to enable")
endif()

# ── Optional: Snappy (install libsnappy-dev to enable) ────────────────────────
find_library(SNAPPY_LIB NAMES snappy)
find_path(SNAPPY_INCLUDE NAMES snappy.h)
if(SNAPPY_LIB AND SNAPPY_INCLUDE)
  list(APPEND CODEC_LIBS ${SNAPPY_LIB})
  list(APPEND EXTRA_INCLUDES ${SNAPPY_INCLUDE})
  add_compile_definitions(HAVE_SNAPPY)
  message(STATUS "Snappy found: ${SNAPPY_LIB}")
else()
  message(STATUS "Snappy not found - install libsnappy-dev to enable")
endif()

# ── Optional: LERC (install liblerc-dev to enable) ────────────────────────────
find_library(LERC_LIB NAMES Lerc lerc)
find_path(LERC_INCLUDE NAMES Lerc_c_api.h)
if(LERC_LIB AND LERC_INCLUDE)
  list(APPEND CODEC_LIBS ${LERC_LIB})
  list(APPEND EXTRA_INCLUDES ${LERC_INCLUDE})
  add_compile_definitions(HAVE_LERC)
  message(STATUS "LERC found: ${LERC_LIB}")
else()
  message(STATUS "LERC not found - install liblerc-dev to enable")
endif()

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

add_executable(test_uint16_codecs tests/test_uint16_codecs.cpp)
target_include_directories(test_uint16_codecs PRIVATE ${EXTRA_INCLUDES})
target_link_libraries(test_uint16_codecs PRIVATE ${CODEC_LIBS} GTest::gtest_main)

add_executable(test_int32_codecs_server tests/test_int32_codecs_server.cpp)
target_include_directories(test_int32_codecs_server PRIVATE ${EXTRA_INCLUDES})
target_link_libraries(test_int32_codecs_server PRIVATE ${CODEC_LIBS} GTest::gtest_main)

add_executable(test_uint16_codecs_server tests/test_uint16_codecs_server.cpp)
target_include_directories(test_uint16_codecs_server PRIVATE ${EXTRA_INCLUDES})
target_link_libraries(test_uint16_codecs_server PRIVATE ${CODEC_LIBS} GTest::gtest_main)

add_executable(test_uint8_codecs_server tests/test_uint8_codecs_server.cpp)
target_include_directories(test_uint8_codecs_server PRIVATE ${EXTRA_INCLUDES})
target_link_libraries(test_uint8_codecs_server PRIVATE GTest::gtest_main)

add_executable(test_fused_codecs tests/test_fused_codecs.cpp)
target_include_directories(test_fused_codecs PRIVATE ${EXTRA_INCLUDES})
target_link_libraries(test_fused_codecs PRIVATE ${CODEC_LIBS} GTest::gtest_main)

include(GoogleTest)
gtest_discover_tests(test_comp)
gtest_discover_tests(test_remappings)
gtest_discover_tests(test_bench_utils)
gtest_discover_tests(test_uint16_codecs)
gtest_discover_tests(test_int32_codecs_server)
gtest_discover_tests(test_uint16_codecs_server)
gtest_discover_tests(test_uint8_codecs_server)
gtest_discover_tests(test_fused_codecs)

# ── Benchmark executables ─────────────────────────────────────────────────────
add_executable(bench_comp bench/bench_comp.cpp)
configure_bench(bench_comp)

add_executable(bench_pipeline bench/bench_pipeline.cpp)
configure_bench(bench_pipeline)

# Multi-threaded variant: X parallel decode workers over one shared grid
# (grid = n*X blocks; each worker decodes a disjoint n-block slice).
add_executable(bench_pipeline_multithread bench/bench_pipeline_multithread.cpp)
configure_bench(bench_pipeline_multithread)
find_package(Threads REQUIRED)
target_link_libraries(bench_pipeline_multithread PRIVATE Threads::Threads)

# Two-band fused NDVI decode benchmark (L1-temp / shared-b streamed).
add_executable(bench_pipeline_2band bench/bench_pipeline_2band.cpp)
configure_bench(bench_pipeline_2band)
target_link_libraries(bench_pipeline_2band PRIVATE Ndvi2Band)

add_executable(for128_parity bench/for128_parity.cpp)
configure_bench(for128_parity)

add_executable(for256_parity bench/for256_parity.cpp)
configure_bench(for256_parity)

add_executable(for256_pfor_parity bench/for256_pfor_parity.cpp)
configure_bench(for256_pfor_parity)

# Same as bench_pipeline but the w128 FoR/base decoders skip the widening sum
# (XOR sink) — times produce-OutReg-only. Sums are wrong; for decode timing only.
add_executable(bench_pipeline_noagg bench/bench_pipeline.cpp)
configure_bench(bench_pipeline_noagg)
target_compile_definitions(bench_pipeline_noagg PRIVATE FOR_DECODE_NOAGG)
