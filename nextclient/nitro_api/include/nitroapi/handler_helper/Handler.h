#pragma once

#include <nitroapi/chain/HandlersChainInterface.h>

namespace nitroapi
{
    template<class TResult, class... TArgs>
    class Handler : public HandlerInterface<TResult, TArgs...>
    {
        std::function<TResult(TArgs..., NextHandlerInterface<TResult, TArgs...>*)> callback_;

    public:
        explicit Handler(std::function<TResult(TArgs..., NextHandlerInterface<TResult, TArgs...>*)> callback)
        {
            callback_ = callback;
        }

        TResult Handle(TArgs... args, NextHandlerInterface<TResult, TArgs...> *next) override
        {
            return callback_(std::forward<TArgs>(args)..., next);
        }
    };
}