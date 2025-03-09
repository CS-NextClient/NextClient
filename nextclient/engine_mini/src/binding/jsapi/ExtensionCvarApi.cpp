#include "ExtensionCvarApi.h"
#include <taskcoro/TaskCoro.h>
#include <engine.h>

using namespace taskcoro;

void RegisterExtensionCvarApi() {
	CefRegisterExtension(
		"ncl/cvar-api",

		"(function() {"
		"	nextclient.getCvarValue = function(cvarName) {"
		"		native function getCvarValue();"
		"		return getCvarValue(cvarName);"
		"	};"
		"})()",

		new CefFunctionV8Handler(
			[](const CefString& name, CefRefPtr<CefV8Value> object, const CefV8ValueList& arguments, 
				CefRefPtr<CefV8Value>& retval, CefString& exception) -> bool {
				
				if(name == "getCvarValue" && !arguments.empty()) {
					std::optional<std::string> result;

					auto cvar_name = arguments[0]->GetStringValue();
					auto task = TaskCoro::RunInMainThread<std::optional<std::string>>([cvar_name] {
						cvar_t* cvar = gEngfuncs.pfnGetCvarPointer(cvar_name.c_str());
						return cvar ? std::string(cvar->string) : std::optional<std::string>{};
					});

					try
					{
						result = task.get();
					}
					catch (const std::exception& e)
					{
						return false;
					}

					retval = result
						? CefV8Value::CreateString(*result)
						: CefV8Value::CreateNull();

					return true;
				}
				
				return false;
			}
		)
	);
}