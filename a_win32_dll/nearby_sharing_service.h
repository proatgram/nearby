// Copyright 2022 Google LLC
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

#ifndef THIRD_PARTY_NEARBY_SHARING_NEARBY_SHARING_SERVICE_H_
#define THIRD_PARTY_NEARBY_SHARING_NEARBY_SHARING_SERVICE_H_

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "absl/strings/string_view.h"
#include "internal/network/url.h"

#include "share_target.h"
#include "attachment.h"

using DeviceAddedCallback = std::function<void(std::string device_name, std::string endpoint_id)>;
using ProgressUpdateCallback = std::function<void(float progress, bool isComplete)>;
using AuthTokenCallback = std::function<void(std::string token)>;

namespace nearby {

class AccountManager;

namespace sharing {

class NearbyNotificationDelegate;
class NearbyShareCertificateManager;
class NearbyShareContactManager;
class NearbyShareHttpNotifier;

// This service implements Nearby Sharing on top of the Nearby Connections mojo.
// Currently, only single profile will be allowed to be bound at a time and only
// after the user has enabled Nearby Sharing in prefs.
class NearbySharingService {
 public:
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused. If entries are added, kMaxValue
  // should be updated.
  enum class StatusCodes {
    // The operation was successful.
    kOk = 0,
    // The operation failed, without any more information.
    kError = 1,
    // The operation failed since it was called in an invalid order.
    kOutOfOrderApiCall = 2,
    // Tried to stop something that was already stopped.
    kStatusAlreadyStopped = 3,
    // Tried to register an opposite foreground surface in the midst of a
    // transfer or connection.
    // (Tried to register Send Surface when receiving a file or tried to
    // register Receive Surface when
    // sending a file.)
    kTransferAlreadyInProgress = 4,
    // There is no available connection medium to use.
    kNoAvailableConnectionMedium = 5,
    // Bluetooth or WiFi hardware ran into an irrecoverable state. User PC needs
    // to be restarted.
    kIrrecoverableHardwareError = 6,
    kMaxValue = kIrrecoverableHardwareError
  };

  enum class ReceiveSurfaceState {
    // Default, invalid state.
    kUnknown,
    // Background receive surface advertises only to contacts.
    kBackground,
    // Foreground receive surface advertises to everyone.
    kForeground,
  };

  enum class SendSurfaceState {
    // Default, invalid state.
    kUnknown,
    // Background send surface only listens to transfer update.
    kBackground,
    // Foreground send surface both scans and listens to transfer update.
    kForeground,
  };

  class Observer {
   public:
    virtual ~Observer() = default;
    virtual void OnHighVisibilityChangeRequested() {}
    virtual void OnHighVisibilityChanged(bool in_high_visibility) = 0;

    virtual void OnStartAdvertisingFailure() {}
    virtual void OnStartDiscoveryResult(bool success) {}

    virtual void OnFastInitiationDevicesDetected() {}
    virtual void OnFastInitiationDevicesNotDetected() {}
    virtual void OnFastInitiationScanningStopped() {}

    virtual void OnBluetoothStatusChanged() {}
    virtual void OnWifiStatusChanged() {}
    virtual void OnLanStatusChanged() {}
    virtual void OnIrrecoverableHardwareErrorReported() {}

    // Called during the |KeyedService| shutdown, but before everything has been
    // cleaned up. It is safe to remove any observers on this event.
    virtual void OnShutdown() = 0;
  };

  static std::string StatusCodeToString(StatusCodes status_code);

  virtual ~NearbySharingService() = default;

  virtual void StartScanning() = 0;
  virtual void StartScanning(DeviceAddedCallback callback) = 0;
  virtual void SendAttachments(std::string endpoint_id, std::string filePathIn,
      ProgressUpdateCallback progressCallback, AuthTokenCallback authCallback) = 0;

  // Sends |attachments| to the remote |share_target|.
  virtual void SendAttachments(
      const ShareTarget& share_target,
      std::vector<std::unique_ptr<Attachment>> attachments,
      std::function<void(StatusCodes)> status_codes_callback) = 0;

  // Accepts incoming share from the remote |share_target|.
  virtual void Accept(
      const ShareTarget& share_target,
      std::function<void(StatusCodes status_codes)> status_codes_callback) = 0;
};

}  // namespace sharing
}  // namespace nearby

#endif  // THIRD_PARTY_NEARBY_SHARING_NEARBY_SHARING_SERVICE_H_
