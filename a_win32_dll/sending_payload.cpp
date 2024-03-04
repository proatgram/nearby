
#include "sending_payload.h"


void NearbySharingServiceImpl::SendAttachments()
{
    
    
    //ShareTargetInfo* info = GetShareTargetInfo(share_target);
    //if (!info || !info->endpoint_id()) {
    //    NL_LOG(WARNING)
    //        << __func__
    //        << ": Failed to send attachments. Unknown ShareTarget.";
    //    std::move(status_codes_callback)(StatusCodes::kError);
    //    return;
    //}

    //// Set session ID.
    //info->set_session_id(analytics_recorder_->GenerateNextId());

    //if (!share_target_copy.has_attachments()) {
    //    NL_LOG(WARNING) << __func__ << ": No attachments to send.";
    //    std::move(status_codes_callback)(StatusCodes::kError);
    //    return;
    //}

    //// For sending advertisement from scanner, the request advertisement
    //// should always be visible to everyone.
    //std::optional<std::vector<uint8_t>> endpoint_info =
    //    CreateEndpointInfo(local_device_data_manager_->GetDeviceName());
    //if (!endpoint_info) {
    //    NL_LOG(WARNING) << __func__
    //        << ": Could not create local endpoint info.";
    //    std::move(status_codes_callback)(StatusCodes::kError);
    //    return;
    //}

    //info->set_transfer_update_callback(
    //    std::make_unique<TransferUpdateDecorator>(
    //        [&](const ShareTarget& share_target,
    //            const TransferMetadata& transfer_metadata) {
    //                OnOutgoingTransferUpdate(share_target, transfer_metadata);
    //        }));

    //// Log analytics event of sending start.
    //analytics_recorder_->NewSendStart(
    //    info->session_id(),
    //    /*transfer_position=*/GetConnectedShareTargetPos(share_target),
    //    /*concurrent_connections=*/GetConnectedShareTargetCount(),
    //    share_target);

    //send_attachments_timestamp_ = context_->GetClock()->Now();
    //OnTransferStarted(/*is_incoming=*/false);
    //is_connecting_ = true;
    //InvalidateSendSurfaceState();

    //// Send process initialized successfully, from now on status updated
    //// will be sent out via OnOutgoingTransferUpdate().
    //info->transfer_update_callback()->OnTransferUpdate(
    //    share_target_copy,
    //    TransferMetadataBuilder()
    //    .set_status(TransferMetadata::Status::kConnecting)
    //    .build());

    //CreatePayloads(std::move(share_target_copy),
    //    [this, endpoint_info = std::move(*endpoint_info)](
    //        ShareTarget share_target, bool success) {
    //            // Log analytics event of describing attachments.
    //            analytics_recorder_->NewDescribeAttachments(
    //                share_target.GetAttachments());

    //            OnCreatePayloads(std::move(endpoint_info),
    //                share_target, success);
    //    });

    //std::move(status_codes_callback)(StatusCodes::kOk);
}