# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION 3.5)

file(MAKE_DIRECTORY
  "/home/omar/compression-geospatial-git/build_skipexc/_deps/cli11-src"
  "/home/omar/compression-geospatial-git/build_skipexc/_deps/cli11-build"
  "/home/omar/compression-geospatial-git/build_skipexc/_deps/cli11-subbuild/cli11-populate-prefix"
  "/home/omar/compression-geospatial-git/build_skipexc/_deps/cli11-subbuild/cli11-populate-prefix/tmp"
  "/home/omar/compression-geospatial-git/build_skipexc/_deps/cli11-subbuild/cli11-populate-prefix/src/cli11-populate-stamp"
  "/home/omar/compression-geospatial-git/build_skipexc/_deps/cli11-subbuild/cli11-populate-prefix/src"
  "/home/omar/compression-geospatial-git/build_skipexc/_deps/cli11-subbuild/cli11-populate-prefix/src/cli11-populate-stamp"
)

set(configSubDirs )
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "/home/omar/compression-geospatial-git/build_skipexc/_deps/cli11-subbuild/cli11-populate-prefix/src/cli11-populate-stamp/${subDir}")
endforeach()
if(cfgdir)
  file(MAKE_DIRECTORY "/home/omar/compression-geospatial-git/build_skipexc/_deps/cli11-subbuild/cli11-populate-prefix/src/cli11-populate-stamp${cfgdir}") # cfgdir has leading slash
endif()
