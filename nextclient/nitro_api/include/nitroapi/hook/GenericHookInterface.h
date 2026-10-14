#pragma once

#include "HookInterface.h"
#include <functional>

namespace nitroapi
{
    template<class TResult, class... TArgs>
    class GenericHookInterface : public HookInterface
    {
    public:
        virtual void SetCallbackAndEnableHook(std::function<TResult(TArgs...)>&& hook) = 0;
        virtual TResult InvokeOriginal(TArgs... args) = 0;
        virtual TResult Invoke(TArgs... args) = 0;
    };
}