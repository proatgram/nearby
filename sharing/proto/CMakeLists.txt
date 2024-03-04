# Copyright 2023 Google LLC
#
# Licensed under the Apache License Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     https://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing software
# distributed under the License is distributed on an "AS IS" BASIS
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

# target share_cc_proto

protobuf_generate(
  LANGUAGE cpp
  OUT_VAR share_cc_proto_GENERATED
  IMPORT_DIRS
    ${CMAKE_SOURCE_DIR}
  PROTOC_OUT_DIR
    ${CMAKE_BINARY_DIR}
  PROTOS
    "certificate_rpc.proto"
    "contact_rpc.proto"
    "device_rpc.proto"
    "encrypted_metadata.proto"
    "enums.proto"
    "field_mask.proto"
    "rpc_resources.proto"
    "settings_observer_data.proto"
    "timestamp.proto"
    "wire_format.proto"
)

add_library(share_cc_proto
    ${share_cc_proto_GENERATED}
)

target_include_directories(share_cc_proto PUBLIC ${CMAKE_BINARY_DIR})
target_link_libraries(share_cc_proto PUBLIC protobuf::libprotobuf)

add_subdirectory("analytics")