#ifndef CORE_V2_INTERNAL_OFFLINE_FRAMES_H_
#define CORE_V2_INTERNAL_OFFLINE_FRAMES_H_

#include <cstdint>
#include <vector>

#include "core_v2/options.h"
#include "proto/connections/offline_wire_formats.pb.h"
#include "platform_v2/base/byte_array.h"
#include "platform_v2/base/exception.h"
#include "proto/connections_enums.pb.h"

namespace location {
namespace nearby {
namespace connections {
namespace parser {

using UpgradePathInfo = BandwidthUpgradeNegotiationFrame::UpgradePathInfo;

// Serialize/Deserialize Nearby Connections Protocol messages.

// Parses incoming message.
// Returns OfflineFrame if parser was able to understand it, or
// Exception::kInvalidProtocolBuffer, if parser failed.
ExceptionOr<OfflineFrame> FromBytes(const ByteArray& offline_frame_bytes);

// Returns FrameType of a parsed message, or
// V1Frame::UNKNOWN_FRAME_TYPE, if frame contents is not recognized.
V1Frame::FrameType GetFrameType(const OfflineFrame& offline_frame);

// Builds Connection Request / Response messages.
ByteArray ForConnectionRequest(
    const std::string& endpoint_id, const ByteArray& endpoint_info,
    std::int32_t nonce, const std::vector<Medium>& mediums);
ByteArray ForConnectionResponse(std::int32_t status);

// Builds Payload transfer messages.
ByteArray ForDataPayloadTransfer(
    const PayloadTransferFrame::PayloadHeader& header,
    const PayloadTransferFrame::PayloadChunk& chunk);
ByteArray ForControlPayloadTransfer(
    const PayloadTransferFrame::PayloadHeader& header,
    const PayloadTransferFrame::ControlMessage& control);

// Builds Bandwidth Upgrade [BWU] messages.
ByteArray ForBwuIntroduction(const std::string& endpoint_id);
ByteArray ForBwuWifiHotspotPathAvailable(const std::string& ssid,
                                         const std::string& password,
                                         std::int32_t port);
ByteArray ForBwuWifiLanPathAvailable(const std::string& ip_address,
                                     std::int32_t port);
ByteArray ForBwuBluetoothPathAvailable(const std::string& service_id,
                                       const std::string& mac_address);
ByteArray ForBwuFailure(const UpgradePathInfo& info);
ByteArray ForBwuLastWrite();
ByteArray ForBwuSafeToClose();

ByteArray ForKeepAlive();

UpgradePathInfo::Medium MediumToUpgradePathInfoMedium(Medium medium);
Medium UpgradePathInfoMediumToMedium(UpgradePathInfo::Medium medium);

ConnectionRequestFrame::Medium MediumToConnectionRequestMedium(Medium medium);
Medium ConnectionRequestMediumToMedium(ConnectionRequestFrame::Medium medium);
std::vector<Medium> ConnectionRequestMediumsToMediums(
    const ConnectionRequestFrame& connection_request_frame);

}  // namespace parser
}  // namespace connections
}  // namespace nearby
}  // namespace location

#endif  // CORE_V2_INTERNAL_OFFLINE_FRAMES_H_
