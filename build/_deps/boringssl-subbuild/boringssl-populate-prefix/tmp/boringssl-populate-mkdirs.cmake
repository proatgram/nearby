# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION 3.5)

file(MAKE_DIRECTORY
  "D:/Code/GNearby/GNearbyCmake/GNearby/build/_deps/boringssl-src"
  "D:/Code/GNearby/GNearbyCmake/GNearby/build/_deps/boringssl-build"
  "D:/Code/GNearby/GNearbyCmake/GNearby/build/_deps/boringssl-subbuild/boringssl-populate-prefix"
  "D:/Code/GNearby/GNearbyCmake/GNearby/build/_deps/boringssl-subbuild/boringssl-populate-prefix/tmp"
  "D:/Code/GNearby/GNearbyCmake/GNearby/build/_deps/boringssl-subbuild/boringssl-populate-prefix/src/boringssl-populate-stamp"
  "D:/Code/GNearby/GNearbyCmake/GNearby/build/_deps/boringssl-subbuild/boringssl-populate-prefix/src"
  "D:/Code/GNearby/GNearbyCmake/GNearby/build/_deps/boringssl-subbuild/boringssl-populate-prefix/src/boringssl-populate-stamp"
)

set(configSubDirs Debug)
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "D:/Code/GNearby/GNearbyCmake/GNearby/build/_deps/boringssl-subbuild/boringssl-populate-prefix/src/boringssl-populate-stamp/${subDir}")
endforeach()
if(cfgdir)
  file(MAKE_DIRECTORY "D:/Code/GNearby/GNearbyCmake/GNearby/build/_deps/boringssl-subbuild/boringssl-populate-prefix/src/boringssl-populate-stamp${cfgdir}") # cfgdir has leading slash
endif()
