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

cmake_minimum_required(VERSION 3.27)

include(FetchContent)
include(ExternalProject)

option(BUILD_SHARED_LIBS "Build shared libraries" ON)

project(Nearby
  DESCRIPTION "A collection of projects focused on connectivity that enable building cross-device experiences."
  HOMEPAGE_URL "https://github.com/google/nearby"
  LANGUAGES 
    C
    CXX
)

set(FETCHCONTENT_QUIET OFF)
cmake_policy(SET CMP0135 NEW)

option(BUILD_TESTING "Build testing binaries" OFF)
option(NO_WEBRTC "Build with no WebRTC support" ON)

if(NO_WEBRTC)
  add_compile_definitions("NO_WEBRTC")
endif()

# Prevents GLOG adding it's own GFLAGS to it's export target, resulting in an error with CMake
set(WITH_GFLAGS OFF CACHE BOOL "Disables building of GFlags")
set(GFLAGS_IS_SUBPROJECT TRUE)
set(GTEST_IS_SUBPROJECT TRUE)
set(INSTALL_GTEST OFF CACHE BOOL "Turns off installing GTest because it's being embedded")
set(BUILD_GMOCK OFF CACHE BOOL "Disable building of GMock in Google Test")
set(ABSL_ENABLE_INSTALL ON CACHE BOOL "Enables install targets of Abseil")
set(protobuf_BUILD_TESTS OFF CACHE BOOL "Disables Protobuf testing")
set(JSON_MultipleHeaders ON CACHE BOOL "Enables nlohmann jsons Multiple headers")

if(BUILD_SHARED_LIBS)
  set(nearby_SHARED_LIBS ${BUILD_SHARED_LIBS})
endif()

set(BUILD_SHARED_LIBS OFF CACHE BOOL "Disable building of shared libs for third party projects" FORCE)
set(CMAKE_POSITION_INDEPENDENT_CODE TRUE)

include("cmake/dependencies.cmake")

set(CMAKE_CXX_STANDARD 20)

set(BUILD_SHARED_LIBS ${nearby_SHARED_LIBS} CACHE BOOL "Revert the option to build the installable project" FORCE)

# Targets
add_subdirectory("proto")
add_subdirectory("internal/analytics")
add_subdirectory("internal/proto")
add_subdirectory("internal/crypto_cros")
add_subdirectory("internal/crypto")
add_subdirectory("internal/flags")
add_subdirectory("internal/base")
add_subdirectory("internal/interop")
add_subdirectory("internal/network")
add_subdirectory("internal/platform")
add_subdirectory("internal/test")
add_subdirectory("internal/weave")
add_subdirectory("internal/data")
add_subdirectory("connections")
add_subdirectory("sharing")