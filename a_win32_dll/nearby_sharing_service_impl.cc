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

#include "nearby_sharing_service_impl.h"

#include <stdint.h>

#include <cstdlib>
#include <ctime>
#include <filesystem>  // NOLINT(build/c++17)
#include <functional>
#include <ios>
#include <memory>
#include <optional>
#include <ostream>
#include <sstream>
#include <string>
#include <utility>
#include <vector>
#include <thread>

#include "absl/algorithm/container.h"
#include "absl/container/flat_hash_map.h"
#include "absl/random/random.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "absl/time/time.h"
#include "absl/types/span.h"

#include "proto/sharing_enums.pb.h"

#include "transfer_update_callback.h"
#include "share_target_info.h"
#include "logging.h"
#include "transfer_metadata_builder.h"
#include "nearby_sharing_util.h"
#include "payload_tracker.h"
#include "constants.h"
#include "hardcoded.h"

namespace nearby {
namespace sharing {
namespace {

    using ::location::nearby::proto::sharing::AttachmentTransmissionStatus;
    using ::location::nearby::proto::sharing::OSType;

constexpr absl::Duration kBackgroundAdvertisementRotationDelayMin =
    absl::Minutes(12);
// 870 seconds represents 14:30 minutes
constexpr absl::Duration kBackgroundAdvertisementRotationDelayMax =
    absl::Seconds(870);
constexpr absl::Duration kInvalidateSurfaceStateDelayAfterTransferDone =
    absl::Milliseconds(3000);
constexpr absl::Duration kProcessShutdownPendingTimerDelay =  // NOLINT
    absl::Seconds(15);
constexpr absl::Duration kProcessNetworkChangeTimerDelay = absl::Seconds(1);

// Cooldown period after a successful incoming share before we allow the "Device
// nearby is sharing" notification to appear again.
constexpr absl::Duration kFastInitiationScannerCooldown = absl::Seconds(8);

// The maximum number of certificate downloads that can be performed during a
// discovery session.
constexpr size_t kMaxCertificateDownloadsDuringDiscovery = 3u;
// The time between certificate downloads during a discovery session. The
// download is only attempted if there are discovered, contact-based
// advertisements that cannot decrypt any currently stored public certificates.
constexpr absl::Duration kCertificateDownloadDuringDiscoveryPeriod =
    absl::Seconds(10);

constexpr absl::string_view kConnectionListenerName = "nearby-share-service";
constexpr absl::string_view kScreenStateListenerName = "nearby-share-service";
constexpr absl::string_view kProfileRelativePath = "Google/Nearby/Sharing";

// Wraps a call to OnTransferUpdate() to filter any updates after receiving a
// final status.
class TransferUpdateDecorator : public TransferUpdateCallback {
 public:
  using Callback =
      std::function<void(const ShareTarget&, const TransferMetadata&)>;

  explicit TransferUpdateDecorator(Callback callback)
      : callback_(std::move(callback)) {}
  TransferUpdateDecorator(const TransferUpdateDecorator&) = delete;
  TransferUpdateDecorator& operator=(const TransferUpdateDecorator&) = delete;
  ~TransferUpdateDecorator() override = default;

  void OnTransferUpdate(const ShareTarget& share_target,
                        const TransferMetadata& transfer_metadata) override {
    if (got_final_status_) {
      // If we already got a final status, we can ignore any subsequent final
      // statuses caused by race conditions.
      //NL_VLOG(1)
      //    << __func__ << ": Transfer update decorator swallowed "
      //    << "status update because a final status was already received: "
      //    << share_target.id << ": "
      //    << TransferMetadata::StatusToString(transfer_metadata.status());
      return;
    }
    got_final_status_ = transfer_metadata.is_final_status();
    callback_(share_target, transfer_metadata);
  }

