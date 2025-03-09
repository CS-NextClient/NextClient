#include "ExtensionGameUiApi.h"

#include <TaskRun.h>

#include "../GameUi.h"
#include "BasePanel.h"
#include "AcceptedDomains.h"

CExtensionGameUiApiEvents::CExtensionGameUiApiEvents(CefRefPtr<CefV8Context> context)
	: context_(context) { }

void CExtensionGameUiApiEvents::OnActivateGameUI() {
	CefPostTask(TID_UI, new CefFunctionTask([this] {
		CefV8ContextCapture capture(context_);
		if (!context_->GetFrame().get())
			return;

		context_->GetFrame()->ExecuteJavaScript("nextclient.gameUi.dispatchEvent('show')", "", 0);
	}));
}

void CExtensionGameUiApiEvents::OnHideGameUI() {
	CefPostTask(TID_UI, new CefFunctionTask([this] {
		CefV8ContextCapture capture(context_);
		if (!context_->GetFrame().get())
			return;

		context_->GetFrame()->ExecuteJavaScript("nextclient.gameUi.dispatchEvent('hide')", "", 0);
	}));
}

void ContainerExtensionGameUiApi::OnActivateGameUI() {
	for(auto &handler : events_)
		handler->OnActivateGameUI();
}

void ContainerExtensionGameUiApi::OnHideGameUI() {
	for(auto &handler : events_)
		handler->OnHideGameUI();
}

void ContainerExtensionGameUiApi::RunFrame() {
	if(!options_page_to_open_.empty()) {
		BasePanel()->OnOpenOptionsDialog(options_page_to_open_.c_str());
		options_page_to_open_.clear();
	}
}

ContainerExtensionGameUiApi::ContainerExtensionGameUiApi() {
	CefRegisterExtension(
		"ncl/gameui-api",

		"if(!nextclient.gameUi) nextclient.gameUi = { optionsDialog: {} };"
		"(function() {"
		"	native function init(); init();"
		"	nextclient.gameUi.__defineGetter__('shown', function() {"
		"		native function getShownStatus();"
		"		return getShownStatus();"
		"	});"
		"	nextclient.gameUi.optionsDialog.open = function(tabName) {"
		"		native function openOptionsDialogTab();"
		"		if(typeof tabName == 'string')"
		"			openOptionsDialogTab(tabName);"
		"	};"
		"	makeAsEventTarget(nextclient.gameUi);"
		"})()",

		new CefFunctionV8Handler(
			[this](const CefString& name, CefRefPtr<CefV8Value> object, const CefV8ValueList& arguments,
					 CefRefPtr<CefV8Value>& retval, CefString& exception) -> bool {
				
				if(name == "init") {
					auto context = CefV8Context::GetCurrentContext();
					TaskCoro::RunInMainThread([this, context] {
						events_.push_back(std::make_unique<CExtensionGameUiApiEvents>(context));
					});
					return true;
				}
				else if(name == "getShownStatus") {
					auto is_gameui_active = TaskCoro::RunInMainThreadAndWait([this]{
						return GameUI().IsGameUIActive();
					});
					if (is_gameui_active.has_error())
						return false;
					retval = CefV8Value::CreateBool(*is_gameui_active);
					return true;
				}
				else if(name == "openOptionsDialogTab") {
					if(!IsV8CurrentContextOnAcceptedDomain())
						return false;
					
					const auto tab_name = std::string(arguments[0]->GetStringValue());
					TaskCoro::RunInMainThread([this, tab_name]{
						options_page_to_open_ = tab_name;
					});
					return true;
				}

				return false;
			}
		)
	);
}

ContainerExtensionGameUiApi::~ContainerExtensionGameUiApi() {
}