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

# target internal_platform_implementation_windows_types
add_library(internal_platform_implementation_windows_types
    "atomic_boolean.h"
    "atomic_reference.h"
    "bluetooth_adapter.h"
    "condition_variable.h"
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
    "timer.h"
    "utils.h"
    "device_info.cc"
    "log_message.cc"
    "timer.cc"
)

target_link_libraries(internal_platform_implementation_windows_types
  PUBLIC
    internal::platform::implementation::windows::comm
    internal::base::bluetooth_address
    internal::platform::base
    internal::platform::types
    internal::platform::uuid
    internal::platform::implementation::types
    absl::core_headers
    absl::flat_hash_map
    absl::any_invocable
    absl::memory
    absl::strings
    absl::synchronization
    absl::time
    absl::span
    nlohmann_json::nlohmann_json
)

target_include_directories(internal_platform_implementation_windows_types INTERFACE ${CMAKE_SOURCE_DIR})

add_library(internal::platform::implementation::windows::types ALIAS internal_platform_implementation_windows_types)

# target internal_platform_implementation_windows_comm
add_library(internal_platform_implementation_windows_comm
    "ble.h"
    "ble_gatt_client.h"
    "ble_gatt_server.h"
    "ble_medium.h"
    "ble_peripheral.h"
    "ble_socket.h"
    "ble_v2.h"
    "ble_v2_peripheral.h"
    "ble_v2_server_socket.h"
    "ble_v2_socket.h"
    "bluetooth_adapter.h"
    "bluetooth_classic.h"
    "bluetooth_classic_device.h"
    "bluetooth_classic_medium.h"
    "bluetooth_classic_server_socket.h"
    "bluetooth_classic_socket.h"
    "bluetooth_pairing.h"
    "condition_variable.h"
    "executor.h"
    "file.h"
    "file_path.h"
    "http_loader.h"
    "mutex.h"
    "scheduled_executor.h"
    "server_sync.h"
    "session_manager.h"
    "submittable_executor.h"
    "thread_pool.h"
    "webrtc.h"
    "wifi.h"
    "wifi_direct.h"
    "wifi_hotspot.h"
    "wifi_lan.h"
)

target_link_libraries(internal_platform_implementation_windows_comm
  PUBLIC
    internal::platform::base
    internal::platform::comm
    internal::platform::types
    internal::platform::uuid
    internal::platform::implementation::comm
    internal::platform::implementation::types
    absl::core_headers
    absl::flat_hash_map
    absl::flat_hash_set
    absl::any_invocable
    absl::memory
    absl::status
    absl::statusor
    absl::strings
    absl::synchronization
    absl::time
    absl::optional
)

target_include_directories(internal_platform_implementation_windows_comm INTERFACE ${CMAKE_SOURCE_DIR})

add_library(internal::platform::implementation::windows::comm ALIAS internal_platform_implementation_windows_comm)

# target internal_platform_implementation_windows_crypto
add_library(internal_platform_implementation_windows_crypto
    "crypto.cc"
)

target_link_libraries(internal_platform_implementation_windows_crypto
  PUBLIC
    internal::platform::base
    internal::platform::implementation::types
    boringssl::crypto
    absl::strings
)

target_include_directories(internal_platform_implementation_windows_crypto INTERFACE ${CMAKE_SOURCE_DIR})

add_library(internal::platform::implementation::windows::crypto ALIAS internal_platform_implementation_windows_crypto)

# target internal_platform_implementation_windows
add_library(internal_platform_implementation_windows
    "ble_gatt_client.cc"
    "ble_gatt_server.cc"
    "ble_medium.cc"
    "ble_socket.cc"
    "ble_v2.cc"
    "ble_v2_peripheral.cc"
    "ble_v2_server_socket.cc"
    "ble_v2_socket.cc"
    "bluetooth_adapter.cc"
    "bluetooth_classic_device.cc"
    "bluetooth_classic_medium.cc"
    "bluetooth_classic_server_socket.cc"
    "bluetooth_classic_socket.cc"
    "bluetooth_pairing.cc"
    "executor.cc"
    "file.cc"
    "file_path.cc"
    "http_loader.cc"
    "platform.cc"
    "preferences_manager.cc"
    "preferences_repository.cc"
    "preferences_repository.h"
    "scheduled_executor.cc"
    "session_manager.cc"
    "submittable_executor.cc"
    "system_clock.cc"
    "thread_pool.cc"
    "utils.cc"
    "webrtc.cc"
    "wifi_direct_medium.cc"
    "wifi_direct_server_socket.cc"
    "wifi_direct_socket.cc"
    "wifi_hotspot_medium.cc"
    "wifi_hotspot_server_socket.cc"
    "wifi_hotspot_socket.cc"
    "wifi_lan_medium.cc"
    "wifi_lan_server_socket.cc"
    "wifi_lan_socket.cc"
    "wifi_medium.cc"
)

target_link_libraries(internal_platform_implementation_windows
  PUBLIC
    internal::platform::implementation::windows::comm
    internal::platform::implementation::windows::crypto
    internal::platform::implementation::windows::types
    internal::flags::nearby_flags
    internal::platform::base
    internal::platform::cancellation_flag
    internal::platform::comm
    internal::platform::types
    internal::platform::uuid
    internal::platform::flags::platform_flags
    internal::platform::implementation::comm
    internal::platform::implementation::platform
    internal::platform::implementation::types
    internal::platform::implementation::shared::count_down_latch
    internal::platform::implementation::shared::file
    absl::core_headers
    absl::flat_hash_map
    absl::any_invocable
    absl::check
    absl::memory
    absl::status
    absl::statusor
    absl::strings
    absl::str_format
    absl::synchronization
    absl::time
    absl::optional
    nlohmann_json::nlohmann_json
)

target_include_directories(internal_platform_implementation_windows INTERFACE ${CMAKE_SOURCE_DIR})

add_library(internal::platform::implementation::windows ALIAS internal_platform_implementation_windows)