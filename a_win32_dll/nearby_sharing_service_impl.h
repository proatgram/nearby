// Copyright 2022-2023 Google LLC
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

#ifndef THIRD_PARTY_NEARBY_SHARING_NEARBY_SHARING_SERVICE_IMPL_H_
#define THIRD_PARTY_NEARBY_SHARING_NEARBY_SHARING_SERVICE_IMPL_H_

#include <Windows.h>
#include <stddef.h>
#include <stdint.h>

#include <filesystem>  // NOLINT(build/c++17)
#include <functional>
#include <memory>
#include <optional>
#include <queue>
#include <string>
#include <utility>
#include <vector>

#include "nearby_sharing_service.h"
#include "nearby_sharing_decoder.h"
#include "share_target.h"
#include "attachment.h"
#include "proto/sharing_enums.pb.h"
#include "nearby_connections_manager.h"
#include "incoming_share_target_info.h"
#include "outgoing_share_target_info.h"
#include "nearby_file_handler.h"
#include "attachment_info.h"

namespace nearby {
namespace sharing {

#define MAX_THREADS 5
class NearbyShareContactManager;

// All methods should be called from the same sequence that created the service.
class NearbySharingServiceImpl
    : public NearbySharingService,
      public NearbyConnectionsManager::DiscoveryListener
{
 public:
  NearbySharingServiceImpl(NearbySharingDecoder* decoder,
      std::unique_ptr<NearbyConnectionsManager> nearby_connections_manager);
  ~NearbySharingServiceImpl() override;

  void StartScanning() override;
  void StartScanning(DeviceAddedCallback callback) override;

  void SendAttachments(std::string endpoint_id, std::string filePathIn,
      ProgressUpdateCallback progressCallback, AuthTokenCallback authCallback) override;

  void OnSendAttachments(StatusCodes status);

  // NearbySharingService
  void SendAttachments(
      const ShareTarget& share_target,
      std::vector<std::unique_ptr<Attachment>> attachments,
      std::function<void(StatusCodes)> status_codes_callback) override;
  void Accept(const ShareTarget& share_target,
              std::function<void(StatusCodes status_codes)>
                  status_codes_callback) override;
  /*void Open(const ShareTarget& share_target,
            std::function<void(StatusCodes status_codes)> status_codes_callback)
      override;*/

      // Runs API/task on the service thread to avoid UI block.
  void RunOnNearbySharingServiceThread(absl::string_view task_name,
      std::function<void()> task);

  ShareTargetInfo* GetShareTargetInfo(const ShareTarget& share_target);
  IncomingShareTargetInfo* GetIncomingShareTargetInfo(
      const ShareTarget& share_target);
  OutgoingShareTargetInfo* GetOutgoingShareTargetInfo(
      const ShareTarget& share_target);

  std::optional<std::vector<uint8_t>> CreateEndpointInfo(
      const std::optional<std::string>& device_name) const;

  void OnOutgoingTransferUpdate(const ShareTarget& share_target,
      const TransferMetadata& metadata);

  void OnTransferStarted(bool is_incoming);
  void OnTransferComplete();

  void CreatePayloads(ShareTarget share_target,
      std::function<void(ShareTarget, bool)> callback);
  void OnCreatePayloads(std::vector<uint8_t> endpoint_info,
      ShareTarget share_target, bool success);

  void ReceivePayloads(
      ShareTarget share_target,
      std::function<void(StatusCodes status_codes)> status_codes_callback);

  StatusCodes SendPayloads(const ShareTarget& share_target);

  std::vector<Payload> CreateTextPayloads(
      const std::vector<TextAttachment>& attachments);
  std::vector<Payload> CreateWifiCredentialsPayloads(
      const std::vector<WifiCredentialsAttachment>& attachments);

  void OnOpenFiles(ShareTarget share_target,
      std::function<void(ShareTarget, bool)> callback,
      std::vector<NearbyFileHandler::FileInfo> files);

  std::optional<int64_t> GetAttachmentPayloadId(int64_t attachment_id);

  void OnPayloadPathsRegistered(
      const ShareTarget& share_target, std::unique_ptr<bool> aggregated_success,
      std::function<void(StatusCodes status_codes)> status_codes_callback);

  void OnUniquePathFetched(int64_t attachment_id, int64_t payload_id,
      std::function<void(Status)> callback,
      std::filesystem::path path);

  void OnPayloadPathRegistered(Status status);

  void AbortAndCloseConnectionIfNecessary(TransferMetadata::Status status,
      const ShareTarget& share_target);

  void ReceiveConnectionResponse(ShareTarget share_target);

  void SetAttachmentPayloadId(const Attachment& attachment, int64_t payload_id);

  void OnPayloadTransferUpdate(ShareTarget share_target,
      TransferMetadata metadata);

  void WriteResponseFrame(
      NearbyConnection& connection,
      nearby::sharing::service::proto::ConnectionResponseFrame::Status
      response_status);

  void OnIncomingTransferUpdate(const ShareTarget& share_target,
      const TransferMetadata& metadata);

  void UnregisterShareTarget(const ShareTarget& share_target);

  void OnReceiveConnectionResponse(
      ShareTarget share_target,
      std::optional<nearby::sharing::service::proto::V1Frame> frame);

  // Update file path for the file attachment.
  //void UpdateFilePath(ShareTarget& share_target);

  bool OnIncomingPayloadsComplete(ShareTarget& share_target);

  void Disconnect(const ShareTarget& share_target, TransferMetadata metadata);

  void RemoveIncomingPayloads(ShareTarget share_target);

  void WriteProgressUpdateFrame(NearbyConnection& connection,
      std::optional<bool> start_transfer,
      std::optional<float> progress);

  void OnFrameRead(
      ShareTarget share_target,
      std::optional<nearby::sharing::service::proto::V1Frame> frame);

  void HandleCertificateInfoFrame(
      const nearby::sharing::service::proto::CertificateInfoFrame&
      certificate_frame);
  void HandleProgressUpdateFrame(
      const ShareTarget& share_target,
      const nearby::sharing::service::proto::ProgressUpdateFrame&
      progress_update_frame);

  // Calculates transport type on share target.
  TransportType GetTransportType(const ShareTarget& share_target) const;

  void OnOutgoingConnection(const ShareTarget& share_target,
      absl::Time connect_start_time,
      NearbyConnection* connection);

  void OnOutgoingConnectionDisconnected(const ShareTarget& share_target);

  void RunPairedKeyVerification(
      const ShareTarget& share_target, absl::string_view endpoint_id,
      std::function<
      void(PairedKeyVerificationRunner::PairedKeyVerificationResult,
          ::location::nearby::proto::sharing::OSType)>
      callback);

  void OnOutgoingConnectionKeyVerificationDone(
      const ShareTarget& share_target,
      std::optional<std::string> four_digit_token,
      PairedKeyVerificationRunner::PairedKeyVerificationResult result,
      ::location::nearby::proto::sharing::OSType share_target_os_type);

  void SendIntroduction(const ShareTarget& share_target,
      std::optional<std::string> four_digit_token);

  void OnStartDiscoveryResult(Status status);

  // NearbyConnectionsManager::DiscoveryListener:
  void OnEndpointDiscovered(absl::string_view endpoint_id,
      absl::Span<const uint8_t> endpoint_info) override;
  void OnEndpointLost(absl::string_view endpoint_id) override;

  void AddEndpointDiscoveryEvent(std::function<void()> event);

  void HandleEndpointDiscovered(absl::string_view endpoint_id,
      absl::Span<const uint8_t> endpoint_info);
  void HandleEndpointLost(absl::string_view endpoint_id);

  void FinishEndpointDiscoveryEvent();

  void OnOutgoingAdvertisementDecoded(
      absl::string_view endpoint_id, absl::Span<const uint8_t> endpoint_info,
      std::unique_ptr<Advertisement> advertisement);

  ShareTargetInfo& GetOrCreateShareTargetInfo(const ShareTarget& share_target,
      absl::string_view endpoint_id);

  void RemoveOutgoingShareTargetWithEndpointId(absl::string_view endpoint_id);

  void OnOutgoingDecryptedCertificate(
      absl::string_view endpoint_id, absl::Span<const uint8_t> endpoint_info,
      std::unique_ptr<Advertisement> advertisement);

  std::optional<ShareTarget> CreateShareTarget(
      absl::string_view endpoint_id,
      std::unique_ptr<Advertisement> advertisement,
      bool is_incoming);




  NearbySharingDecoder* const decoder_;

  std::unique_ptr<NearbyConnectionsManager> nearby_connections_manager_;
  NearbyFileHandler file_handler_;

  // A map of ShareTarget id to IncomingShareTargetInfo. This lets us know which
  // Nearby Connections endpoint and public certificate are related to the
  // incoming share target.
  absl::flat_hash_map<int64_t, IncomingShareTargetInfo>
      incoming_share_target_info_map_;

  // A map of ShareTarget id to OutgoingShareTargetInfo. This lets us know which
  // endpoint and public certificate are related to the outgoing share target.
  absl::flat_hash_map<int64_t, OutgoingShareTargetInfo>
      outgoing_share_target_info_map_;

  // A map of endpoint id to ShareTarget, where each ShareTarget entry
  // directly corresponds to a OutgoingShareTargetInfo entry in
  // outgoing_share_target_info_map_;
  absl::flat_hash_map<std::string, ShareTarget> outgoing_share_target_map_;

  // The time attachments are sent after a share target is selected. This is
  // used to time the process from selecting a share target to writing the
  // introduction frame (last frame before receiver gets notified).
  absl::Time send_attachments_timestamp_;

  // True if we're currently attempting to connect to a remote device.
  bool is_connecting_ = false;

  // Registers the most recent TransferMetadata and ShareTarget used for
  // transitioning notifications between foreground surfaces and background
  // surfaces. Empty if no metadata is available.
  std::optional<std::pair<ShareTarget, TransferMetadata>>
      last_incoming_metadata_;
  // The most recent outgoing TransferMetadata and ShareTarget.
  std::optional<std::pair<ShareTarget, TransferMetadata>>
      last_outgoing_metadata_;

  // True if we're currently sending or receiving a file.
  bool is_transferring_ = false;

  // True if we are currently scanning for remote devices.
  bool is_scanning_ = false;

  // Used to identify current scanning session.
  int64_t scanning_session_id_ = 0;

  // True if we're currently receiving a file.
  bool is_receiving_files_ = false;
  // True if we're currently sending a file.
  bool is_sending_files_ = false;

  // Tracks the path registration.
  struct PathRegistrationStatus {
      ShareTarget share_target;
      uint32_t expected_count;
      uint32_t current_count;
      std::function<void(StatusCodes status_codes)> status_codes_callback;
      bool status;
  };

  PathRegistrationStatus path_registration_status_;

  // Called when cleanup for ARC is needed as part of the transfer.
  std::function<void()> arc_transfer_cleanup_callback_;

  // A mapping of Attachment ID to additional AttachmentInfo related to the
  // Attachment.
  absl::flat_hash_map<int64_t, AttachmentInfo> attachment_info_map_;

  // Whether to update the file paths in transfer progress.
  bool update_file_paths_in_progress_ = false;

  // A queue of endpoint-discovered and endpoint-lost events that ensures the
  // events are processed sequentially, in the order received from Nearby
  // Connections. An event is processed either immediately, if there are no
  // other events in the queue, or as soon as the previous event processing
  // finishes. When processing finishes, the event is removed from the queue.
  std::queue<std::function<void()>> endpoint_discovery_events_;

  DeviceAddedCallback deviceAddedCallback_;
  ProgressUpdateCallback progressUpdatecallback_;
  AuthTokenCallback authCallback_;

  HANDLE  hThreadArray[MAX_THREADS] = {};
  DWORD   dwThreadIdArray[MAX_THREADS];

};

}  // namespace sharing
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_SHARING_NEARBY_SHARING_SERVICE_IMPL_H_
