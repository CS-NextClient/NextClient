#include "ExtensionConsoleApi.h"
#include <format>
#include <GameUi.h>
#include <TaskRun.h>
#include <cef_utils/function_handler.h>

#include "AcceptedDomains.h"

ContainerExtensionConsoleApi::ContainerExtensionConsoleApi()
{
    CefRegisterExtension(
        "ncl/console-api",

        "if(!nextclient.console) nextclient.console = {};"
            "(function() {"
            "	native function init(); init();"
            "	makeAsEventTarget(nextclient.console);"
            "})()",

        new CefFunctionV8Handler(
            [this](const CefString& name, CefRefPtr<CefV8Value> object,
                   const CefV8ValueList& arguments, CefRefPtr<CefV8Value>& retval, CefString& exception) -> bool
            {
                if (name == "init")
                {
                    if (is_initialized_)
                    {
                        return true;
                    }

                    auto context = CefV8Context::GetCurrentContext();
                    TaskCoro::RunInMainThread([this, context]
                    {
                        events_.push_back(std::make_unique<CExtensionConsoleApiEvent>(context));
                    });

                    is_initialized_ = true;
                    return true;
                }

                return false;
            }
        )
    );
}
