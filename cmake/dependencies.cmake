# Copyright 2023 Google LLC
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     https://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

fetchcontent_declare(
  absl
  GIT_REPOSITORY "https://github.com/abseil/abseil-cpp.git"
  GIT_TAG "20230802.0"
  GIT_PROGRESS TRUE
  # FIND_PACKAGE_ARGS CONFIG
  OVERRIDE_FIND_PACKAGE
)

fetchcontent_declare(
  protobuf
  GIT_REPOSITORY "https://github.com/protocolbuffers/protobuf.git"
  GIT_TAG "v3.17.0"
  GIT_PROGRESS TRUE
  SOURCE_SUBDIR "cmake"
  # FIND_PACKAGE_ARGS CONFIG
  OVERRIDE_FIND_PACKAGE
)

fetchcontent_declare(
  smhasher
  GIT_REPOSITORY "https://github.com/aappleby/smhasher.git"
  GIT_PROGRESS TRUE
  SOURCE_SUBDIR "src"
  # OVERRIDE_FIND_PACKAGE
  OVERRIDE_FIND_PACKAGE
)

fetchcontent_declare(
  nlohmann_json
  GIT_REPOSITORY "https://github.com/nlohmann/json.git"
  GIT_TAG "v3.10.5"
  GIT_PROGRESS TRUE
  # FIND_PACKAGE_ARGS CONFIG
  OVERRIDE_FIND_PACKAGE
)

fetchcontent_declare(
  GTest
  GIT_REPOSITORY "https://github.com/google/googletest.git"
  GIT_TAG "main"
  GIT_PROGRESS TRUE
  # FIND_PACKAGE_ARGS CONFIG
  OVERRIDE_FIND_PACKAGE
)

fetchcontent_declare(
  gflags
  GIT_REPOSITORY "https://github.com/gflags/gflags.git"
  GIT_TAG "v2.2.2"
  GIT_PROGRESS TRUE
  # FIND_PACKAGE_ARGS
  OVERRIDE_FIND_PACKAGE
)

fetchcontent_declare(
  glog
  GIT_REPOSITORY "https://github.com/google/glog.git"
  GIT_TAG "v0.4.0"
  GIT_PROGRESS TRUE
  # FIND_PACKAGE_ARGS CONFIG
  OVERRIDE_FIND_PACKAGE
)

fetchcontent_declare(
  boringssl
  GIT_REPOSITORY "https://boringssl.googlesource.com/boringssl.git"
  GIT_PROGRESS TRUE
  OVERRIDE_FIND_PACKAGE
)

fetchcontent_declare(
  protobuf_matchers
  GIT_REPOSITORY "https://github.com/inazarenko/protobuf-matchers.git"
  GIT_PROGRESS TRUE
  OVERRIDE_FIND_PACKAGE
)

fetchcontent_declare(
  re2
  GIT_REPOSITORY "https://github.com/google/re2.git"
  GIT_TAG "2021-06-01"
  GIT_PROGRESS TRUE
  OVERRIDE_FIND_PACKAGE
)

fetchcontent_declare(
  ukey2
  GIT_REPOSITORY "https://github.com/google/ukey2.git"
  GIT_TAG "c2436e55116964d88532080784f6ed496b0d11f9"
  GIT_PROGRESS TRUE
  OVERRIDE_FIND_PACKAGE
)

fetchcontent_declare(
  securemessage
  GIT_REPOSITORY "https://github.com/google/securemessage"
  GIT_PROGRESS TRUE
  OVERRIDE_FIND_PACKAGE
)

fetchcontent_declare(
  sdbus-c++
  GIT_REPOSITORY "https://github.com/Kistler-Group/sdbus-cpp.git"
  GIT_TAG "v1.3.0"
  GIT_PROGRESS TRUE
  # FIND_PACKAGE_ARGS CONFIG NAMES sdbus-c++
  OVERRIDE_FIND_PACKAGE
)

fetchcontent_declare(
  leveldb
  GIT_REPOSITORY "https://github.com/google/leveldb.git"
  GIT_TAG "v1.20"
  GIT_PROGRESS TRUE
  # FIND_PACKAGE_ARGS CONFIG
  OVERRIDE_FIND_PACKAGE
)

fetchcontent_makeavailable(
  absl
  protobuf
  glog
  smhasher
  nlohmann_json
  GTest
  gflags
  boringssl
  #protobuf_matchers
  re2
  leveldb
)

# Find packages
include(FindGTest)
if(NOT TARGET gtest)
  add_library(gtest ALIAS GTest::gtest)
endif()

if(UNIX AND NOT APPLE)
  include(FindPkgConfig)
  fetchcontent_makeavailable(
    sdbus-c++
  )
  if(NOT TARGET SDBusCpp)
      add_library(SDBusCpp::sdbus-c++ ALIAS sdbus-c++)
  endif()
  # SDBus sets the PIC option to the same as BUILD_SHARED_LIBS
  # We are not building shared libraries with the dependencies
  # but we are with the actual library output, thus we require
  # this option to be true to link static sdbus to our shared
  # targets
  set_target_properties(sdbus-c++-objlib PROPERTIES POSITION_INDEPENDENT_CODE TRUE)
  pkg_check_modules(
    libsystemd
    REQUIRED
    IMPORTED_TARGET
    libsystemd
  )
  pkg_check_modules(
    libcurl
    REQUIRED
    IMPORTED_TARGET
    libcurl
  )
endif()

# The function used in some of the dependencies to generate
# protobuf files use this variable instead of a target to
# link to libprotobuf
set(Protobuf_LIBRARIES protobuf::libprotobuf)

if(NOT protobuf_FOUND)
  # Include the CMake file that adds the function protobuf_generate into our project
  fetchcontent_getproperties(protobuf BINARY_DIR protobuf_BINARY_DIR)
  include(${protobuf_BINARY_DIR}/cmake/protobuf/protobuf-generate.cmake)
endif()

# UKey2 and securemessage depends on the functions included in protobuf-generate.cmake
# We make this avaliable after we include that
fetchcontent_makeavailable(securemessage)
fetchcontent_makeavailable(ukey2)

# For BoringSSL, it throws all warnings as errors, and this gets thrown on the current "master" branch
target_compile_options(crypto INTERFACE "-Wno-error=ignored-attributes")

# Alias targets for easier readablity
add_library(boringssl::crypto ALIAS crypto)
add_library(ukey2::ukey2 ALIAS ukey2)

# Protobuf targets for UKey2 and SecureMessage don't link to protobuf::libprotobuf
# thus throwing a `#error` that the protobuf version isn't correct.

# For aappleby_smhasher it does not add an include directory to it's target
fetchcontent_getproperties(smhasher
  SOURCE_DIR
    smhasher_SOURCE_DIR
)
set_property(
  TARGET
    SMHasherSupport
  APPEND
  PROPERTY
    INCLUDE_DIRECTORIES
      ${smhasher_SOURCE_DIR}
)
set_property(
  TARGET
    SMHasherSupport
  APPEND
  PROPERTY
    INTERFACE_INCLUDE_DIRECTORIES
      ${smhasher_SOURCE_DIR}
)

# aappleby_smhasher also does not have export and install targets, so no alias target is made
add_library(smhasher::SMHasherSupport ALIAS SMHasherSupport)

# Projects that don't have CMake support
# WebRTC
# These will need to be implemented here with ExternalProject_Add()