 private:
  bool got_final_status_ = false;
  Callback callback_;
};

}  // namespace

NearbySharingServiceImpl::NearbySharingServiceImpl(NearbySharingDecoder* decoder,
    std::unique_ptr<NearbyConnectionsManager> nearby_connections_manager)
    : decoder_(decoder),
      nearby_connections_manager_(std::move(nearby_connections_manager))
{
  NL_DCHECK(decoder_);
  NL_DCHECK(nearby_connections_manager_);
}

NearbySharingServiceImpl::~NearbySharingServiceImpl() = default;

DWORD WINAPI SendPayloadWork(LPVOID lpParam)
{
    /*std::function<void()> task = *(std::function<void()>*)(lpParam);
    task();*/
    return 0;
}

void NearbySharingServiceImpl::RunOnNearbySharingServiceThread(
    absl::string_view task_name, std::function<void()> task)
{
    std::thread([task]() {
        task();
        }).detach();


    //hThreadArray[0] = CreateThread(
    //    NULL,                   // default security attributes
    //    0,                      // use default stack size  
    //    SendPayloadWork,       // thread function name
    //    &task,          // argument to thread function 
    //    0,                      // use default creation flags 
    //    &dwThreadIdArray[0]);   // returns the thread identifier 
}

ShareTargetInfo* NearbySharingServiceImpl::GetShareTargetInfo(
    const ShareTarget& share_target) {
    if (share_target.is_incoming)
        return GetIncomingShareTargetInfo(share_target);
    else
        return GetOutgoingShareTargetInfo(share_target);
}

IncomingShareTargetInfo* NearbySharingServiceImpl::GetIncomingShareTargetInfo(
    const ShareTarget& share_target) {
    auto it = incoming_share_target_info_map_.find(share_target.id);
    if (it == incoming_share_target_info_map_.end()) {
        return nullptr;
    }

    return &it->second;
}

OutgoingShareTargetInfo* NearbySharingServiceImpl::GetOutgoingShareTargetInfo(
    const ShareTarget& share_target) {
    auto it = outgoing_share_target_info_map_.find(share_target.id);
    if (it == outgoing_share_target_info_map_.end()) {
        return nullptr;
    }

    return &it->second;
}

int64_t GenerateNextId() {
    absl::BitGen bit_gen;
    return absl::Uniform(bit_gen, 0, INT64_MAX - 1) + 1;
}

std::vector<uint8_t> GenerateRandomBytes(size_t num_bytes) {
    std::vector<uint8_t> bytes(num_bytes);
    crypto::RandBytes(absl::Span<uint8_t>(bytes));
    return bytes;
}

std::optional<std::vector<uint8_t>>
NearbySharingServiceImpl::CreateEndpointInfo(
    const std::optional<std::string>& device_name) const
{
    std::vector<uint8_t> salt;
    std::vector<uint8_t> encrypted_key;

    //if (account_manager_.GetCurrentAccount().has_value()) {
    //    // If the user already signed in, setup contacts certificate for everyone
    //    // mode to show correct user icon on remote device.
    //    DeviceVisibility visibility = settings_->GetVisibility();
    //    if (visibility == proto::DEVICE_VISIBILITY_EVERYONE) {
    //        // Make sure using all contacts certificate for everyone mode
    //        visibility = proto::DEVICE_VISIBILITY_ALL_CONTACTS;
    //    }

    //    std::optional<NearbyShareEncryptedMetadataKey> encrypted_metadata_key =
    //        certificate_manager_->EncryptPrivateCertificateMetadataKey(visibility);
    //    if (encrypted_metadata_key.has_value()) {
    //        salt = encrypted_metadata_key->salt();
    //        encrypted_key = encrypted_metadata_key->encrypted_key();
    //    }
    //    else {
    //        NL_LOG(WARNING) << __func__
    //            << ": Failed to encrypt private certificate metadata key "
    //            << "for advertisement.";
    //    }
    //}

    // Generate random metadata key for non-login user or failed to generate
    // metadata keys for login user.

    if (salt.empty() || encrypted_key.empty()) {
        salt = GenerateRandomBytes(sharing::Advertisement::kSaltSize);
        encrypted_key = GenerateRandomBytes(
            sharing::Advertisement::kMetadataEncryptionKeyHashByteSize);
    }

    ShareTargetType device_type =
        static_cast<ShareTargetType>(hardcoded::GetDeviceType());

    std::unique_ptr<Advertisement> advertisement = Advertisement::NewInstance(
        std::move(salt), std::move(encrypted_key), device_type, device_name);
    if (advertisement) {
        return advertisement->ToEndpointInfo();
    }
    else {
        return std::nullopt;
    }
}

void NearbySharingServiceImpl::SendAttachments(
    const ShareTarget& share_target,
    std::vector<std::unique_ptr<Attachment>> attachments,
    std::function<void(StatusCodes)> status_codes_callback) {
  ShareTarget share_target_copy = share_target;
  for (std::unique_ptr<Attachment>& attachment : attachments) {
    attachment->MoveToShareTarget(share_target_copy);
  }

  RunOnNearbySharingServiceThread(
      "api_send_attachments",
      [&, share_target, share_target_copy = std::move(share_target_copy),
       status_codes_callback = std::move(status_codes_callback)]()
      {
        ShareTargetInfo* info = GetShareTargetInfo(share_target);
        if (!info || !info->endpoint_id()) {
          NL_LOG(WARNING)
              << __func__
              << ": Failed to send attachments. Unknown ShareTarget.";
          std::move(status_codes_callback)(StatusCodes::kError);
          return;
        }

        // Set session ID.

        info->set_session_id(GenerateNextId());

        if (!share_target_copy.has_attachments()) {
          NL_LOG(WARNING) << __func__ << ": No attachments to send.";
          std::move(status_codes_callback)(StatusCodes::kError);
          return;
        }

        // For sending advertisement from scanner, the request advertisement
        // should always be visible to everyone.
        std::optional<std::vector<uint8_t>> endpoint_info =
            CreateEndpointInfo(hardcoded::GetDeviceName());
        if (!endpoint_info) {
          NL_LOG(WARNING) << __func__
                          << ": Could not create local endpoint info.";
          std::move(status_codes_callback)(StatusCodes::kError);
          return;
        }

        info->set_transfer_update_callback(
            std::make_unique<TransferUpdateDecorator>(
                [&](const ShareTarget& share_target,
                    const TransferMetadata& transfer_metadata) {
                  OnOutgoingTransferUpdate(share_target, transfer_metadata);
                }));

        //send_attachments_timestamp_ = context_->GetClock()->Now();
        
        OnTransferStarted(/*is_incoming=*/false);
        is_connecting_ = true;
        
        //InvalidateSendSurfaceState();

        // Send process initialized successfully, from now on status updated
        // will be sent out via OnOutgoingTransferUpdate().
        info->transfer_update_callback()->OnTransferUpdate(
            share_target_copy,
            TransferMetadataBuilder()
                .set_status(TransferMetadata::Status::kConnecting)
                .build());

        CreatePayloads(std::move(share_target_copy),
                       [this, endpoint_info = std::move(*endpoint_info)](
                           ShareTarget share_target, bool success) {
                         // Log analytics event of describing attachments.
                         /*analytics_recorder_->NewDescribeAttachments(
                             share_target.GetAttachments());*/

                         OnCreatePayloads(std::move(endpoint_info),
                                          share_target, success);
                       });

        std::move(status_codes_callback)(StatusCodes::kOk);
      });
}

void NearbySharingServiceImpl::Accept(
    const ShareTarget& share_target,
    std::function<void(StatusCodes status_codes)> status_codes_callback)
{
  RunOnNearbySharingServiceThread(
      "api_accept",
      [&, share_target,
       status_codes_callback = std::move(status_codes_callback)]() {
        // Log analytics event of responding to introduction.
        /*analytics_recorder_->NewRespondToIntroduction(
            ResponseToIntroduction::ACCEPT_INTRODUCTION, receiving_session_id_);*/

        ShareTargetInfo* info = GetShareTargetInfo(share_target);
        if (!info || !info->connection()) {
          NL_LOG(WARNING) << __func__
                          << ": Accept invoked for unknown share target";
          std::move(status_codes_callback)(StatusCodes::kOutOfOrderApiCall);
          return;
        }

        std::optional<std::pair<ShareTarget, TransferMetadata>> metadata =
            share_target.is_incoming ? last_incoming_metadata_
                                     : last_outgoing_metadata_;
        //if (!ReadyToAccept(share_target,
        //                   metadata.has_value()
        //                       ? metadata->second.status()
        //                       : TransferMetadata::Status::kUnknown)) {
        //  NL_LOG(WARNING) << __func__ << ": out of order API call.";
        //  status_codes_callback(StatusCodes::kOutOfOrderApiCall);
        //  return;
        //}

        /*is_waiting_to_record_accept_to_transfer_start_metric_ =
            share_target.is_incoming;*/

        if (share_target.is_incoming)
        {
          //incoming_share_accepted_timestamp_ = context_->GetClock()->Now();

          ReceivePayloads(share_target, std::move(status_codes_callback));
          return;
        }

        std::move(status_codes_callback)(SendPayloads(share_target));
      });
}

void NearbySharingServiceImpl::OnOutgoingTransferUpdate(
    const ShareTarget& share_target, const TransferMetadata& metadata)
{
    // kInProgress status is logged extensively elsewhere so avoid the spam.
    if (metadata.status() != TransferMetadata::Status::kInProgress)
    {
        //NL_VLOG(1) << __func__ << ": Nearby Share service: "
        //    << "Outgoing transfer update for share target with ID "
        //    << share_target.id << ": "
        //    << TransferMetadata::StatusToString(metadata.status());
    }

    if (metadata.status() == TransferMetadata::Status::kInProgress || metadata.is_final_status())
    {
        if (progressUpdatecallback_)
        {
            progressUpdatecallback_(metadata.progress(), metadata.is_final_status());
        }
    }

    OutgoingShareTargetInfo* info = GetOutgoingShareTargetInfo(share_target);
    if (metadata.is_final_status()) {
        // Log analytics event of sending attachment end.
        int64_t sent_bytes =
            share_target.GetTotalAttachmentsSize() * metadata.progress() / 100;
        AttachmentTransmissionStatus transmission_status =
            ConvertToTransmissionStatus(metadata.status());

        if (info == nullptr) {
            // The situation may happen when user cancel connection during
            // establishing connection.
            NL_LOG(INFO) << "No share target info is created for share_target:"
                << share_target.device_name;
        }
        else {
            //analytics_recorder_->NewSendAttachmentsEnd(
            //    info->session_id(), sent_bytes, share_target, transmission_status,
            //    /*transfer_position=*/GetConnectedShareTargetPos(share_target),
            //    /*concurrent_connections=*/GetConnectedShareTargetCount(),
            //    /*duration_millis=*/info->connection_start_time().has_value()
            //    ? absl::ToInt64Milliseconds(context_->GetClock()->Now() -
            //        *(info->connection_start_time()))
            //    : 0,
            //    /*referrer_package=*/std::nullopt,
            //    ConvertToConnectionLayerStatus(info->connection_layer_status()),
            //    info->os_type());
        }
        is_connecting_ = false;
        OnTransferComplete();
    }
    else if (metadata.status() == TransferMetadata::Status::kMediaDownloading ||
        metadata.status() ==
        TransferMetadata::Status::kAwaitingLocalConfirmation) {
        is_connecting_ = false;
        OnTransferStarted(/*is_incoming=*/false);
    }

    /*bool has_foreground_send_surface =
        !foreground_send_transfer_callbacks_.empty();
    ObserverList<TransferUpdateCallback>& transfer_callbacks =
        has_foreground_send_surface ? foreground_send_transfer_callbacks_
        : background_send_transfer_callbacks_;*/
    if (info) {
        // only call transfer update when having share target info.
        /*for (TransferUpdateCallback* callback : transfer_callbacks.GetObservers()) {
            callback->OnTransferUpdate(share_target, metadata);
        }*/

        // check whether need to send next payload.
        if (true/*NearbyFlags::GetInstance().GetBoolFlag(
            config_package_nearby::nearby_sharing_feature::
            kEnableTransferCancellationOptimization)*/) {
            if (metadata.in_progress_attachment_transferred_bytes().has_value() &&
                metadata.in_progress_attachment_total_bytes().has_value() &&
                *metadata.in_progress_attachment_transferred_bytes() ==
                *metadata.in_progress_attachment_total_bytes()) {
                std::optional<Payload> payload = info->ExtractNextPayload();
                if (payload.has_value()) {
                    NL_LOG(INFO) << __func__ << ": Send  payload " << payload->id;
                    nearby_connections_manager_->Send(*info->endpoint_id(),
                        std::make_unique<Payload>(*payload),
                        info->payload_tracker());
                }
                else {
                    NL_LOG(WARNING) << __func__ << ": There is no paylaods to send.";
                }
            }
        }
    }

    if (/*has_foreground_send_surface && */metadata.is_final_status()) {
        last_outgoing_metadata_ = std::nullopt;
    }
    else {
        last_outgoing_metadata_ =
            std::make_pair(share_target, TransferMetadataBuilder::Clone(metadata)
                .set_is_original(false)
                .build());
    }
}

void NearbySharingServiceImpl::OnTransferStarted(bool is_incoming) {
    is_transferring_ = true;
    if (is_incoming) {
        is_receiving_files_ = true;
    }
    else {
        is_sending_files_ = true;
    }
    //InvalidateSurfaceState();
}

void NearbySharingServiceImpl::CreatePayloads(
    ShareTarget share_target, std::function<void(ShareTarget, bool)> callback) {
    OutgoingShareTargetInfo* info = GetOutgoingShareTargetInfo(share_target);
    if (!info || !share_target.has_attachments()) {
        std::move(callback)(std::move(share_target), /*success=*/false);
        return;
    }

    if (!info->file_payloads().empty() || !info->text_payloads().empty() ||
        !info->wifi_credentials_payloads().empty()) {
        // We may have already created the payloads in the case of retry, so we can
        // skip this step.
        std::move(callback)(std::move(share_target), /*success=*/false);
        return;
    }

    info->set_text_payloads(CreateTextPayloads(share_target.text_attachments));
    info->set_wifi_credentials_payloads(
        CreateWifiCredentialsPayloads(share_target.wifi_credentials_attachments));
    if (share_target.file_attachments.empty()) {
        std::move(callback)(std::move(share_target), /*success=*/true);
        return;
    }

    std::vector<std::filesystem::path> file_paths;
    for (const FileAttachment& attachment : share_target.file_attachments) {
        if (!attachment.file_path()) {
            NL_LOG(WARNING) << __func__ << ": Got file attachment without path";
            std::move(callback)(std::move(share_target), /*success=*/false);
            return;
        }
        file_paths.push_back(*attachment.file_path());
    }

    file_handler_.OpenFiles(
        std::move(file_paths),
        [&, share_target = std::move(share_target),
        callback = std::move(callback)](
            std::vector<NearbyFileHandler::FileInfo> file_infos) {
                OnOpenFiles(std::move(share_target), std::move(callback),
                std::move(file_infos));
        });
}

void NearbySharingServiceImpl::ReceivePayloads(
    ShareTarget share_target,
    std::function<void(StatusCodes status_codes)> status_codes_callback) {
    //mutual_acceptance_timeout_alarm_->Stop();

    std::filesystem::path download_path = *hardcoded::GetDownloadPath();

    // Register payload path for all valid file payloads.
    absl::flat_hash_map<int64_t, std::filesystem::path> valid_file_payloads;
    for (auto& file : share_target.file_attachments) {
        std::optional<int64_t> payload_id = GetAttachmentPayloadId(file.id());
        if (!payload_id) {
            NL_LOG(WARNING)
                << __func__
                << ": Failed to register payload path for attachment id - "
                << file.id();
            continue;
        }

        std::filesystem::path file_path =
            download_path / std::filesystem::u8path(file.file_name().cbegin(),
                file.file_name().cend());
        valid_file_payloads.emplace(file.id(), std::move(file_path));
    }

    auto aggregated_success = std::make_unique<bool>(true);

    if (valid_file_payloads.empty()) {
        OnPayloadPathsRegistered(share_target, std::move(aggregated_success),
            std::move(status_codes_callback));
        return;
    }

    path_registration_status_.share_target = share_target;
    path_registration_status_.expected_count = valid_file_payloads.size();
    path_registration_status_.current_count = 0;
    path_registration_status_.status_codes_callback =
        std::move(status_codes_callback);
    path_registration_status_.status = true;

    for (const auto& payload : valid_file_payloads) {
        std::optional<int64_t> payload_id = GetAttachmentPayloadId(payload.first);
        NL_DCHECK(payload_id);

        file_handler_.GetUniquePath(
            payload.second,
            [&, attachment_id = payload.first,
            payload_id = *payload_id](std::filesystem::path unique_path) {
                OnUniquePathFetched(
                    attachment_id, payload_id,
                    [&](Status status) { OnPayloadPathRegistered(status); },
                    unique_path);
            });
    }
}

NearbySharingService::StatusCodes NearbySharingServiceImpl::SendPayloads(
    const ShareTarget& share_target)
{
    //NL_VLOG(1) << __func__ << ": Preparing to send payloads to "
    //    << share_target.id;

    ShareTargetInfo* info = GetShareTargetInfo(share_target);
    if (!info || !info->connection()) {
        NL_LOG(WARNING) << __func__
            << ": Failed to send payload due to missing connection.";
        return StatusCodes::kOutOfOrderApiCall;
    }
    if (!info->transfer_update_callback()) {
        NL_LOG(WARNING)
            << __func__
            << ": Failed to send payload due to missing transfer update "
            "callback. Disconnecting.";
        AbortAndCloseConnectionIfNecessary(
            TransferMetadata::Status::kMissingTransferUpdateCallback, share_target);
        return StatusCodes::kOutOfOrderApiCall;
    }

    // Log analytics event of sending attachment start.
    //analytics_recorder_->NewSendAttachmentsStart(
    //    info->session_id(), share_target.GetAttachments(),
    //    /*transfer_position=*/GetConnectedShareTargetPos(share_target),
    //    /*concurrent_connections=*/GetConnectedShareTargetCount());

    info->transfer_update_callback()->OnTransferUpdate(
        share_target,
        TransferMetadataBuilder()
        .set_token(info->token())
        .set_status(TransferMetadata::Status::kAwaitingRemoteAcceptance)
        .build());

    if (!info->endpoint_id()) {
        NL_LOG(WARNING) << __func__
            << ": Failed to send payload due to missing endpoint id.";
        AbortAndCloseConnectionIfNecessary(
            TransferMetadata::Status::kMissingEndpointId, share_target);
        return StatusCodes::kOutOfOrderApiCall;
    }

    ReceiveConnectionResponse(share_target);
    //OnReceiveConnectionResponse(share_target);
    return StatusCodes::kOk;
}

void NearbySharingServiceImpl::OnTransferComplete() {
    bool was_sending_files = is_sending_files_;
    is_receiving_files_ = false;
    is_transferring_ = false;
    is_sending_files_ = false;

    // Cleanup ARC after send transfer completes since reading from file
    // descriptor(s) are done at this point even though there could be Nearby
    // Connection frames cached that are not yet sent to the remote device.
    if (was_sending_files && arc_transfer_cleanup_callback_) {
        arc_transfer_cleanup_callback_();
    }

    //NL_VLOG(1) << __func__ << ": NearbySharing state change transfer finished";
    // Files transfer is done! Receivers can immediately cancel, but senders
    // should add a short delay to ensure the final in-flight packet(s) make
    // it to the remote device.
    
    /*RunOnNearbySharingServiceThreadDelayed(
        "transfer_done_delay",
        was_sending_files ? kInvalidateSurfaceStateDelayAfterTransferDone
        : absl::Milliseconds(1),
        [&]() { InvalidateSurfaceState(); });*/
}

std::vector<Payload> NearbySharingServiceImpl::CreateTextPayloads(
    const std::vector<TextAttachment>& attachments) {
    std::vector<Payload> payloads;
    payloads.reserve(attachments.size());
    for (const TextAttachment& attachment : attachments) {
        absl::string_view body = attachment.text_body();
        std::vector<uint8_t> bytes(body.begin(), body.end());

        Payload payload{ bytes };
        SetAttachmentPayloadId(attachment, payload.id);
        payloads.push_back(std::move(payload));
    }
    return payloads;
}

std::vector<Payload> NearbySharingServiceImpl::CreateWifiCredentialsPayloads(
    const std::vector<WifiCredentialsAttachment>& attachments) {
    std::vector<Payload> payloads;
    payloads.reserve(attachments.size());
    for (const WifiCredentialsAttachment& attachment : attachments) {
        nearby::sharing::service::proto::WifiCredentials wifi_credentials;
        wifi_credentials.set_password(std::string(attachment.password()));
        wifi_credentials.set_hidden_ssid(attachment.is_hidden());

        std::vector<uint8_t> bytes(wifi_credentials.ByteSizeLong());
        wifi_credentials.SerializeToArray(bytes.data(),
            wifi_credentials.ByteSizeLong());

        Payload payload{ bytes };
        SetAttachmentPayloadId(attachment, payload.id);
        payloads.push_back(std::move(payload));
    }
    return payloads;
}

void NearbySharingServiceImpl::OnOpenFiles(
    ShareTarget share_target, std::function<void(ShareTarget, bool)> callback,
    std::vector<NearbyFileHandler::FileInfo> files) {
    OutgoingShareTargetInfo* info = GetOutgoingShareTargetInfo(share_target);
    if (!info || files.size() != share_target.file_attachments.size()) {
        std::move(callback)(std::move(share_target), /*success=*/false);
        return;
    }

    std::vector<Payload> payloads;
    payloads.reserve(files.size());

    for (size_t i = 0; i < files.size(); ++i) {
        FileAttachment& attachment = share_target.file_attachments[i];
        attachment.set_size(files[i].size);
        InputFile input_file;
        input_file.path = files[i].file_path;
        Payload payload(input_file, attachment.parent_folder());
        payload.content.file_payload.size = files[i].size;
        SetAttachmentPayloadId(attachment, payload.id);
        payloads.push_back(std::move(payload));
    }

    info->set_file_payloads(std::move(payloads));
    std::move(callback)(std::move(share_target), /*success=*/true);
}

std::optional<int64_t> NearbySharingServiceImpl::GetAttachmentPayloadId(
    int64_t attachment_id) {
    auto it = attachment_info_map_.find(attachment_id);
    if (it == attachment_info_map_.end()) return std::nullopt;

    return it->second.payload_id;
}

void NearbySharingServiceImpl::OnPayloadPathsRegistered(
    const ShareTarget& share_target, std::unique_ptr<bool> aggregated_success,
    std::function<void(StatusCodes status_codes)> status_codes_callback) {
    NL_DCHECK(aggregated_success);
    if (!*aggregated_success) {
        NL_LOG(WARNING)
            << __func__
            << ": Not all payload paths could be registered successfully.";
        std::move(status_codes_callback)(StatusCodes::kError);
        return;
    }

    ShareTargetInfo* info = GetShareTargetInfo(share_target);
    if (!info || !info->connection()) {
        NL_LOG(WARNING) << __func__ << ": Accept invoked for unknown share target";
        std::move(status_codes_callback)(StatusCodes::kOutOfOrderApiCall);
        return;
    }
    NearbyConnection* connection = info->connection();

    if (!info->transfer_update_callback()) {
        NL_LOG(WARNING) << __func__
            << ": Accept invoked for share target without transfer "
            "update callback. Disconnecting.";
        AbortAndCloseConnectionIfNecessary(
            TransferMetadata::Status::kMissingTransferUpdateCallback, share_target);
        std::move(status_codes_callback)(StatusCodes::kOutOfOrderApiCall);
        return;
    }

    // Log analytics event of starting to receive payloads.
    /*analytics_recorder_->NewReceiveAttachmentsStart(
        receiving_session_id_, share_target.GetAttachments());*/

    info->set_payload_tracker(std::make_shared<PayloadTracker>(
        share_target, attachment_info_map_,
        [&](ShareTarget share_target, TransferMetadata transfer_metadata) {
            OnPayloadTransferUpdate(share_target, transfer_metadata);
        }));

    // Register status listener for all payloads.
    for (int64_t attachment_id : share_target.GetAttachmentIds()) {
        std::optional<int64_t> payload_id = GetAttachmentPayloadId(attachment_id);
        if (!payload_id) {
            NL_LOG(WARNING) << __func__
                << ": Failed to retrieve payload for attachment id - "
                << attachment_id;
            continue;
        }

        //NL_VLOG(1) << __func__ << ": Started listening for progress on payload - "
        //    << *payload_id;

        nearby_connections_manager_->RegisterPayloadStatusListener(
            *payload_id, info->payload_tracker());

        //NL_VLOG(1) << __func__ << ": Accepted incoming files from share target - "
        //    << share_target.id;
    }

    WriteResponseFrame(
        *connection,
        nearby::sharing::service::proto::ConnectionResponseFrame::ACCEPT);
    
    //NL_VLOG(1) << __func__ << ": Successfully wrote response frame";

    info->transfer_update_callback()->OnTransferUpdate(
        share_target,
        TransferMetadataBuilder()
        .set_status(TransferMetadata::Status::kAwaitingRemoteAcceptance)
        .set_token(info->token())
        .build());

    std::optional<std::string> endpoint_id = info->endpoint_id();
    if (endpoint_id.has_value()) {
        //if (share_target.GetTotalAttachmentsSize() >=
        //    kAttachmentsSizeThresholdOverHighQualityMedium) {
        //    // Upgrade bandwidth regardless of advertising visibility because either
        //    // the system or the user has verified the sender's identity; the
        //    // stable identifiers potentially exposed by performing a bandwidth
        //    // upgrade are no longer a concern.
        //    NL_LOG(INFO) << __func__ << ": Upgrade bandwidth when receiving accept.";
        //    nearby_connections_manager_->UpgradeBandwidth(*endpoint_id);
        //}
    }
    else {
        NL_LOG(WARNING) << __func__
            << ": Failed to initiate bandwidth upgrade. No endpoint_id "
            "found for target - "
            << share_target.id;
        std::move(status_codes_callback)(StatusCodes::kOutOfOrderApiCall);
        return;
    }

    std::move(status_codes_callback)(StatusCodes::kOk);
}

void NearbySharingServiceImpl::OnUniquePathFetched(
    int64_t attachment_id, int64_t payload_id,
    std::function<void(Status)> callback, std::filesystem::path file_path) {
    attachment_info_map_[attachment_id].file_path = file_path;
    nearby_connections_manager_->RegisterPayloadPath(payload_id, file_path,
        std::move(callback));
}

void NearbySharingServiceImpl::OnPayloadPathRegistered(Status status) {
    if (status != Status::kSuccess) {
        path_registration_status_.status = false;
    }

    path_registration_status_.current_count += 1;
    if (path_registration_status_.current_count ==
        path_registration_status_.expected_count) {
        OnPayloadPathsRegistered(
            path_registration_status_.share_target,
            std::make_unique<bool>(path_registration_status_.status),
            std::move(path_registration_status_.status_codes_callback));
    }
}

void NearbySharingServiceImpl::AbortAndCloseConnectionIfNecessary(
    TransferMetadata::Status status, const ShareTarget& share_target) {
    RunOnNearbySharingServiceThread(
        "abort_and_close_connection_if_necessary", [&, status, share_target]() {
            TransferMetadata metadata =
                TransferMetadataBuilder().set_status(status).build();
            ShareTargetInfo* info = GetShareTargetInfo(share_target);

            if (info == nullptr) {
                NL_LOG(WARNING) << ": Share target " << share_target.id << " lost";
                return;
            }

            // First invoke the appropriate transfer callback with the final
            // |status|.
            if (info && info->transfer_update_callback()) {
                info->transfer_update_callback()->OnTransferUpdate(share_target,
                    metadata);
            }
            else if (share_target.is_incoming) {
                OnIncomingTransferUpdate(share_target, metadata);
            }
            else {
                OnOutgoingTransferUpdate(share_target, metadata);
            }

            // Close connection if necessary.
            if (info && info->connection()) {
                // Ensure that the disconnect listener is set to UnregisterShareTarget
                // because the other listeners also try to record a final status
                // metric.
                info->connection()->SetDisconnectionListener([&, share_target]() {
                    RunOnNearbySharingServiceThread(
                        "disconnection_listener",
                        [&, share_target]() { UnregisterShareTarget(share_target); });
                    });

                info->connection()->Close();
            }
        });
}

void NearbySharingServiceImpl::ReceiveConnectionResponse(
    ShareTarget share_target) {
    NL_VLOG(1) << __func__ << ": Receiving response frame from "
        << share_target.id;
    ShareTargetInfo* info = GetShareTargetInfo(share_target);
    NL_DCHECK(info && info->connection());

    info->frames_reader()->ReadFrame(
        nearby::sharing::service::proto::V1Frame::RESPONSE,
        [&, share_target = std::move(share_target)](
            std::optional<nearby::sharing::service::proto::V1Frame> frame) {
                OnReceiveConnectionResponse(share_target, std::move(frame));
        },
        kReadResponseFrameTimeout);
}

void NearbySharingServiceImpl::SetAttachmentPayloadId(
    const Attachment& attachment, int64_t payload_id) {
    attachment_info_map_[attachment.id()].payload_id = payload_id;
}

void NearbySharingServiceImpl::OnPayloadTransferUpdate(
    ShareTarget share_target, TransferMetadata metadata) {
    bool is_in_progress =
        metadata.status() == TransferMetadata::Status::kInProgress;

    /*if (is_in_progress && share_target.is_incoming &&
        is_waiting_to_record_accept_to_transfer_start_metric_) {
        is_waiting_to_record_accept_to_transfer_start_metric_ = false;
    }*/

    // kInProgress status is logged extensively elsewhere so avoid the spam.
    if (!is_in_progress) {
        //NL_VLOG(1) << __func__ << ": Nearby Share service: "
        //    << "Payload transfer update for share target with ID "
        //    << share_target.id << ": "
        //    << TransferMetadata::StatusToString(metadata.status());
    }

    // Update file paths during progress. It may impact transfer speed.
    // TODO: b/289290115 - Revisit UpdateFilePath to enhance transfer speed for
    // MacOS.
    if (false && update_file_paths_in_progress_ && share_target.is_incoming) {
        //UpdateFilePath(share_target);
    }

    if (metadata.status() == TransferMetadata::Status::kComplete &&
        share_target.is_incoming) {
        if (!OnIncomingPayloadsComplete(share_target)) {
            metadata = TransferMetadataBuilder()
                .set_status(TransferMetadata::Status::kIncompletePayloads)
                .build();

            // Reset file paths for file attachments.
            for (auto& file : share_target.file_attachments)
                file.set_file_path(std::nullopt);

            // Reset body of text attachments.
            for (auto& text : share_target.text_attachments)
                text.set_text_body(std::string());

            // Reset password of Wi-Fi credentials attachments.
            for (auto& wifi_credentials : share_target.wifi_credentials_attachments) {
                wifi_credentials.set_password(std::string());
                wifi_credentials.set_is_hidden(false);
            }
        }

        /*if (IsBackgroundScanningFeatureEnabled()) {
            fast_initiation_scanner_cooldown_timer_->Stop();
            fast_initiation_scanner_cooldown_timer_->Start(
                absl::ToInt64Milliseconds(kFastInitiationScannerCooldown), 0, [&]() {
                    fast_initiation_scanner_cooldown_timer_->Stop();
                    InvalidateFastInitiationScanning();
                });
        }*/
    }
    else if (metadata.status() == TransferMetadata::Status::kCancelled &&
        share_target.is_incoming) {
        //NL_VLOG(1) << __func__ << ": Update file paths for cancelled transfer";
        //if (!update_file_paths_in_progress_) {
        //    UpdateFilePath(share_target);
        //}
    }

    // Make sure to call this before calling Disconnect, or we risk losing some
    // transfer updates in the receive case due to the Disconnect call cleaning up
    // share targets.
    ShareTargetInfo* info = GetShareTargetInfo(share_target);
    if (info && info->transfer_update_callback())
        info->transfer_update_callback()->OnTransferUpdate(share_target, metadata);

    // Cancellation has its own disconnection strategy, possibly adding a delay
    // before disconnection to provide the other party time to process the
    // cancellation.
    if (TransferMetadata::IsFinalStatus(metadata.status()) &&
        metadata.status() != TransferMetadata::Status::kCancelled) {
        Disconnect(share_target, metadata);
    }
}

void NearbySharingServiceImpl::WriteResponseFrame(
    NearbyConnection& connection,
    nearby::sharing::service::proto::ConnectionResponseFrame::Status
    response_status) {
    nearby::sharing::service::proto::Frame frame;
    frame.set_version(nearby::sharing::service::proto::Frame::V1);
    nearby::sharing::service::proto::V1Frame* v1_frame = frame.mutable_v1();
    v1_frame->set_type(nearby::sharing::service::proto::V1Frame::RESPONSE);
    v1_frame->mutable_connection_response()->set_status(response_status);

    std::vector<uint8_t> data(frame.ByteSizeLong());
    frame.SerializeToArray(data.data(), frame.ByteSizeLong());

    connection.Write(std::move(data));
}

void NearbySharingServiceImpl::OnIncomingTransferUpdate(
    const ShareTarget& share_target, const TransferMetadata& metadata) {
    // kInProgress status is logged extensively elsewhere so avoid the spam.
    if (metadata.status() != TransferMetadata::Status::kInProgress) {
        //NL_VLOG(1) << __func__ << ": Nearby Share service: "
        //    << "Incoming transfer update for share target with ID "
        //    << share_target.id << ": "
        //    << TransferMetadata::StatusToString(metadata.status());
    }
    if (metadata.status() != TransferMetadata::Status::kCancelled &&
        metadata.status() != TransferMetadata::Status::kRejected) {
        last_incoming_metadata_ =
            std::make_pair(share_target, TransferMetadataBuilder::Clone(metadata)
                .set_is_original(false)
                .build());
    }
    else {
        last_incoming_metadata_ = std::nullopt;
    }

    if (metadata.is_final_status()) {
        // Log analytics event of receiving attachment end.
        int64_t received_bytes =
            share_target.GetTotalAttachmentsSize() * metadata.progress() / 100;
        AttachmentTransmissionStatus transmission_status =
            ConvertToTransmissionStatus(metadata.status());

        //analytics_recorder_->NewReceiveAttachmentsEnd(
        //    receiving_session_id_, received_bytes, transmission_status,
        //    /* referrer_package=*/std::nullopt);

        OnTransferComplete();
        if (metadata.status() != TransferMetadata::Status::kComplete) {
            // For any type of failure, lets make sure any pending files get cleaned
            // up.
            RemoveIncomingPayloads(share_target);
        }
    }
    else if (metadata.status() ==
        TransferMetadata::Status::kAwaitingLocalConfirmation) {
        OnTransferStarted(/*is_incoming=*/true);
    }

    /*ObserverList<TransferUpdateCallback>& transfer_callbacks =
        foreground_receive_callbacks_.empty() ? background_receive_callbacks_
        : foreground_receive_callbacks_;

    for (TransferUpdateCallback* callback : transfer_callbacks.GetObservers()) {
        callback->OnTransferUpdate(share_target, metadata);
    }*/
}

void NearbySharingServiceImpl::UnregisterShareTarget(
    const ShareTarget& share_target) {
    //NL_VLOG(1) << __func__ << ": Unregistering share target - "
    //    << share_target.id;

    // For metrics.
    //all_cancelled_share_target_ids_.erase(share_target.id);

    if (share_target.is_incoming) {
        if (last_incoming_metadata_ &&
            last_incoming_metadata_->first.id == share_target.id) {
            last_incoming_metadata_.reset();
        }

        // Clear legacy incoming payloads to release resources.
        nearby_connections_manager_->ClearIncomingPayloads();
        incoming_share_target_info_map_.erase(share_target.id);
    }
    else {
        if (last_outgoing_metadata_ &&
            last_outgoing_metadata_->first.id == share_target.id) {
            last_outgoing_metadata_.reset();
        }
        // Find the endpoint id that matches the given share target.
        std::optional<std::string> endpoint_id;
        auto it = outgoing_share_target_info_map_.find(share_target.id);
        if (it != outgoing_share_target_info_map_.end())
            endpoint_id = it->second.endpoint_id();

        /*if (endpoint_id.has_value()) {
            RemoveOutgoingShareTargetWithEndpointId(*endpoint_id);
            mutual_acceptance_timeout_alarm_->Stop();
            return;
        }*/

        // Be careful not to clear out the share target info map if a new session
        // was started during the cancellation delay.
        /*if (!is_scanning_ && !is_transferring_) {
            ClearOutgoingShareTargetInfoMap();
        }*/

        //NL_VLOG(1) << __func__ << ": Unregister share target: " << share_target.id;
    }
    //mutual_acceptance_timeout_alarm_->Stop();
}

void NearbySharingServiceImpl::OnReceiveConnectionResponse(
    ShareTarget share_target,
    std::optional<nearby::sharing::service::proto::V1Frame> frame) {
    OutgoingShareTargetInfo* info = GetOutgoingShareTargetInfo(share_target);
    if (!info || !info->connection()) {
        NL_LOG(WARNING) << __func__
            << ": Ignore received connection response, due to no "
            "connection established.";
        return;
    }

    if (!info->transfer_update_callback()) {
        NL_LOG(WARNING) << __func__
            << ": No transfer update callback. Disconnecting.";
        AbortAndCloseConnectionIfNecessary(
            TransferMetadata::Status::kMissingTransferUpdateCallback, share_target);
        return;
    }

    if (!frame) {
        NL_LOG(WARNING)
            << __func__
            << ": Failed to read a response from the remote device. Disconnecting.";
        AbortAndCloseConnectionIfNecessary(
            TransferMetadata::Status::kFailedToReadOutgoingConnectionResponse,
            share_target);
        return;
    }

    //mutual_acceptance_timeout_alarm_->Stop();

    NL_VLOG(1) << __func__
        << ": Successfully read the connection response frame.";

    nearby::sharing::service::proto::ConnectionResponseFrame response =
        std::move(frame->connection_response());
    switch (response.status()) {
    case nearby::sharing::service::proto::ConnectionResponseFrame::ACCEPT: {
        // Write progress update frame to remote machine.
        WriteProgressUpdateFrame(*info->connection(), true, std::nullopt);

        info->frames_reader()->ReadFrame(
            [&, share_target](
                std::optional<nearby::sharing::service::proto::V1Frame> frame) {
                    OnFrameRead(share_target, std::move(frame));
            });

        info->transfer_update_callback()->OnTransferUpdate(
            share_target, TransferMetadataBuilder()
            .set_status(TransferMetadata::Status::kInProgress)
            .build());

        info->set_payload_tracker(std::make_unique<PayloadTracker>(
            share_target, attachment_info_map_,
            [&](ShareTarget share_target, TransferMetadata transfer_metadata) {
                OnPayloadTransferUpdate(share_target, transfer_metadata);
            }));

        if (false/*NearbyFlags::GetInstance().GetBoolFlag(
            config_package_nearby::nearby_sharing_feature::
            kEnableTransferCancellationOptimization)*/) {
            std::optional<Payload> payload = info->ExtractNextPayload();
            if (payload.has_value()) {
                NL_LOG(INFO) << __func__ << ": Send  payload " << payload->id;

                nearby_connections_manager_->Send(*info->endpoint_id(),
                    std::make_unique<Payload>(*payload),
                    info->payload_tracker());
            }
            else {
                NL_LOG(WARNING) << __func__ << ": There is no payloads to send.";
            }
        }
        else {
            for (auto& payload : info->ExtractTextPayloads()) {
                nearby_connections_manager_->Send(*info->endpoint_id(),
                    std::make_unique<Payload>(payload),
                    info->payload_tracker());
            }
            for (auto& payload : info->ExtractFilePayloads()) {
                nearby_connections_manager_->Send(*info->endpoint_id(),
                    std::make_unique<Payload>(payload),
                    info->payload_tracker());
            }
        }
        NL_VLOG(1)
            << __func__
            << ": The connection was accepted. Payloads are now being sent.";
        break;
    }
    case nearby::sharing::service::proto::ConnectionResponseFrame::REJECT:
        AbortAndCloseConnectionIfNecessary(TransferMetadata::Status::kRejected,
            share_target);
        NL_VLOG(1)
            << __func__
            << ": The connection was rejected. The connection has been closed.";
        break;
        case nearby::sharing::service::proto::ConnectionResponseFrame::
        NOT_ENOUGH_SPACE:
            AbortAndCloseConnectionIfNecessary(
                TransferMetadata::Status::kNotEnoughSpace, share_target);
            NL_VLOG(1) << __func__
                << ": The connection was rejected because the remote device "
                "does not have enough space for our attachments. The "
                "connection has been closed.";
            break;
            case nearby::sharing::service::proto::ConnectionResponseFrame::
            UNSUPPORTED_ATTACHMENT_TYPE:
                AbortAndCloseConnectionIfNecessary(
                    TransferMetadata::Status::kUnsupportedAttachmentType, share_target);
                NL_VLOG(1) << __func__
                    << ": The connection was rejected because the remote device "
                    "does not support the attachments we were sending. The "
                    "connection has been closed.";
                break;
            case nearby::sharing::service::proto::ConnectionResponseFrame::TIMED_OUT:
                AbortAndCloseConnectionIfNecessary(TransferMetadata::Status::kTimedOut,
                    share_target);
                NL_VLOG(1) << __func__
                    << ": The connection was rejected because the remote device "
                    "timed out. The connection has been closed.";
                break;
            default:
                AbortAndCloseConnectionIfNecessary(TransferMetadata::Status::kFailed,
                    share_target);
                NL_VLOG(1) << __func__
                    << ": The connection failed. The connection has been closed.";
                break;
    }
}

bool NearbySharingServiceImpl::OnIncomingPayloadsComplete(
    ShareTarget& share_target) {
    NL_DCHECK(share_target.is_incoming);

    ShareTargetInfo* info = GetShareTargetInfo(share_target);
    if (!info || !info->connection()) {
        //NL_VLOG(1) << __func__ << ": Connection not found for target - "
        //    << share_target.id;

        return false;
    }
    NearbyConnection* connection = info->connection();

    connection->SetDisconnectionListener([&, share_target]() {
        RunOnNearbySharingServiceThread(
            "disconnection_listener",
            [&, share_target]() { UnregisterShareTarget(share_target); });
        });

    if (!update_file_paths_in_progress_) {
        //UpdateFilePath(share_target);
    }

    for (auto& text : share_target.text_attachments) {
        AttachmentInfo& attachment_info = attachment_info_map_[text.id()];
        std::optional<int64_t> payload_id = attachment_info.payload_id;
        if (!payload_id) {
            NL_LOG(WARNING) << __func__ << ": No payload id found for text - "
                << text.id();
            return false;
        }

        Payload* incoming_payload =
            nearby_connections_manager_->GetIncomingPayload(*payload_id);
        if (!incoming_payload || !incoming_payload->content.is_bytes()) {
            NL_LOG(WARNING) << __func__ << ": No payload found for text - "
                << text.id();
            return false;
        }

        std::vector<uint8_t> bytes = incoming_payload->content.bytes_payload.bytes;
        if (bytes.empty()) {
            NL_LOG(WARNING)
                << __func__
                << ": Incoming bytes is empty for text payload with payload_id - "
                << *payload_id;
            return false;
        }

        std::string text_body(bytes.begin(), bytes.end());
        text.set_text_body(text_body);

        attachment_info.text_body = std::move(text_body);
    }

    for (auto& wifi_credentials_attachment :
        share_target.wifi_credentials_attachments) {
        AttachmentInfo& attachment_info =
            attachment_info_map_[wifi_credentials_attachment.id()];
        std::optional<int64_t> payload_id = attachment_info.payload_id;
        if (!payload_id) {
            NL_LOG(WARNING) << __func__
                << ": No payload id found for WiFi credentials - "
                << wifi_credentials_attachment.id();
            return false;
        }

        Payload* incoming_payload =
            nearby_connections_manager_->GetIncomingPayload(*payload_id);
        if (!incoming_payload || !incoming_payload->content.is_bytes()) {
            NL_LOG(WARNING) << __func__
                << ": No payload found for WiFi credentials - "
                << wifi_credentials_attachment.id();
            return false;
        }

        std::vector<uint8_t> bytes = incoming_payload->content.bytes_payload.bytes;
        if (bytes.empty()) {
            NL_LOG(WARNING) << __func__
                << ": Incoming bytes is empty for WiFi credentials "
                "payload with payload_id - "
                << *payload_id;
            return false;
        }

        auto wifi_credentials =
            std::make_unique<nearby::sharing::service::proto::WifiCredentials>();
        if (!wifi_credentials->ParseFromArray(bytes.data(), bytes.size())) {
            NL_LOG(WARNING) << __func__
                << ": Incoming bytes is invalid for WiFi credentials "
                "payload with payload_id - "
                << *payload_id;
            return false;
        }

        wifi_credentials_attachment.set_password(wifi_credentials->password());
        wifi_credentials_attachment.set_is_hidden(wifi_credentials->hidden_ssid());
    }

    return true;
}

void NearbySharingServiceImpl::Disconnect(const ShareTarget& share_target,
    TransferMetadata metadata) {
    ShareTargetInfo* share_target_info = GetShareTargetInfo(share_target);
    if (!share_target_info) {
        NL_LOG(WARNING)
            << __func__
            << ": Failed to disconnect. No share target info found for target - "
            << share_target.id;
        return;
    }

    std::optional<std::string> endpoint_id = share_target_info->endpoint_id();
    if (!endpoint_id.has_value()) {
        NL_LOG(WARNING)
            << __func__
            << ": Failed to disconnect. No endpoint id found for share target - "
            << share_target.id;
        return;
    }

    // Failed to send or receive. No point in continuing, so disconnect
    // immediately.
    if (metadata.status() != TransferMetadata::Status::kComplete) {
        if (share_target_info->connection()) {
            share_target_info->connection()->Close();
        }
        else {
            nearby_connections_manager_->Disconnect(*endpoint_id);
        }
        return;
    }

    // Files received successfully. Receivers can immediately cancel.
    if (share_target.is_incoming) {
        if (share_target_info->connection()) {
            share_target_info->connection()->Close();
        }
        else {
            nearby_connections_manager_->Disconnect(*endpoint_id);
        }
        return;
    }

    // Disconnect after a timeout to make sure any pending payloads are sent.
    //
    // We assign endpoint_id = *endpoint_id here since endpoint_id is
    // std::optional<std::string> so the lambda will capture the string by value.
    // For absl::string_view, please make it sure you wrap the string_view object
    // with std::string() so that it captures the string by value correctly.
    /*auto timer = context_->CreateTimer();
    timer->Start(absl::ToInt64Milliseconds(kOutgoingDisconnectionDelay), 0,
        [&, endpoint_id = *endpoint_id]() {
            OnDisconnectingConnectionTimeout(endpoint_id);
        });

    disconnection_timeout_alarms_[*endpoint_id] = std::move(timer);*/

    // Stop the disconnection timeout if the connection has been closed already.
    //
    // We assign endpoint_id = *endpoint_id here since endpoint_id is
    // std::optional<std::string> so the lambda will capture the string by value.
    // For absl::string_view, please make it sure you wrap the string_view object
    // with std::string() so that it captures the string by value correctly.
    /*if (share_target_info->connection()) {
        share_target_info->connection()->SetDisconnectionListener(
            [&, share_target, share_target_info, endpoint_id = *endpoint_id]() {
                share_target_info->set_connection(nullptr);
                RunOnNearbySharingServiceThread(
                    "disconnection_listener", [&, share_target, endpoint_id]() {
                        OnDisconnectingConnectionDisconnected(share_target,
                        endpoint_id);
                    });
            });
    }*/
}

void NearbySharingServiceImpl::RemoveIncomingPayloads(
    ShareTarget share_target) {
    if (!share_target.is_incoming) {
        return;
    }

    NL_LOG(INFO) << __func__ << ": Cleaning up payloads due to transfer failure";
    nearby_connections_manager_->ClearIncomingPayloads();
    std::vector<std::filesystem::path> files_for_deletion;
    for (const auto& file : share_target.file_attachments) {
        if (!file.file_path().has_value()) continue;
        auto file_path = *file.file_path();
        //NL_VLOG(1) << __func__
        //    << ": file_path=" << GetCompatibleU8String(file_path.u8string());
        if (attachment_info_map_.find(file.id()) == attachment_info_map_.end()) {
            continue;
        }
        files_for_deletion.push_back(file_path);
    }
    file_handler_.DeleteFilesFromDisk(std::move(files_for_deletion), []() {});
}

void NearbySharingServiceImpl::WriteProgressUpdateFrame(
    NearbyConnection& connection, std::optional<bool> start_transfer,
    std::optional<float> progress) {
    NL_LOG(INFO) << __func__ << ": Writing progress update frame. start_transfer="
        << (start_transfer.has_value() ? *start_transfer : false)
        << ", progress=" << (progress.has_value() ? *progress : 0.0);
    nearby::sharing::service::proto::Frame frame;
    frame.set_version(nearby::sharing::service::proto::Frame::V1);
    nearby::sharing::service::proto::V1Frame* v1_frame = frame.mutable_v1();
    v1_frame->set_type(nearby::sharing::service::proto::V1Frame::PROGRESS_UPDATE);
    nearby::sharing::service::proto::ProgressUpdateFrame* progress_frame =
        v1_frame->mutable_progress_update();
    if (start_transfer.has_value()) {
        progress_frame->set_start_transfer(*start_transfer);
    }
    if (progress.has_value()) {
        progress_frame->set_progress(*progress);
    }

    std::vector<uint8_t> data(frame.ByteSizeLong());
    frame.SerializeToArray(data.data(), frame.ByteSizeLong());

    connection.Write(std::move(data));
}

void NearbySharingServiceImpl::OnFrameRead(
    ShareTarget share_target,
    std::optional<nearby::sharing::service::proto::V1Frame> frame) {
    if (!frame.has_value()) {
        // This is the case when the connection has been closed since we wait
        // indefinitely for incoming frames.
        return;
    }

    switch (frame->type()) {
    case nearby::sharing::service::proto::V1Frame::CANCEL:
        //RunOnAnyThread("cancel_transfer", [&, share_target]() {
        //    NL_LOG(INFO) << __func__
        //        << ": Read the cancel frame, closing connection";
        //    DoCancel(
        //        share_target, [&](StatusCodes status_codes) {},
        //        /*is_initiator_of_cancellation=*/false);
        //    });
        //break;

    case nearby::sharing::service::proto::V1Frame::CERTIFICATE_INFO:
        HandleCertificateInfoFrame(frame->certificate_info());
        break;

    case nearby::sharing::service::proto::V1Frame::PROGRESS_UPDATE:
        HandleProgressUpdateFrame(share_target, frame->progress_update());
        break;

    default:
        //NL_LOG(ERROR) << __func__ << ": Discarding unknown frame of type";
        break;
    }

    ShareTargetInfo* info = GetShareTargetInfo(share_target);
    if (!info || !info->frames_reader()) {
        NL_LOG(WARNING) << __func__
            << ": Stopped reading further frames, due to no connection "
            "established.";
        return;
    }

    info->frames_reader()->ReadFrame(
        [&, share_target = std::move(share_target)](
            std::optional<nearby::sharing::service::proto::V1Frame> frame) {
                OnFrameRead(share_target, std::move(frame));
        });
}

void NearbySharingServiceImpl::HandleCertificateInfoFrame(
    const nearby::sharing::service::proto::CertificateInfoFrame&
    certificate_frame) {
}

void NearbySharingServiceImpl::HandleProgressUpdateFrame(
    const ShareTarget& share_target,
    const nearby::sharing::service::proto::ProgressUpdateFrame&
    progress_update_frame) {
    if (progress_update_frame.has_start_transfer() &&
        progress_update_frame.start_transfer()) {
        ShareTargetInfo* info = GetShareTargetInfo(share_target);

        if (info != nullptr && info->endpoint_id().has_value() &&
            share_target.GetTotalAttachmentsSize() >=
            kAttachmentsSizeThresholdOverHighQualityMedium) {
            NL_LOG(INFO)
                << __func__
                << ": Upgrade bandwidth when receiving progress update frame "
                "for endpoint "
                << (*info->endpoint_id());
            //nearby_connections_manager_->UpgradeBandwidth(*info->endpoint_id());
        }
    }

    if (progress_update_frame.has_progress()) {
        NL_LOG(WARNING) << __func__ << ": Current progress for ShareTarget "
            << share_target.id << " is "
            << progress_update_frame.progress();
    }
}

void NearbySharingServiceImpl::OnCreatePayloads(
    std::vector<uint8_t> endpoint_info, ShareTarget share_target,
    bool success) {
    OutgoingShareTargetInfo* info = GetOutgoingShareTargetInfo(share_target);
    bool has_payloads = info && (!info->text_payloads().empty() ||
        !info->file_payloads().empty());
    if (!success || !has_payloads || !info->endpoint_id()) {
        NL_LOG(WARNING) << __func__
            << ": Failed to send file to remote ShareTarget. Failed to "
            "create payloads.";
        if (info && info->transfer_update_callback()) {
            info->transfer_update_callback()->OnTransferUpdate(
                share_target,
                TransferMetadataBuilder()
                .set_status(TransferMetadata::Status::kMediaUnavailable)
                .build());
        }
        return;
    }

    std::optional<std::vector<uint8_t>> bluetooth_mac_address =
        hardcoded::GetBluetoothMacAddressForShareTarget(share_target);

    // For metrics.
    //all_cancelled_share_target_ids_.clear();

    //info->set_connection_start_time(context_->GetClock()->Now());

    nearby_connections_manager_->Connect(
        std::move(endpoint_info), *info->endpoint_id(),
        std::move(bluetooth_mac_address), hardcoded::GetDataUsage(),
        GetTransportType(share_target),
        [&, share_target, info](NearbyConnection* connection, Status status) {
            // Log analytics event of new connection.
            info->set_connection_layer_status(status);
            //if (connection == nullptr) {
            //    analytics_recorder_->NewEstablishConnection(
            //        info->session_id(),
            //        EstablishConnectionStatus::CONNECTION_STATUS_FAILURE,
            //        share_target,
            //        /*transfer_position=*/GetConnectedShareTargetPos(share_target),
            //        /*concurrent_connections=*/GetConnectedShareTargetCount(),
            //        info->connection_start_time().has_value()
            //        ? absl::ToInt64Milliseconds(context_->GetClock()->Now() -
            //            *(info->connection_start_time()))
            //        : 0,
            //        std::nullopt);
            //}

            OnOutgoingConnection(share_target, absl::Now(),
                connection);
        });
}

TransportType NearbySharingServiceImpl::GetTransportType(
    const ShareTarget& share_target) const {
    if (share_target.GetTotalAttachmentsSize() >
        kAttachmentsSizeThresholdOverHighQualityMedium) {
        NL_LOG(INFO) << __func__ << ": Transport type is kHighQuality";
        return TransportType::kHighQuality;
    }

    if (share_target.file_attachments.empty()) {
        NL_LOG(INFO) << __func__ << ": Transport type is kNonDisruptive";
        return TransportType::kNonDisruptive;
    }

    NL_LOG(INFO) << __func__ << ": Transport type is kAny";
    return TransportType::kAny;
}

void NearbySharingServiceImpl::OnOutgoingConnection(
    const ShareTarget& share_target, absl::Time connect_start_time,
    NearbyConnection* connection) {
    OutgoingShareTargetInfo* info = GetOutgoingShareTargetInfo(share_target);
    bool success = info && info->endpoint_id() && connection;

    if (!success) {
        NL_LOG(WARNING) << __func__
            << ": Failed to initiate connection to share target "
            << share_target.id;
        TransferMetadata::Status transfer_status =
            TransferMetadata::Status::kFailedToInitiateOutgoingConnection;
        if (info != nullptr &&
            info->connection_layer_status() == Status::kTimeout) {
            transfer_status = TransferMetadata::Status::kTimedOut;
            info->set_connection_layer_status(Status::kUnknown);
        }
        AbortAndCloseConnectionIfNecessary(transfer_status, share_target);
        return;
    }

    info->set_connection(connection);

    // Log analytics event of establishing connection.
    //analytics_recorder_->NewEstablishConnection(
    //    info->session_id(), EstablishConnectionStatus::CONNECTION_STATUS_SUCCESS,
    //    share_target,
    //    /*transfer_position=*/GetConnectedShareTargetPos(share_target),
    //    /*concurrent_connections=*/GetConnectedShareTargetCount(),
    //    info->connection_start_time().has_value()
    //    ? absl::ToInt64Milliseconds((context_->GetClock()->Now() -
    //        *(info->connection_start_time())))
    //    : 0,
    //    std::nullopt);

    connection->SetDisconnectionListener([&, share_target]() {
        RunOnNearbySharingServiceThread(
            "disconnection_listener", [&, share_target]() {
                OnOutgoingConnectionDisconnected(share_target);
            });
        });

    std::optional<std::string> four_digit_token = TokenToFourDigitString(
        nearby_connections_manager_->GetRawAuthenticationToken(
            *info->endpoint_id()));

    if (authCallback_ && four_digit_token.has_value())
    {
        authCallback_(four_digit_token.value());
    }

    RunPairedKeyVerification(
        share_target, *info->endpoint_id(),
        [&, share_target, four_digit_token = std::move(four_digit_token)](
            PairedKeyVerificationRunner::PairedKeyVerificationResult result,
            OSType remote_os_type) {
                OnOutgoingConnectionKeyVerificationDone(share_target, four_digit_token,
                result, remote_os_type);
        });
}

void NearbySharingServiceImpl::OnOutgoingConnectionDisconnected(
    const ShareTarget& share_target) {
    ShareTargetInfo* info = GetShareTargetInfo(share_target);
    if (info && info->transfer_update_callback()) {
        info->transfer_update_callback()->OnTransferUpdate(
            share_target,
            TransferMetadataBuilder()
            .set_status(TransferMetadata::Status::kUnexpectedDisconnection)
            .build());
    }
    UnregisterShareTarget(share_target);
}

void NearbySharingServiceImpl::RunPairedKeyVerification(
    const ShareTarget& share_target, absl::string_view endpoint_id,
    std::function<void(PairedKeyVerificationRunner::PairedKeyVerificationResult,
        OSType)>
    callback) {
    std::optional<std::vector<uint8_t>> token =
        nearby_connections_manager_->GetRawAuthenticationToken(endpoint_id);
    if (!token) {
        NL_VLOG(1) << __func__
            << ":LOGINFO Failed to read authentication token from endpoint - "
            << endpoint_id;
        std::move(callback)(
            PairedKeyVerificationRunner::PairedKeyVerificationResult::kFail,
            OSType::UNKNOWN_OS_TYPE);
        return;
    }

    ShareTargetInfo* share_target_info = GetShareTargetInfo(share_target);
    NL_DCHECK(share_target_info);

    share_target_info->set_frames_reader(std::make_shared<IncomingFramesReader>(
        decoder_, share_target_info->connection()));

    bool restrict_to_contacts = false;
    bool self_share_feature_enabled = false;
    share_target_info->set_key_verification_runner(
        std::make_shared<PairedKeyVerificationRunner>(
            self_share_feature_enabled, share_target, endpoint_id, *token,
            share_target_info->connection(), restrict_to_contacts,
            share_target_info->frames_reader(), kReadFramesTimeout));
    share_target_info->key_verification_runner()->Run(std::move(callback));
}

void NearbySharingServiceImpl::OnOutgoingConnectionKeyVerificationDone(
    const ShareTarget& share_target,
    std::optional<std::string> four_digit_token,
    PairedKeyVerificationRunner::PairedKeyVerificationResult result,
    OSType share_target_os_type) {
    ShareTargetInfo* info = GetShareTargetInfo(share_target);
    if (!info || !info->connection()) {
        return;
    }

    if (!info->transfer_update_callback()) {
        NL_VLOG(1) << __func__ << ": No transfer update callback. Disconnecting.";
        AbortAndCloseConnectionIfNecessary(
            TransferMetadata::Status::kMissingTransferUpdateCallback, share_target);
        return;
    }

    info->set_os_type(share_target_os_type);

    switch (result) {
    case PairedKeyVerificationRunner::PairedKeyVerificationResult::kFail:
        NL_VLOG(1) << __func__ << ":LOGINFO Paired key handshake failed for target "
            << share_target.id << ". Disconnecting.";
        AbortAndCloseConnectionIfNecessary(
            TransferMetadata::Status::kPairedKeyVerificationFailed, share_target);
        return;

    case PairedKeyVerificationRunner::PairedKeyVerificationResult::kSuccess:
        NL_VLOG(1) << __func__ << ":LOGINFO Paired key handshake succeeded for target - "
            << share_target.id;
        SendIntroduction(share_target, /*four_digit_token=*/std::nullopt);
        SendPayloads(share_target);
        return;

    case PairedKeyVerificationRunner::PairedKeyVerificationResult::kUnable:
        NL_VLOG(1) << __func__
            << ":LOGINFO Unable to verify paired key encryption when "
            "initiating connection to target - "
            << share_target.id;

        if (four_digit_token) {
            info->set_token(*four_digit_token);
        }

        if (true/*NearbyFlags::GetInstance().GetBoolFlag(
            config_package_nearby::nearby_sharing_feature::
            kSenderSkipsConfirmation)*/) {
            NL_VLOG(1) << __func__
                << ":LOGINFO Sender-side verification is disabled. Skipping "
                "token comparison with "
                << share_target.id;
            SendIntroduction(share_target, /*four_digit_token=*/std::nullopt);
            SendPayloads(share_target);
        }
        else {
            SendIntroduction(share_target, std::move(four_digit_token));
        }
        return;

    case PairedKeyVerificationRunner::PairedKeyVerificationResult::kUnknown:
        NL_VLOG(1) << __func__
            << ": Unknown PairedKeyVerificationResult for target "
            << share_target.id << ". Disconnecting.";
        AbortAndCloseConnectionIfNecessary(
            TransferMetadata::Status::kPairedKeyVerificationFailed, share_target);
        break;
    }
}

void NearbySharingServiceImpl::SendIntroduction(
    const ShareTarget& share_target,
    std::optional<std::string> four_digit_token) {
    // We successfully connected! Now lets build up Payloads for all the files we
    // want to send them. We won't send any just yet, but we'll send the Payload
    // IDs in our introduction frame so that they know what to expect if they
    // accept.
    //NL_VLOG(1) << __func__ << ": Preparing to send introduction to "
    //    << share_target.id;

    ShareTargetInfo* info = GetShareTargetInfo(share_target);
    if (!info || !info->connection()) {
        NL_LOG(WARNING) << __func__ << ": No NearbyConnection tied to "
            << share_target.id;
        return;
    }

    // Log analytics event of sending introduction.
    //analytics_recorder_->NewSendIntroduction(
    //    info->session_id(), share_target,
    //    /*transfer_position=*/GetConnectedShareTargetPos(share_target),
    //    /*concurrent_connections=*/GetConnectedShareTargetCount(),
    //    info->os_type());

    NearbyConnection* connection = info->connection();

    if (!info->transfer_update_callback()) {
        NL_LOG(WARNING) << __func__
            << ": No transfer update callback, disconnecting.";
        AbortAndCloseConnectionIfNecessary(
            TransferMetadata::Status::kMissingTransferUpdateCallback, share_target);
        return;
    }

    //if (foreground_send_transfer_callbacks_.empty() &&
    //    background_send_transfer_callbacks_.empty()) {
    //    NL_LOG(WARNING) << __func__ << ": No transfer callbacks, disconnecting.";
    //    connection->Close();
    //    return;
    //}

    // Build the introduction.
    auto introduction =
        std::make_unique<nearby::sharing::service::proto::IntroductionFrame>();
    introduction->set_start_transfer(true);
    //NL_VLOG(1) << __func__ << ": Sending attachments to " << share_target.id;

    // Write introduction of file payloads.
    for (const auto& file : share_target.file_attachments) {
        std::optional<int64_t> payload_id = GetAttachmentPayloadId(file.id());
        if (!payload_id) {
            //NL_VLOG(1) << __func__ << ": Skipping unknown file attachment";
            continue;
        }
        auto* file_metadata = introduction->add_file_metadata();
        file_metadata->set_id(file.id());
        file_metadata->set_name(absl::StrCat(file.file_name()));
        file_metadata->set_payload_id(*payload_id);
        file_metadata->set_type(file.type());
        file_metadata->set_mime_type(absl::StrCat(file.mime_type()));
        file_metadata->set_size(file.size());
    }

    // Write introduction of text payloads.
    for (const auto& text : share_target.text_attachments) {
        std::optional<int64_t> payload_id = GetAttachmentPayloadId(text.id());
        if (!payload_id) {
            //NL_VLOG(1) << __func__ << ": Skipping unknown text attachment";
            continue;
        }
        auto* text_metadata = introduction->add_text_metadata();
        text_metadata->set_id(text.id());
        text_metadata->set_text_title(std::string(text.text_title()));
        text_metadata->set_type(text.type());
        text_metadata->set_size(text.size());
        text_metadata->set_payload_id(*payload_id);
    }

    // Write introduction of Wi-Fi credentials payloads.
    for (const auto& wifi_credentials :
        share_target.wifi_credentials_attachments) {
        std::optional<int64_t> payload_id =
            GetAttachmentPayloadId(wifi_credentials.id());
        if (!payload_id) {
            //NL_VLOG(1) << __func__
            //    << ": Skipping unknown WiFi credentials attachment";
            continue;
        }
        auto* wifi_credentials_metadata =
            introduction->add_wifi_credentials_metadata();
        wifi_credentials_metadata->set_id(wifi_credentials.id());
        wifi_credentials_metadata->set_ssid(std::string(wifi_credentials.ssid()));
        wifi_credentials_metadata->set_security_type(
            wifi_credentials.security_type());
        wifi_credentials_metadata->set_payload_id(*payload_id);
    }

    if (introduction->file_metadata_size() == 0 &&
        introduction->text_metadata_size() == 0 &&
        introduction->wifi_credentials_metadata_size() == 0) {
        NL_LOG(WARNING) << __func__
            << ": No payloads tied to transfer, disconnecting.";
        AbortAndCloseConnectionIfNecessary(
            TransferMetadata::Status::kMissingPayloads, share_target);
        return;
    }

    // Write the introduction to the remote device.
    nearby::sharing::service::proto::Frame frame;
    frame.set_version(nearby::sharing::service::proto::Frame::V1);
    nearby::sharing::service::proto::V1Frame* v1_frame = frame.mutable_v1();
    v1_frame->set_type(nearby::sharing::service::proto::V1Frame::INTRODUCTION);
    v1_frame->set_allocated_introduction(introduction.release());

    std::vector<uint8_t> data(frame.ByteSizeLong());
    frame.SerializeToArray(data.data(), frame.ByteSizeLong());
    connection->Write(std::move(data));

    // We've successfully written the introduction, so we now have to wait for the
    // remote side to accept.
    //NL_VLOG(1) << __func__ << ": Successfully wrote the introduction frame";

   /* mutual_acceptance_timeout_alarm_->Stop();
    mutual_acceptance_timeout_alarm_->Start(
        absl::ToInt64Milliseconds(kReadResponseFrameTimeout), 0,
        [&, share_target]() { OnOutgoingMutualAcceptanceTimeout(share_target); });*/

    info->transfer_update_callback()->OnTransferUpdate(
        share_target,
        TransferMetadataBuilder()
        .set_status(TransferMetadata::Status::kAwaitingLocalConfirmation)
        .set_token(four_digit_token)
        .build());
}

void NearbySharingServiceImpl::StartScanning(DeviceAddedCallback callback)
{
    deviceAddedCallback_ = callback;

    StartScanning();
}

void NearbySharingServiceImpl::StartScanning()
{
    is_scanning_ = true;

    scanning_session_id_ = GenerateNextId();

    nearby_connections_manager_->StartDiscovery(
        /*listener=*/this, hardcoded::GetDataUsage(), [&](Status status) {
            // Log analytics event of starting discovery.
            //analytics::AnalyticsInformation analytics_information;
            //analytics_information.send_surface_state =
            //    foreground_send_discovery_callbacks_.empty()
            //    ? analytics::SendSurfaceState::kBackground
            //    : analytics::SendSurfaceState::kForeground;
            //analytics_recorder_->NewScanForShareTargetsStart(
            //    scanning_session_id_,
            //    status == Status::kSuccess ? SessionStatus::SUCCEEDED_SESSION_STATUS
            //    : SessionStatus::FAILED_SESSION_STATUS,
            //    analytics_information,
            //    /*flow_id=*/1, /*referrer_package=*/std::nullopt);

            OnStartDiscoveryResult(status);
        });
}

void NearbySharingServiceImpl::OnStartDiscoveryResult(Status status) {
   
}

// NearbyConnectionsManager::DiscoveryListener:
void NearbySharingServiceImpl::OnEndpointDiscovered(
    absl::string_view endpoint_id, absl::Span<const uint8_t> endpoint_info) {
    // The calling thread may already be completed when calling lambda. We
    // make a local copy of calling parameter to avoid possible memory
    // issue.
    std::vector<uint8_t> endpoint_info_copy{ endpoint_info.begin(),
                                            endpoint_info.end() };
    RunOnNearbySharingServiceThread(
        "on_endpoint_discovered",
        [&, endpoint_id = std::string(endpoint_id),
        endpoint_info_copy = std::move(endpoint_info_copy)]() {
            AddEndpointDiscoveryEvent([&, endpoint_id, endpoint_info_copy]() {
                HandleEndpointDiscovered(endpoint_id, endpoint_info_copy);
                });
        });
}

void NearbySharingServiceImpl::OnEndpointLost(absl::string_view endpoint_id) {
    RunOnNearbySharingServiceThread(
        "on_endpoint_lost", [&, endpoint_id = std::string(endpoint_id)]() {
            AddEndpointDiscoveryEvent(
                [&, endpoint_id]() { HandleEndpointLost(endpoint_id); });
        });
}

// Processes endpoint discovered/lost events. We queue up the events to ensure
// each discovered or lost event is fully handled before the next is run. For
// example, we don't want to start processing an endpoint-lost event before
// the corresponding endpoint-discovered event is finished. This is especially
// important because of the asynchronous steps required to process an
// endpoint-discovered event.
void NearbySharingServiceImpl::AddEndpointDiscoveryEvent(
    std::function<void()> event) {
    endpoint_discovery_events_.push(std::move(event));
    if (endpoint_discovery_events_.size() == 1u) {
        auto discovery_event = std::move(endpoint_discovery_events_.front());
        discovery_event();
    }
}

void NearbySharingServiceImpl::HandleEndpointDiscovered(
    absl::string_view endpoint_id, absl::Span<const uint8_t> endpoint_info) {
    //NL_VLOG(1) << __func__ << ": endpoint_id=" << endpoint_id
    //    << ", endpoint_info=" << nearby::utils::HexEncode(endpoint_info);
    if (!is_scanning_) {
        //NL_VLOG(1)
        //    << __func__
        //    << ": Ignoring discovered endpoint because we're no longer scanning";
        FinishEndpointDiscoveryEvent();
        return;
    }

    std::unique_ptr<Advertisement> advertisement =
        decoder_->DecodeAdvertisement(endpoint_info);

    if (deviceAddedCallback_)
    {
        deviceAddedCallback_(advertisement->device_name().value(), std::string(endpoint_id));
    }
    
    OnOutgoingAdvertisementDecoded(endpoint_id, endpoint_info,
        std::move(advertisement));
}

void NearbySharingServiceImpl::HandleEndpointLost(
    absl::string_view endpoint_id) {
    //NL_VLOG(1) << __func__ << ": endpoint_id=" << endpoint_id;

    if (!is_scanning_) {
        //NL_VLOG(1) << __func__
        //    << ": Ignoring lost endpoint because we're no longer scanning";
        FinishEndpointDiscoveryEvent();
        return;
    }

    /*discovered_advertisements_to_retry_map_.erase(endpoint_id);
    discovered_advertisements_retried_set_.erase(endpoint_id);
    RemoveOutgoingShareTargetWithEndpointId(endpoint_id);*/
    FinishEndpointDiscoveryEvent();
}

void NearbySharingServiceImpl::FinishEndpointDiscoveryEvent() {
    NL_DCHECK(!endpoint_discovery_events_.empty());
    NL_DCHECK(endpoint_discovery_events_.front() == nullptr);
    endpoint_discovery_events_.pop();

    // Handle the next queued up endpoint discovered/lost event.
    if (!endpoint_discovery_events_.empty()) {
        NL_DCHECK(endpoint_discovery_events_.front() != nullptr);
        auto discovery_event = std::move(endpoint_discovery_events_.front());
        discovery_event();
    }
}

void GetDecryptedPublicCertificate(
    std::function<void(void)> callback)
{
    callback();
}

void NearbySharingServiceImpl::OnOutgoingAdvertisementDecoded(
    absl::string_view endpoint_id, absl::Span<const uint8_t> endpoint_info,
    std::unique_ptr<Advertisement> advertisement) {
    if (!advertisement) {
        NL_LOG(WARNING) << __func__
            << ": Failed to parse discovered advertisement.";
        FinishEndpointDiscoveryEvent();
        return;
    }

    // Now we will report endpoints met before in NearbyConnectionsManager.
    // Check outgoingShareTargetInfoMap first and pass the same shareTarget if we
    // found one.

    // Looking for the ShareTarget based on endpoint id.
    /*if (outgoing_share_target_map_.find(endpoint_id) !=
        outgoing_share_target_map_.end()) {
        FinishEndpointDiscoveryEvent();
        return;
    }*/

    // Once we get the advertisement, the first thing to do is decrypt the
    // certificate.
    /*NearbyShareEncryptedMetadataKey encrypted_metadata_key(
        advertisement->salt(), advertisement->encrypted_metadata_key());*/

    std::string endpoint_id_copy = std::string(endpoint_id);
    std::vector<uint8_t> endpoint_info_copy{ endpoint_info.begin(),
                                            endpoint_info.end() };
    /*GetCertificateManager()->GetDecryptedPublicCertificate(
        std::move(encrypted_metadata_key),
        [this, endpoint_id_copy, endpoint_info_copy,
        advertisement_copy =
        *advertisement](std::optional<NearbyShareDecryptedPublicCertificate>
            decrypted_public_certificate) {
                std::unique_ptr<Advertisement> advertisement =
                    Advertisement::NewInstance(
                        advertisement_copy.salt(),
                        advertisement_copy.encrypted_metadata_key(),
                        advertisement_copy.device_type(),
                        advertisement_copy.device_name());
                OnOutgoingDecryptedCertificate(endpoint_id_copy, endpoint_info_copy,
                    std::move(advertisement),
                    decrypted_public_certificate);
        });*/

    GetDecryptedPublicCertificate(
        [this, endpoint_id_copy, endpoint_info_copy,
        advertisement_copy =
        *advertisement]() {
                std::unique_ptr<Advertisement> advertisement =
                    Advertisement::NewInstance(
                        advertisement_copy.salt(),
                        advertisement_copy.encrypted_metadata_key(),
                        advertisement_copy.device_type(),
                        advertisement_copy.device_name());
                OnOutgoingDecryptedCertificate(endpoint_id_copy, endpoint_info_copy,
                    std::move(advertisement));
        });
}

ShareTargetInfo& NearbySharingServiceImpl::GetOrCreateShareTargetInfo(
    const ShareTarget& share_target, absl::string_view endpoint_id) {
    if (share_target.is_incoming) {
        auto& info = incoming_share_target_info_map_[share_target.id];
        info.set_endpoint_id(std::string(endpoint_id));
        return info;
    }
    else {
        // We need to explicitly remove any previous share target for
        // |endpoint_id| if one exists, notifying observers that a share target is
        // lost.
        const auto it = outgoing_share_target_map_.find(endpoint_id);
        if (it != outgoing_share_target_map_.end() &&
            it->second.id != share_target.id) {
            RemoveOutgoingShareTargetWithEndpointId(endpoint_id);
        }

        //NL_VLOG(1) << __func__ << ": Adding (endpoint_id=" << endpoint_id
        //    << ", share_target_id=" << share_target.id
        //    << ") to outgoing share target map";
        outgoing_share_target_map_.insert_or_assign(endpoint_id, share_target);
        auto& info = outgoing_share_target_info_map_[share_target.id];
        info.set_endpoint_id(std::string(endpoint_id));
        info.set_connection_layer_status(Status::kUnknown);
        return info;
    }
}

void NearbySharingServiceImpl::RemoveOutgoingShareTargetWithEndpointId(
    absl::string_view endpoint_id) {
    auto it = outgoing_share_target_map_.find(endpoint_id);
    if (it == outgoing_share_target_map_.end()) {
        return;
    }

    //NL_VLOG(1) << __func__ << ": Removing (endpoint_id=" << it->first
    //    << ", share_target.id=" << it->second.id
    //    << ") from outgoing share target map";
    ShareTarget share_target = std::move(it->second);
    outgoing_share_target_map_.erase(it);

    auto info_it = outgoing_share_target_info_map_.find(share_target.id);
    if (info_it != outgoing_share_target_info_map_.end()) {
        outgoing_share_target_info_map_.erase(info_it);
    }
    else {
        NL_LOG(WARNING) << __func__ << ": share_target.id=" << it->second.id
            << " not found in outgoing share target info map.";
        return;
    }

    //for (ShareTargetDiscoveredCallback* discovery_callback :
    //    foreground_send_discovery_callbacks_.GetObservers()) {
    //    if (discovery_callback != nullptr) {
    //        discovery_callback->OnShareTargetLost(share_target);
    //    }
    //    else {
    //        NL_LOG(WARNING) << __func__
    //            << "Foreground Discovery Callback is not exist";
    //    }
    //}
    //for (ShareTargetDiscoveredCallback* discovery_callback :
    //    background_send_discovery_callbacks_.GetObservers()) {
    //    if (discovery_callback != nullptr) {
    //        discovery_callback->OnShareTargetLost(share_target);
    //    }
    //    else {
    //        NL_LOG(WARNING) << __func__
    //            << "Background Discovery Callback is not exist";
    //    }
    //}

    //NL_VLOG(1) << __func__ << ": Reported OnShareTargetLost";
}

void NearbySharingServiceImpl::OnOutgoingDecryptedCertificate(
    absl::string_view endpoint_id, absl::Span<const uint8_t> endpoint_info,
    std::unique_ptr<Advertisement> advertisement) {
    // Check again for this endpoint id, to avoid race conditions.
    if (outgoing_share_target_map_.find(endpoint_id) !=
        outgoing_share_target_map_.end()) {
        FinishEndpointDiscoveryEvent();
        return;
    }

    // The certificate provides the device name, in order to create a ShareTarget
    // to represent this remote device.
    std::optional<ShareTarget> share_target = CreateShareTarget(
        endpoint_id, std::move(advertisement),
        /*is_incoming=*/false);
    if (!share_target.has_value()) {
        //if (discovered_advertisements_retried_set_.contains(endpoint_id)) {
        //    NL_LOG(INFO)
        //        << __func__
        //        << ": Don't try to download public certificates again for endpoint="
        //        << endpoint_id;
        //    FinishEndpointDiscoveryEvent();
        //    return;
        //}

        //NL_LOG(INFO)
        //    << __func__ << ": Failed to convert discovered advertisement to share "
        //    << "target. Ignoring endpoint until next certificate download.";
        std::vector<uint8_t> endpoint_info_data(endpoint_info.begin(),
            endpoint_info.end());

        //discovered_advertisements_to_retry_map_[endpoint_id] = endpoint_info_data;
        FinishEndpointDiscoveryEvent();
        return;
    }

    // Update the endpoint id for the share target.
    NL_LOG(INFO) << __func__
        << ": An endpoint has been discovered, with an advertisement "
        "containing a valid share target.";

    // Log analytics event of discovering share target.
    //analytics_recorder_->NewDiscoverShareTarget(
    //    *share_target, scanning_session_id_,
    //    absl::ToInt64Milliseconds(context_->GetClock()->Now() -
    //        scanning_start_timestamp_),
    //    /*flow_id=*/1, /*referrer_package=*/std::nullopt,
    //    share_foreground_send_surface_start_timestamp_ == absl::InfinitePast()
    //    ? -1
    //    : absl::ToInt64Milliseconds(
    //        context_->GetClock()->Now() -
    //        share_foreground_send_surface_start_timestamp_));

    // Notifies the user that we discovered a device.
    //NL_VLOG(1) << __func__ << ": There are "
    //    << (foreground_send_discovery_callbacks_.size() +
    //        background_send_discovery_callbacks_.size())
    //    << " discovery callbacks be called.";

    /*for (ShareTargetDiscoveredCallback* discovery_callback :
        foreground_send_discovery_callbacks_.GetObservers()) {
        discovery_callback->OnShareTargetDiscovered(*share_target);
    }
    for (ShareTargetDiscoveredCallback* discovery_callback :
        background_send_discovery_callbacks_.GetObservers()) {
        discovery_callback->OnShareTargetDiscovered(*share_target);
    }*/

    //NL_VLOG(1) << __func__ << ": Reported OnShareTargetDiscovered "
    //    << (context_->GetClock()->Now() - scanning_start_timestamp_);

    FinishEndpointDiscoveryEvent();

    //SendAttachments(endpoint_id);
}

std::optional<ShareTarget> NearbySharingServiceImpl::CreateShareTarget(
    absl::string_view endpoint_id, std::unique_ptr<Advertisement> advertisement,
    bool is_incoming) {
    NL_DCHECK(advertisement);

    //if (!advertisement->device_name() && !certificate.has_value()) {
    //    NL_VLOG(1) << __func__
    //        << ": Failed to retrieve public certificate for contact "
    //        "only advertisement.";
    //    return std::nullopt;
    //}

    std::optional<std::string> device_name =
        hardcoded::GetDeviceName(advertisement.get());
    if (!device_name.has_value()) {
        //NL_VLOG(1) << __func__
        //    << ": Failed to retrieve device name for advertisement.";
        return std::nullopt;
    }

    ShareTarget target;
    target.type = advertisement->device_type();
    target.device_name = std::move(*device_name);
    target.is_incoming = is_incoming;
    target.device_id = hardcoded::GetDeviceId(endpoint_id);
   
    /*if (NearbyFlags::GetInstance().GetBoolFlag(
        config_package_nearby::nearby_sharing_feature::kEnableSelfShare)) {
        target.for_self_share = certificate && certificate->for_self_share();
    }*/

    ShareTargetInfo& info = GetOrCreateShareTargetInfo(target, endpoint_id);

    //if (certificate.has_value()) {
    //    if (certificate->unencrypted_metadata().has_full_name())
    //        target.full_name = certificate->unencrypted_metadata().full_name();

    //    if (certificate->unencrypted_metadata().has_icon_url()) {
    //        absl::StatusOr<::nearby::network::Url> url =
    //            ::nearby::network::Url::Create(
    //                certificate->unencrypted_metadata().icon_url());
    //        if (url.ok()) {
    //            target.image_url = url.value();
    //        }
    //        else {
    //            target.image_url = std::nullopt;
    //        }
    //    }

    //    target.is_known = true;
    //    //info.set_certificate(std::move(*certificate));
    //}

    return target;
}

void NearbySharingServiceImpl::SendAttachments(std::string endpoint, std::string filePathIn,
    ProgressUpdateCallback progressCallback, AuthTokenCallback authCallback)
{
    absl::string_view endpoint_id(endpoint);
    auto it = outgoing_share_target_map_.find(endpoint_id);
    if (it == outgoing_share_target_map_.end()) {
        return;
    }

    progressUpdatecallback_ = progressCallback;
    authCallback_ = authCallback;

    const ShareTarget& target = it->second;

    SendAttachments(target, hardcoded::CreateFileAttachments(filePathIn),
        [&](StatusCodes status) {
            OnSendAttachments(status);
        });
}

void NearbySharingServiceImpl::OnSendAttachments(StatusCodes status)
{
    std::cout << "attachments are away..." << std::endl;
}

}  // namespace sharing
}  // namespace nearby
