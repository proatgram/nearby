#include "internal/platform/implementation/linux/bluez_advertisement_monitor.h"
#include "internal/platform/byte_array.h"
#include "internal/platform/implementation/ble_v2.h"
#include "internal/platform/implementation/linux/dbus.h"
#include "internal/platform/implementation/linux/utils.h"
#include "internal/platform/uuid.h"

namespace nearby {
#pragma push_macro("linux")
#undef linux
namespace linux {
namespace bluez {
AdvertisementMonitor::AdvertisementMonitor(
    sdbus::IConnection &system_bus, Uuid service_uuid,
    api::ble_v2::TxPowerLevel tx_power_level, absl::string_view type,
    std::shared_ptr<BluetoothDevices> devices,
    api::ble_v2::BleMedium::ScanCallback scan_callback)
    : AdvertisementMonitor(
          system_bus, service_uuid, tx_power_level, type, std::move(devices),
          api::ble_v2::BleMedium::ScanningCallback{
              .start_scanning_result = nullptr,
              .advertisement_found_cb =
                  std::move(scan_callback.advertisement_found_cb)}) {}

AdvertisementMonitor::AdvertisementMonitor(
    sdbus::IConnection &system_bus, Uuid service_uuid,
    api::ble_v2::TxPowerLevel tx_power_level, absl::string_view type,
    std::shared_ptr<BluetoothDevices> devices,
    api::ble_v2::BleMedium::ScanningCallback scan_callback)
    : AdaptorInterfaces(system_bus, bluez::advertisement_monitor_path(
                                        std::string{service_uuid})),
      devices_(std::move(devices)),
      scan_callback_{std::move(scan_callback.advertisement_found_cb)},
      start_scanning_result_callback_(
          std::move(scan_callback.start_scanning_result)),
      type_(type),
      service_uuid_(service_uuid),
      tx_power_level_(tx_power_level) {
  registerAdaptor();
}

void AdvertisementMonitor::DeviceFound(const sdbus::ObjectPath &device) {
  devices_->cleanup_lost_peripherals();
  auto peripheral = devices_->add_new_device(device);
  std::map<std::string, sdbus::Variant> service_data;
  try {
    service_data = peripheral->ServiceData();
  } catch (const sdbus::Error &e) {
    DBUS_LOG_PROPERTY_GET_ERROR(peripheral, "ServiceData", e);
    return;
  }

  struct api::ble_v2::BleAdvertisementData adv_data;
  for (const auto &[uuid_str, data] : service_data) {
    auto uuid = UuidFromString(uuid_str);
    if (!uuid.has_value()) {
      NEARBY_LOGS(ERROR)
          << __func__
          << ": Could not parse UUID string in ServiceData for peripheral "
          << peripheral->getObjectPath();
      continue;
    }

    std::vector<uint8_t> bytes = data;
    adv_data.service_data.emplace(*uuid,
                                  std::string(bytes.begin(), bytes.end()));
  }
  scan_callback_.advertisement_found_cb(*peripheral, adv_data);
}

void AdvertisementMonitor::DeviceLost(const sdbus::ObjectPath &device) {
  devices_->mark_peripheral_lost(device);
}
}  // namespace bluez
}  // namespace linux
#pragma pop_macro("linux")
}  // namespace nearby
