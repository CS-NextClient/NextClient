#include "ExtensionConsoleApi.h"
#include <format>
#include <GameConsole.h>
#include "AcceptedDomains.h"

CExtensionConsoleApiEvents::CExtensionConsoleApiEvents(CefRefPtr<CefV8Context> context)
	: context_(context) { }

bool CExtensionConsoleApiEvents::OnMessage(std::string text, CompletionObserver* completion) {
	bool result = CefPostTask(TID_UI, new CefFunctionTask([this, text, completion] {
		CefV8ContextCapture capture(context_);
		if (!context_->GetFrame().get()) {
			completion->Commit(false);
			return;
		}

		auto console = context_
			->GetGlobal()
			->GetValue("nextclient")
			->GetValue("console");

		auto dispatchEvent = console->GetValue("dispatchEvent");

		auto args = CefV8Value::CreateObject(nullptr);
		args->SetValue("text", CefV8Value::CreateString(text), V8_PROPERTY_ATTRIBUTE_NONE);

		CefRefPtr<CefV8Value> retval;
		CefRefPtr<CefV8Exception> exception;
		
		dispatchEvent->ExecuteFunction(
			console, 
			{ CefV8Value::CreateString("message"), args }, 
			retval, 
			exception, 
			false
		);

		completion->Commit(IsV8CurrentContextOnAcceptedDomain() && retval->GetBoolValue() == false);
	}));

	if(!result)
		completion->Commit(false);

	return result;
}

bool ContainerExtensionConsoleApi::OnMessage(const char* text) {
	auto events_count = events_.size();
	if(!events_count) return false;

	auto observer = new CompletionObserver(events_count, [text = std::string(text)](std::vector<bool>& results) {
		for(auto const result : results) {
			if(result == true)
				return;
		}

		GameConsole().PrintfWithoutJsEvent(text.c_str());
	});	

	for (auto& handler : events_)
		handler->OnMessage(text, observer);

	return true;
}

ContainerExtensionConsoleApi::ContainerExtensionConsoleApi() {
	CefRegisterExtension(
		"ncl/console-api",

		"if(!nextclient.console) nextclient.console = {};"
		"(function() {"
		"	native function init(); init();"
		"	makeAsEventTarget(nextclient.console);"
		"})()",

		new CefFunctionV8Handler(
			[this](const CefString& name, CefRefPtr<CefV8Value> object,
				const CefV8ValueList& arguments, CefRefPtr<CefV8Value>& retval, CefString& exception) -> bool {
				if (name == "init") {
					events_.push_back(
						std::make_unique<CExtensionConsoleApiEvents>(CefV8Context::GetCurrentContext()));
					return true;
				}
				return false;
			}
		)
	);
}

ContainerExtensionConsoleApi::~ContainerExtensionConsoleApi() { }