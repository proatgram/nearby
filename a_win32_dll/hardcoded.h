#pragma once

#include <memory>
#include <optional>
#include <ostream>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include <Shlobj.h>

#include "nearby_share_enums.h"
#include "share_target.h"
#include "sharing/proto/enums.pb.h"
#include "advertisement.h"
#include "internal/platform/device_info.h"


using namespace nearby::sharing;
using ::nearby::sharing::proto::DataUsage;
using ::location::nearby::proto::sharing::OSType;

namespace hardcoded
{
	std::optional<std::vector<uint8_t>> GetBluetoothMacAddressForShareTarget(ShareTarget share_target);
	std::string GetDeviceName();
	std::optional<std::string> GetDeviceName(const Advertisement* advertisement);
	nearby::sharing::ShareTargetType GetDeviceType();
	std::optional<std::filesystem::path> GetDownloadPath();
	std::optional<std::filesystem::path> GetPicturesPath();
	DataUsage GetDataUsage();
	void RunVerification(std::function<void(OSType)> callback);
	bool IsExtendedAdvertisingSupported();

	// Return the most stable device identifier with the following priority:
	//   1. Hash of Bluetooth MAC address.
	//   2. Certificate ID.
	//   3. Endpoint ID.
	//	currently, it is kept simple
	std::string GetDeviceId(absl::string_view endpoint_id);

	std::vector<std::unique_ptr<Attachment>> CreateFileAttachments(std::string filePathIn);
	std::vector<std::unique_ptr<Attachment>> CreateFileAttachments(
		std::vector<std::filesystem::path> file_paths);

	std::string GetMyBluetoothMacAddress();
	std::optional<std::array<uint8_t, 6>> GetMyBluetoothMacAddressInByteArray();

	::nearby::api::DeviceInfo::OsType GetDeviceInfoOsType();

	std::vector<uint8_t> GenerateRandomBytes(size_t num_bytes);
}
