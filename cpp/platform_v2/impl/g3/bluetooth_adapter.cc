#include "platform_v2/impl/g3/bluetooth_adapter.h"

#include <string>

#include "platform_v2/base/medium_environment.h"
#include "platform_v2/base/prng.h"
#include "platform_v2/impl/g3/bluetooth_classic.h"

namespace location {
namespace nearby {
namespace g3 {

BlePeripheral::BlePeripheral(BluetoothAdapter* adapter) : adapter_(*adapter) {}

std::string BlePeripheral::GetName() const { return adapter_.GetName(); }

ByteArray BlePeripheral::GetAdvertisementBytes(
    const std::string& service_id) const {
  return advertisement_bytes_;
}

void BlePeripheral::SetAdvertisementBytes(
    const std::string& service_id, const ByteArray& advertisement_bytes) {
  advertisement_bytes_ = advertisement_bytes;
}

BluetoothDevice::BluetoothDevice(BluetoothAdapter* adapter)
    : adapter_(*adapter) {}

std::string BluetoothDevice::GetName() const { return adapter_.GetName(); }
std::string BluetoothDevice::GetMacAddress() const {
  return adapter_.GetMacAddress();
}

BluetoothAdapter::BluetoothAdapter() {
  std::string mac_address;
  mac_address.resize(6);
  int64_t raw_mac_addr = Prng().NextInt64();
  mac_address[0] = static_cast<char>(raw_mac_addr >> 40);
  mac_address[1] = static_cast<char>(raw_mac_addr >> 32);
  mac_address[2] = static_cast<char>(raw_mac_addr >> 24);
  mac_address[3] = static_cast<char>(raw_mac_addr >> 16);
  mac_address[4] = static_cast<char>(raw_mac_addr >> 8);
  mac_address[5] = static_cast<char>(raw_mac_addr >> 0);
  SetMacAddress(mac_address);
}

BluetoothAdapter::~BluetoothAdapter() { SetStatus(Status::kDisabled); }

void BluetoothAdapter::SetBluetoothClassicMedium(
    api::BluetoothClassicMedium* medium) {
  bluetooth_classic_medium_ = medium;
}

void BluetoothAdapter::SetBleMedium(api::BleMedium* medium) {
  ble_medium_ = medium;
}

bool BluetoothAdapter::SetStatus(Status status) {
  BluetoothAdapter::ScanMode mode;
  bool enabled = status == Status::kEnabled;
  std::string name;
  {
    absl::MutexLock lock(&mutex_);
    enabled_ = enabled;
    name = name_;
    mode = mode_;
  }
  auto& env = MediumEnvironment::Instance();
  env.OnBluetoothAdapterChangedState(*this, device_, name, enabled, mode);
  return true;
}

bool BluetoothAdapter::IsEnabled() const {
  absl::MutexLock lock(&mutex_);
  return enabled_;
}

BluetoothAdapter::ScanMode BluetoothAdapter::GetScanMode() const {
  absl::MutexLock lock(&mutex_);
  return mode_;
}

bool BluetoothAdapter::SetScanMode(BluetoothAdapter::ScanMode mode) {
  bool enabled;
  std::string name;
  {
    absl::MutexLock lock(&mutex_);
    mode_ = mode;
    name = name_;
    enabled = enabled_;
  }
  auto& env = MediumEnvironment::Instance();
  env.OnBluetoothAdapterChangedState(*this, device_, std::move(name), enabled,
                                     mode);
  return true;
}

std::string BluetoothAdapter::GetName() const {
  absl::MutexLock lock(&mutex_);
  return name_;
}

bool BluetoothAdapter::SetName(absl::string_view name) {
  BluetoothAdapter::ScanMode mode;
  bool enabled;
  {
    absl::MutexLock lock(&mutex_);
    name_ = name;
    enabled = enabled_;
    mode = mode_;
  }
  auto& env = MediumEnvironment::Instance();
  env.OnBluetoothAdapterChangedState(*this, device_, std::string(name), enabled,
                                     mode);
  return true;
}

}  // namespace g3
}  // namespace nearby
}  // namespace location
