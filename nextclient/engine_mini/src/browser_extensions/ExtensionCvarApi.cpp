#include "ExtensionCvarApi.h"

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
					auto cvarPtr = gEngfuncs.pfnGetCvarPointer(cvarName.c_str());

					retval = cvarPtr != nullptr 
						? CefV8Value::CreateString(cvarPtr->string) 
						: CefV8Value::CreateNull();

					return true;
				}
				
				return false;
			}
		)
	);
}