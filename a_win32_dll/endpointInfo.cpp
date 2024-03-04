#pragma once
#include <vector>
#include <string>
#include <optional>

#include "endpointInfo.h"
#include "internal/crypto_cros/random.h"

namespace nearby::endpoint
{
    static constexpr uint8_t kSaltSize = 2;
    static constexpr uint8_t kMetadataEncryptionKeyHashByteSize = 4;

    // v1 advertisements:
    //   - ParseVersion() --> 0
    //
    // v2 advertisements:
    //   - ParseVersion() --> 1
    //   - Backwards compatible; no changes in advertisement data--aside from the
    //     version number--or parsing logic compared to v1.
    //   - Only used by GmsCore at the moment.
    constexpr int kMaxSupportedAdvertisementParsedVersionNumber = 1;

    // The bit mask for parsing and writing Version.
    constexpr uint8_t kVersionBitmask = 0b111;

    // The bit mask for parsing and writing Visibility.
    constexpr uint8_t kVisibilityBitmask = 0b1;

    // The bit mask for parsing and writing Device Type.
    constexpr uint8_t kDeviceTypeBitmask = 0b111;

    const uint8_t kMinimumSize =
        /* Version(3 bits)|Visibility(1 bit)|Device Type(3 bits)|Reserved(1 bits)=
         */
        1 + kSaltSize +
        kMetadataEncryptionKeyHashByteSize;

    uint8_t ConvertVersion(int version) {
        return static_cast<uint8_t>((version & kVersionBitmask) << 5);
    }

    uint8_t ConvertDeviceType(ShareTargetType type) {
        return static_cast<uint8_t>((static_cast<int32_t>(type) & kDeviceTypeBitmask)
            << 1);
    }

    uint8_t ConvertHasDeviceName(bool hasDeviceName) {
        return static_cast<uint8_t>((hasDeviceName ? 0 : 1) << 4);
    }

    int ParseVersion(uint8_t b) { return (b >> 5) & kVersionBitmask; }

    bool IsKnownDeviceValue(int32_t value) {
        switch (value) {
        case 0:
        case 1:
        case 2:
        case 3:
            return true;
        default:
            return false;
        }
    }

    ShareTargetType ParseDeviceType(uint8_t b) {
        int32_t intermediate = static_cast<int32_t>(b >> 1 & kDeviceTypeBitmask);
        if (IsKnownDeviceValue(intermediate)) {
            return static_cast<ShareTargetType>(intermediate);
        }

        return ShareTargetType::kUnknown;
    }

    bool ParseHasDeviceName(uint8_t b) {
        return ((b >> 4) & kVisibilityBitmask) == 0;
    }

    std::vector<uint8_t> ToEndpointInfo(std::optional<std::string> device_name_, int version_,
        ShareTargetType device_type_, std::vector<uint8_t> salt_, std::vector<uint8_t> encrypted_metadata_key_)
    {
        int size = kMinimumSize + (device_name_.has_value() ? 1 : 0) +
            (device_name_.has_value() ? device_name_->size() : 0);

        std::vector<uint8_t> endpoint_info;
        endpoint_info.reserve(size);
        endpoint_info.push_back(
            static_cast<uint8_t>(ConvertVersion(version_) |
                ConvertHasDeviceName(device_name_.has_value()) |
                ConvertDeviceType(device_type_)));
        endpoint_info.insert(endpoint_info.end(), salt_.begin(), salt_.end());
        endpoint_info.insert(endpoint_info.end(), encrypted_metadata_key_.begin(),
            encrypted_metadata_key_.end());

        if (device_name_.has_value()) {
            endpoint_info.push_back(static_cast<uint8_t>(device_name_->size() & 0xff));
            endpoint_info.insert(endpoint_info.end(), device_name_->begin(),
                device_name_->end());
        }

        return endpoint_info;
    }

    std::vector<uint8_t> GenerateRandomBytes(size_t num_bytes) {
        std::vector<uint8_t> bytes(num_bytes);
        crypto::RandBytes(absl::Span<uint8_t>(bytes));
        return bytes;
    }

    std::optional<std::vector<uint8_t>>
        CreateEndpointInfo(
            const std::optional<std::string>& device_name,
            ShareTargetType device_type) {
        std::vector<uint8_t> salt;
        std::vector<uint8_t> encrypted_key;

        // Generate random metadata key for non-login user or failed to generate
        // metadata keys for login user.

        if (salt.empty() || encrypted_key.empty()) {
            salt = GenerateRandomBytes(kSaltSize);
            encrypted_key = GenerateRandomBytes(
                kMetadataEncryptionKeyHashByteSize);
        }

        return ToEndpointInfo(device_name, 0, device_type, salt, encrypted_key);
    }
}