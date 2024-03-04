#pragma once

#include "dll_config.h"
#include <memory>
#include <string>
#include <functional>

namespace nearby {
	namespace sharing {
		class NearbySharingService;
	}
}

using DeviceAddedCallback = std::function<void(std::string device_name, std::string endpoint_id)>;
using ProgressUpdateCallback = std::function<void(float progress, bool isComplete)>;
using AuthTokenCallback = std::function<void(std::string token)>;

namespace nearby::windows {

	extern "C" {

		class DLL_API NearbyShareAPI {
		public:
			void __stdcall InitializeNearby();

			void StartScanning();
			void StartScanning2(DeviceAddedCallback deviceAddedCallback);
			void StartAdvertising();
			void SendAttachments(std::string endpoint_id, std::string filePath,
				ProgressUpdateCallback progressCallback, AuthTokenCallback authCallback);


			nearby::sharing::NearbySharingService* nearby_sharing_service_;
		};
	}
}
