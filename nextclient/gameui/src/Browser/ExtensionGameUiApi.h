#pragma once

#include <cef.h>
#include <cef_utils.h>
#include <vector>
#include <memory>
#include <string>

class CExtensionGameUiApiEvents {
	CefRefPtr<CefV8Context> context_;

public:
	explicit CExtensionGameUiApiEvents(CefRefPtr<CefV8Context> context);

	void OnActivateGameUI();
	void OnHideGameUI();
};

class ContainerExtensionGameUiApi {
	std::vector<std::unique_ptr<CExtensionGameUiApiEvents>> events_;
	std::string options_page_to_open_;

public:
	ContainerExtensionGameUiApi();
	~ContainerExtensionGameUiApi();

	void RunFrame();

	void OnActivateGameUI();
	void OnHideGameUI();
};