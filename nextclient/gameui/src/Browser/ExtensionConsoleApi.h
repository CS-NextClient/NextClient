#pragma once

#include "CompletionObserver.h"
#include <cef.h>
#include <cef_utils.h>
#include <memory>
#include <string>
#include <vector>

/*
	nextclient.console.addEventListener('message', function(event) {
		var address = event.detail.text;
	});
*/

class CExtensionConsoleApiEvents {
	CefRefPtr<CefV8Context> context_;

public:
	explicit CExtensionConsoleApiEvents(CefRefPtr<CefV8Context> context);

	bool OnMessage(std::string text, std::shared_ptr<CompletionObserver> completion);
};

class ContainerExtensionConsoleApi {
	std::vector<std::unique_ptr<CExtensionConsoleApiEvents>> events_;

public:
	ContainerExtensionConsoleApi();
	~ContainerExtensionConsoleApi();

	bool OnMessage(const char* text);
};