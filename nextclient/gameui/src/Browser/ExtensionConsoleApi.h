#pragma once
#include <memory>
#include <string>
#include <vector>
#include <concepts>

#include <TaskRun.h>
#include <cef.h>
#include <Color.h>
#include <cef_utils/context_capture.h>
#include <cef_utils/function_task.h>

#include <GameConsole.h>
#include "AcceptedDomains.h"
#include "CompletionObserver.h"

/*
	nextclient.console.addEventListener('message', function(event) {
		var address = event.detail.text;
	});
*/

class CExtensionConsoleApiEvent
{
    CefRefPtr<CefV8Context> context_;

public:
    explicit CExtensionConsoleApiEvent(CefRefPtr<CefV8Context> context) :
        context_(context)
    { }

    template<typename T>
        requires std::is_same_v<T, std::string> || std::is_same_v<T, std::wstring>
    bool HandlePrint(const T& text, std::shared_ptr<CompletionObserver> completion)
    {
        bool result = CefPostTask(TID_UI, new CefFunctionTask([this, text, completion]
        {
            CefV8ContextCapture capture(context_);
            if (!context_->GetFrame().get())
            {
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
                {CefV8Value::CreateString("message"), args},
                retval,
                exception,
                false
            );

            completion->Commit(IsV8CurrentContextOnAcceptedDomain() && retval->GetBoolValue() == false);
        }));

        if (!result)
        {
            completion->Commit(false);
        }

        return result;
    }
};


class ContainerExtensionConsoleApi
{
    std::vector<std::unique_ptr<CExtensionConsoleApiEvent>> events_{};
    std::atomic_bool is_initialized_{};

public:
    explicit ContainerExtensionConsoleApi();

    template<typename T>
        requires std::is_same_v<T, std::string> || std::is_same_v<T, std::wstring>
    bool HandlePrint(Color color, const T& text)
    {
        size_t events_count = events_.size();
        if (!events_count)
        {
            return false;
        }

        auto js_completion_callback = [text, color](std::vector<bool> results)
        {
            TaskCoro::RunInMainThread([color, text, results]
            {
                for (auto const result : results)
                {
                    if (result == true)
                    {
                        return;
                    }
                }

                GameConsole().PrintfWithoutJsEvent(color, text);
            });
        };

        for (auto& handler : events_)
        {
            handler->HandlePrint(text, std::make_shared<CompletionObserver>(events_count, js_completion_callback));
        }

        return true;
    }
};
