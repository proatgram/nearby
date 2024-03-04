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

# target internal_platform_implementation_types
add_library(internal_platform_implementation_types
  INTERFACE
    "atomic_boolean.h"
    "atomic_reference.h"
    "bluetooth_adapter.h"
    "cancelable.h"
    "condition_variable.h"
    "count_down_latch.h"
    "crypto.h"
    "device_info.h"
    "executor.h"
    "future.h"
    "input_file.h"
    "listenable_future.h"
    "log_message.h"
    "mutex.h"
    "output_file.h"
    "preferences_manager.h"
    "scheduled_executor.h"
    "settable_future.h"
    "submittable_executor.h"
    "system_clock.h"
    "timer.h"
)

target_link_libraries(internal_platform_implementation_types
  INTERFACE
    internal::crypto
    internal::platform::base
    absl::base
    absl::strings
    absl::time
    nlohmann_json::nlohmann_json
)

target_include_directories(internal_platform_implementation_types INTERFACE ${CMAKE_SOURCE_DIR})

add_library(internal::platform::implementation::types ALIAS internal_platform_implementation_types)

# target internal_platform_implementation_comm
add_library(internal_platform_implementation_comm
  INTERFACE
    "ble.h"
    "ble_v2.h"
    "bluetooth_adapter.h"
    "bluetooth_classic.h"
    "credential_callbacks.h"
    "credential_storage.h"
    "http_loader.h"
    "server_sync.h"
    "webrtc.h"
    "wifi.h"
    "wifi_direct.h"
    "wifi_hotspot.h"
    "wifi_lan.h"
)

target_link_libraries(internal_platform_implementation_comm
  INTERFACE
    offline_wire_formats_cc_proto
    internal::platform::base
    internal::platform::cancellation_flag
    internal::platform::uuid
    credential_cc_proto
    local_credential_cc_proto
    # TODO: Support WebRTC
    absl::flat_hash_map
    absl::any_invocable
    absl::check
    absl::status
    absl::statusor
    absl::strings
    absl::str_format
)

target_include_directories(internal_platform_implementation_comm INTERFACE ${CMAKE_SOURCE_DIR})

add_library(internal::platform::implementation::comm ALIAS internal_platform_implementation_comm)

# target internal_platform_implementation_platform
add_library(internal_platform_implementation_platform
  INTERFACE
    "platform.h"
)

target_link_libraries(internal_platform_implementation_platform
  INTERFACE
    internal::platform::implementation::comm
    internal::platform::implementation::types
    internal::platform::base
    absl::statusor
    absl::strings
)

target_include_directories(internal_platform_implementation_platform INTERFACE ${CMAKE_SOURCE_DIR})

add_library(internal::platform::implementation::platform ALIAS internal_platform_implementation_platform)

add_subdirectory("shared")
add_subdirectory("windows")

IF (UNIX AND NOT APPLE)
  add_subdirectory("linux")
endif()
