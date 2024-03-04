
#include "hardcoded.h"

#include "absl/random/random.h"
#include "absl/strings/ascii.h"
#include "advertisement.h"
#include "winrt/Windows.Devices.Bluetooth.h"

using namespace nearby::sharing;
using ::nearby::sharing::proto::DataUsage;
using ::location::nearby::proto::sharing::OSType;
using WindowsBluetoothAdapter = winrt::Windows::Devices::Bluetooth::BluetoothAdapter;

namespace hardcoded
{
    std::optional<std::vector<uint8_t>> GetBluetoothMacAddressForShareTarget(ShareTarget share_target)
    {
        return std::nullopt;
        //std::string mac_address =
        //    //certificate.unencrypted_metadata().bluetooth_mac_address();
        //    "sixlen";
        //if (mac_address.size() != 6) {
        //    return std::nullopt;
        //}

        //return std::vector<uint8_t>(mac_address.begin(), mac_address.end());
    }

    std::string GetDeviceName()
    {
        return "RajWindows";
    }

    std::optional<std::string> GetDeviceName(
        const Advertisement* advertisement)
    {
        // Device name is always included when visible to everyone.
        if (advertisement->device_name().has_value()) {
            return advertisement->device_name();
        }

        return std::nullopt;
    }

    nearby::sharing::ShareTargetType GetDeviceType()
    {
        return nearby::sharing::ShareTargetType::kLaptop;
    }

    std::optional<std::filesystem::path> GetDownloadPath() {
        PWSTR path;
        HRESULT result =
            SHGetKnownFolderPath(FOLDERID_Downloads, KF_FLAG_DEFAULT, nullptr, &path);
        if (result == S_OK) {
            std::wstring download_path{ path };
            CoTaskMemFree(path);
            return std::filesystem::path(download_path);
        }

        CoTaskMemFree(path);
        return std::nullopt;
    }

    std::optional<std::filesystem::path> GetPicturesPath() {
        PWSTR path;
        HRESULT result =
            SHGetKnownFolderPath(FOLDERID_Pictures, KF_FLAG_DEFAULT, nullptr, &path);
        if (result == S_OK) {
            std::wstring download_path{ path };
            CoTaskMemFree(path);
            return std::filesystem::path(download_path);
        }

        CoTaskMemFree(path);
        return std::nullopt;
    }
    
    DataUsage GetDataUsage()
    {
        return DataUsage::WIFI_ONLY_DATA_USAGE;
    }


    void RunVerification(std::function<void(OSType)> callback)
    {
        callback(OSType::WINDOWS);
    }

    bool IsExtendedAdvertisingSupported()
    {
        return true;
    }

    // Return the most stable device identifier with the following priority:
//   1. Hash of Bluetooth MAC address.
//   2. Certificate ID.
//   3. Endpoint ID.
    std::string GetDeviceId(absl::string_view endpoint_id)
    {
        return std::string(endpoint_id);
    }

    std::vector<std::unique_ptr<Attachment>> CreateFileAttachments(
        std::vector<std::filesystem::path> file_paths) {
        std::vector<std::unique_ptr<Attachment>> attachments;
        for (auto& file_path : file_paths) {
            attachments.push_back(
                std::make_unique<FileAttachment>(std::move(file_path)));
        }
        return attachments;
    }

    std::vector<std::unique_ptr<Attachment>> CreateFileAttachments(std::string filePathIn)
    {
        std::vector<std::filesystem::path> file_paths;
        std::vector<std::unique_ptr<Attachment>> attachments;
        std::filesystem::path filePath(filePathIn);
        file_paths.push_back(filePath);
        return CreateFileAttachments(file_paths);
    }

    std::string uint64_to_mac_address_string(uint64_t bluetoothAddress) {
        std::string buffer = absl::StrFormat(
            "%02llx:%02llx:%02llx:%02llx:%02llx:%02llx", bluetoothAddress >> 40,
            (bluetoothAddress >> 32) & 0xff, (bluetoothAddress >> 24) & 0xff,
            (bluetoothAddress >> 16) & 0xff, (bluetoothAddress >> 8) & 0xff,
            bluetoothAddress & 0xff);

        return absl::AsciiStrToUpper(buffer);
    }

    std::string GetMyBluetoothMacAddress() {
        WindowsBluetoothAdapter windows_bluetooth_adapter_(nullptr);
        try {
            /*windows_bluetooth_adapter_ =
                winrt::Windows::Devices::Bluetooth::BluetoothAdapter::GetDefaultAsync()
                .get();*/
        }
        catch (const winrt::hresult_error& error) {
            //NEARBY_LOGS(ERROR) << __func__ << ": WinRT exception: " << error.code()
            //    << ": " << winrt::to_string(error.message());
        }
        catch (...) {
            //NEARBY_LOGS(ERROR) << __func__ << ": unknown error.";
        }
        if (windows_bluetooth_adapter_ == nullptr) {
            //NEARBY_LOGS(ERROR) << __func__ << ": No Bluetooth adapter on this device.";
            return "";
        }
        try {
            return uint64_to_mac_address_string(
                windows_bluetooth_adapter_.BluetoothAddress());
        }
        catch (std::exception exception) {
            //NEARBY_LOGS(ERROR) << __func__ << ": exception:" << exception.what();
            return "";
        }
        catch (const winrt::hresult_error& ex) {
            //NEARBY_LOGS(ERROR) << __func__ << ": exception:" << ex.code() << ": "
            //    << winrt::to_string(ex.message());
            return "";
        }
        catch (...) {
            //NEARBY_LOGS(ERROR) << __func__ << ": unknown error.";
            return "";
        }
    }

    std::optional<std::array<uint8_t, 6>> GetMyBluetoothMacAddressInByteArray()
    {
        std::string address_str = GetMyBluetoothMacAddress();
        if (address_str.empty()) return std::nullopt;
        if (address_str.length() != 6) return std::nullopt;

        std::optional<std::array<uint8_t, 6>> bluetooth_mac_address;

        for (size_t i = 0; i < address_str.size(); i++)
        {
            (*bluetooth_mac_address)[i] = address_str[i];
        }

        return bluetooth_mac_address;
    }

    ::nearby::api::DeviceInfo::OsType GetDeviceInfoOsType()
    {
        return ::nearby::api::DeviceInfo::OsType::kWindows;
    }

    std::vector<uint8_t> GenerateRandomBytes(size_t num_bytes)
    {
        std::vector<uint8_t> bytes(num_bytes);
        crypto::RandBytes(absl::Span<uint8_t>(bytes));
        return bytes;
    }
}
