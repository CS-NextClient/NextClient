#include "entry.h"
#include "../engine.h"
#include <cef.h>

#include "ExtensionCvarApi.h"
#include "ExtensionConnectionApi.h"

class ContainerExtensions {
	std::unique_ptr<ContainerExtensionConnectionApi> connection_api_;

public:
	ContainerExtensions() {
		connection_api_ = std::make_unique<ContainerExtensionConnectionApi>(napi());
		RegisterExtensionCvarApi();
	}
};

ContainerExtensions* container;

void InstallBrowserExtensions() {
	container = new ContainerExtensions;
}

void UnInstallBrowserExtensions() {
	delete container;
}
