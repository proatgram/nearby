// Copyright 2022 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef THIRD_PARTY_NEARBY_SHARING_NEARBY_CONNECTIONS_MANAGER_IMPL_H_
#define THIRD_PARTY_NEARBY_SHARING_NEARBY_CONNECTIONS_MANAGER_IMPL_H_

#include <stdint.h>

#include <filesystem>  // NOLINT(build/c++17)
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "nearby_connections_manager.h"
#include "nearby_connections_service.h"
#include "internal/platform/mutex.h"
#include "transfer_manager.h"
#include "nearby_connection_impl.h"

namespace nearby {
namespace sharing {

// Concrete NearbyConnectionsManager implementation.
class NearbyConnectionsManagerImpl : public NearbyConnectionsManager
{
 public:

  explicit NearbyConnectionsManagerImpl(std::unique_ptr<NearbyConnectionsService> nearby_connections_service);
  ~NearbyConnectionsManagerImpl() override;
  NearbyConnectionsManagerImpl(const NearbyConnectionsManagerImpl&) = delete;
  NearbyConnectionsManagerImpl& operator=(const NearbyConnectionsManagerImpl&) = delete;

  // NearbyConnectionsManager:
  void Connect(std::vector<uint8_t> endpoint_info,
               absl::string_view endpoint_id,
               std::optional<std::vector<uint8_t>> bluetooth_mac_address,
               proto::DataUsage data_usage, TransportType transport_type,
               NearbyConnectionCallback callback) override;
  void Disconnect(absl::string_view endpoint_id) override;

  void Send(absl::string_view endpoint_id, std::unique_ptr<Payload> payload,
      std::weak_ptr<PayloadStatusListener> listener) override;

  void RegisterPayloadStatusListener(
      int64_t payload_id,
      std::weak_ptr<PayloadStatusListener> listener) override;

  void RegisterPayloadPath(int64_t payload_id,
      const std::filesystem::path& file_path,
      ConnectionsCallback callback) override;

  void ClearIncomingPayloads() override;

  Payload* GetIncomingPayload(int64_t payload_id) override;

  std::optional<std::vector<uint8_t>> GetRawAuthenticationToken(
      absl::string_view endpoint_id) override;

  void StartDiscovery(DiscoveryListener* listener, proto::DataUsage data_usage,
      ConnectionsCallback callback) override;

  void StartAdvertising(std::vector<uint8_t> endpoint_info,
      IncomingConnectionListener* listener,
      PowerLevel power_level, proto::DataUsage data_usage,
      ConnectionsCallback callback) override;
  
 private:

     // ConnectionLifecycleListener:
     void OnConnectionInitiated(absl::string_view endpoint_id,
         const ConnectionInfo& info);
     void OnConnectionAccepted(absl::string_view endpoint_id);
     void OnConnectionRejected(absl::string_view endpoint_id, Status status);
     void OnDisconnected(absl::string_view endpoint_id);
     void OnBandwidthChanged(absl::string_view endpoint_id, Medium medium);

     // PayloadListener:
     void OnPayloadReceived(absl::string_view endpoint_id, Payload& payload);
     void OnPayloadTransferUpdate(absl::string_view endpoint_id,
         const PayloadTransferUpdate& update);

     void OnConnectionRequested(absl::string_view endpoint_id,
         ConnectionsStatus status);

     std::optional<Medium> GetUpgradedMedium(absl::string_view endpoint_id) const;

     void SendWithoutDelay(absl::string_view endpoint_id,
         std::unique_ptr<Payload> payload);

     // EndpointDiscoveryListener:
     void OnEndpointFound(absl::string_view endpoint_id,
         const DiscoveredEndpointInfo& info);
     void OnEndpointLost(absl::string_view endpoint_id);

     

  
  // Nearby Connections Manager is called from different threads and may have
  // multiple calls to the class from one thread. To avoid deadlock and access
  // violation, use a recursive mutex to protect class members.
  mutable RecursiveMutex mutex_;

  std::unique_ptr<NearbyConnectionsService> nearby_connections_service_ = nullptr;
  IncomingConnectionListener* incoming_connection_listener_ = nullptr;
  DiscoveryListener* discovery_listener_ = nullptr;

  // A map of endpoint_id to ConnectionInfoPtr.
  absl::flat_hash_map<std::string, ConnectionInfo> connection_info_map_
      ABSL_GUARDED_BY(mutex_);

  // A map of endpoint_id to NearbyConnection.
  absl::flat_hash_map<std::string, std::unique_ptr<NearbyConnectionImpl>>
      connections_ ABSL_GUARDED_BY(mutex_);

  // A map of endpoint_id to NearbyConnectionCallback.
  absl::flat_hash_map<std::string, NearbyConnectionCallback>
      pending_outgoing_connections_ ABSL_GUARDED_BY(mutex_);

  // A map of endpoint_id to timers that timeout a connection request.
  absl::flat_hash_map<std::string, std::unique_ptr<Timer>>
      connect_timeout_timers_ ABSL_GUARDED_BY(mutex_);

  // A map of endpoint_id to transfer manager.
  absl::flat_hash_map<std::string, std::unique_ptr<TransferManager>>
      transfer_managers_ ABSL_GUARDED_BY(mutex_);

  // A map of payload_id to PayloadPtr.
  absl::flat_hash_map<int64_t, Payload> incoming_payloads_
      ABSL_GUARDED_BY(mutex_);

  // A map of payload_id to PayloadStatusListener weak pointer.
  absl::flat_hash_map<int64_t, std::weak_ptr<PayloadStatusListener>>
      payload_status_listeners_ ABSL_GUARDED_BY(mutex_);

  // For metrics. A map of endpoint_id to the current upgraded medium.
  absl::flat_hash_map<std::string, Medium> current_upgraded_mediums_
      ABSL_GUARDED_BY(mutex_);

  // Avoid calling to disconnect on an endpoint multiple times.
  absl::flat_hash_set<std::string> disconnecting_endpoints_
      ABSL_GUARDED_BY(mutex_);

  absl::flat_hash_set<std::string> discovered_endpoints_
      ABSL_GUARDED_BY(mutex_);
};

}  // namespace sharing
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_SHARING_NEARBY_CONNECTIONS_MANAGER_IMPL_H_
