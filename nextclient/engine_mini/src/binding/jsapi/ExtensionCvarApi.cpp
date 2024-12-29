#include "ExtensionCvarApi.h"
#include <task/TaskRun.h>
#include <engine.h>

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
				
				if(name == "getCvarValue" && arguments.size() > 0) {
					auto cvarName = arguments[0]->GetStringValue();
					auto taskResult = TaskRun::RunInMainThreadAndWait([cvarName] {
						cvar_t* cvar = gEngfuncs.pfnGetCvarPointer(cvarName.c_str());
						return cvar ? std::string(cvar->string) : std::optional<std::string>{};
					});
					if (taskResult.has_error())
						return false;

					auto cvarValue = *taskResult;
					retval = cvarValue
						? CefV8Value::CreateString(*cvarValue)
						: CefV8Value::CreateNull();

					return true;
				}
				
				return false;
			}
		)
	);
}