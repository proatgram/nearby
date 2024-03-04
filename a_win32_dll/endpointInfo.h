#pragma once

#include "dll_config.h"

#include <vector>
#include <string>
#include <optional>

namespace nearby::endpoint
{
    // Describes the type of device for a ShareTarget.
    // The numeric values are used to encode/decode advertisement bytes, and must
    // be kept in sync with Android implementation.
    // These values are persisted to logs. Entries should not be renumbered and
    // numeric values should never be reused.
    enum class ShareTargetType {
        // Unknown device type.
        kUnknown = 0,
        // A phone.
        kPhone = 1,
        // A tablet.
        kTablet = 2,
        // A laptop.
        kLaptop = 3,
    };


    DLL_API std::optional<std::vector<uint8_t>> __stdcall
        CreateEndpointInfo(
            const std::optional<std::string>& device_name,
            ShareTargetType device_type);
}