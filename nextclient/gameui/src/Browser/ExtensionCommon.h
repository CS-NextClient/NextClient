#pragma once

#include <cef.h>
#include <interface.h>
#include <goldsrc/client_api.h>

class CExtensionCommonHandler : public CefV8Handler {
	IMPLEMENT_REFCOUNTING(CExtensionCommonHandler);
	goldsrc::cl_enginefunc_t* engine_;

public:
	CExtensionCommonHandler(goldsrc::cl_enginefunc_t* engine): engine_(engine) {};

	bool Execute(
		const CefString& name, CefRefPtr<CefV8Value> object,
		const CefV8ValueList& arguments, CefRefPtr<CefV8Value>& retval, CefString& exception
	);
};

void RegisterCommonJsApi(goldsrc::cl_enginefunc_t* engine);