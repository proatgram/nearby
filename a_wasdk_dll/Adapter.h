#pragma once

#include "dll_config.h"

namespace nearby::connections {
	class Core;
	class ServiceController;
	class ServiceControllerRouter;
	class OfflineServiceController;
}  // namespace nearby::connections
namespace nearby::windows {

	extern "C" {

		using Core = connections::Core;
		using ServiceControllerRouter = connections::ServiceControllerRouter;

		// Initializes a Core instance, providing the ServiceController factory from
		// app side. If no factory is provided, it will initialize a new
		// factory creating OfflineServiceController.
		// Returns the instance handle to c# client.
		// TODO(jfcarroll): Is this method needed? The trouble is we can't
		// new up a forward declared class (OfflineServiceController). If this
		// is necessary, must find another way to implement it.
		// DLL_API Core *__stdcall InitCoreWithServiceControllerFactory(
		//    std::function<ServiceController *()> factory = []() {
		//      return new OfflineServiceController;
		//    });

		// Initializes a default Core instance.
		// Returns the instance handle to c# client.
		DLL_API Core* __stdcall InitCore(ServiceControllerRouter*);
	}
}