
#include "Adapter.h"
#include "connections/core.h"

namespace nearby::windows {

	Core* InitCore(connections::ServiceControllerRouter* router) {
		return new nearby::connections::Core(router);
	}
}