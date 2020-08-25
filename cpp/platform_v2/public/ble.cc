#include "platform_v2/public/ble.h"

#include "platform_v2/public/logging.h"
#include "platform_v2/public/mutex_lock.h"

namespace location {
namespace nearby {

bool BleMedium::StartAdvertising(const std::string& service_id,
                                 const ByteArray& advertisement_bytes) {
  return impl_->StartAdvertising(service_id, advertisement_bytes);
}

bool BleMedium::StopAdvertising(const std::string& service_id) {
  return impl_->StopAdvertising(service_id);
}

bool BleMedium::StartScanning(const std::string& service_id,
                              DiscoveredPeripheralCallback callback) {
  {
    MutexLock lock(&mutex_);
    discovered_peripheral_callback_ = std::move(callback);
    peripherals_.clear();
  }
  return impl_->StartScanning(
      service_id,
      {
          .peripheral_discovered_cb =
              [this](api::BlePeripheral& peripheral,
                     const std::string& service_id) {
                MutexLock lock(&mutex_);
                auto pair = peripherals_.emplace(
                    &peripheral, absl::make_unique<ScanningInfo>());
                auto& context = *pair.first->second;
                if (!pair.second) {
                  NEARBY_LOG(INFO,
                             "Discovering (again) peripheral=%p, impl=%p, "
                             "peripheral name=%s",
                             &context.peripheral, &peripheral,
                             peripheral.GetName().c_str());
                } else {
                  context.peripheral = BlePeripheral(&peripheral);
                  NEARBY_LOG(INFO,
                             "Discovering peripheral=%p, impl=%p, "
                             "peripheral name=%s",
                             &context.peripheral, &peripheral,
                             peripheral.GetName().c_str());
                  discovered_peripheral_callback_.peripheral_discovered_cb(
                      context.peripheral, service_id);
                }
              },
          .peripheral_lost_cb =
              [this](api::BlePeripheral& peripheral,
                     const std::string& service_id) {
                MutexLock lock(&mutex_);
                if (peripherals_.empty()) return;
                auto context = peripherals_.find(&peripheral);
                if (context == peripherals_.end()) return;
                NEARBY_LOG(INFO, "Removing peripheral=%p, impl=%p",
                           &(context->second->peripheral), &peripheral);
                discovered_peripheral_callback_.peripheral_lost_cb(
                    context->second->peripheral, service_id);
              },
      });
}

bool BleMedium::StopScanning(const std::string& service_id) {
  {
    MutexLock lock(&mutex_);
    discovered_peripheral_callback_ = {};
    peripherals_.clear();
    NEARBY_LOG(INFO, "Ble Scanning disabled: impl=%p", &GetImpl());
  }
  return impl_->StopScanning(service_id);
}

bool BleMedium::StartAcceptingConnections(const std::string& service_id,
                                          AcceptedConnectionCallback callback) {
  {
    MutexLock lock(&mutex_);
    accepted_connection_callback_ = std::move(callback);
  }
  return impl_->StartAcceptingConnections(
      service_id,
      {
          .accepted_cb =
              [this](api::BleSocket& socket, const std::string& service_id) {
                MutexLock lock(&mutex_);
                auto pair = sockets_.emplace(
                    &socket, absl::make_unique<AcceptedConnectionInfo>());
                auto& context = *pair.first->second;
                if (!pair.second) {
                  NEARBY_LOG(INFO, "Accepting (again) socket=%p, impl=%p",
                             &context.socket, &socket);
                } else {
                  context.socket = BleSocket(&socket);
                  NEARBY_LOG(INFO, "Accepting socket=%p, impl=%p",
                             &context.socket, &socket);
                }
                accepted_connection_callback_.accepted_cb(context.socket,
                                                          service_id);
              },
      });
}

bool BleMedium::StopAcceptingConnections(const std::string& service_id) {
  {
    MutexLock lock(&mutex_);
    accepted_connection_callback_ = {};
    sockets_.clear();
    NEARBY_LOG(INFO, "Ble accepted connection disabled: impl=%p", &GetImpl());
  }
  return impl_->StopAcceptingConnections(service_id);
}

BleSocket BleMedium::Connect(BlePeripheral& peripheral,
                             const std::string& service_id) {
  {
    MutexLock lock(&mutex_);
    NEARBY_LOG(INFO, "BleMedium::Connect: peripheral=%p [impl=%p]", &peripheral,
               &peripheral.GetImpl());
  }
  return BleSocket(impl_->Connect(peripheral.GetImpl(), service_id));
}

}  // namespace nearby
}  // namespace location
