#pragma once

#include <nitroapi/hook/HookInterface.h>
#include <nitroapi/hook/GenericHookInterface.h>
#include "HandlersChainBaseInterface.h"
#include "Unsubscriber.h"
#include <memory>

namespace nitroapi
{
    struct VoidResult{};

    template<class TResult, class... TArgs>
    class NextHandlerInterface
    {
    public:
        virtual TResult Invoke(TArgs...) = 0;
    };

    template<class TResult, class... TArgs>
    class HandlerInterface
    {
    public:
        virtual TResult Handle(TArgs..., NextHandlerInterface<TResult, TArgs...>* next) = 0;
    };

    /* --------------------------
     * SubscriberInterface
     * --------------------------
     */

    template<class TResult, class... TArgs>
    class SubscriberInterface
    {
    public:
        virtual void OnEvent(TArgs..., TResult result) = 0;
    };

    template<class... TArgs>
    class SubscriberInterface<void, TArgs...>
    {
    public:
        virtual void OnEvent(TArgs..., VoidResult result) = 0;
    };

    /* --------------------------
     * HandlersChainInterface
     * --------------------------
     */

    template<class TResult, class... TArgs>
    class HandlersChainInterface : public HandlersChainBaseInterface
    {
    public:
        std::shared_ptr<Unsubscriber> operator|=(const std::function<TResult(TArgs..., NextHandlerInterface<TResult, TArgs...>*)>& func);
        std::shared_ptr<Unsubscriber> operator+=(const std::function<void(TArgs..., TResult)>& func);

    public:
        ~HandlersChainInterface() override = default;

        virtual void AttachHook(GenericHookInterface<TResult, TArgs...>* hook) = 0;
        [[nodiscard]] virtual GenericHookInterface<TResult, TArgs...>* GetHook() const = 0;

        virtual bool AddHandler(HandlerInterface<TResult, TArgs...>* handler) = 0;
        virtual bool RemoveHandler(HandlerInterface<TResult, TArgs...>* handler) = 0;
        virtual bool Subscribe(SubscriberInterface<TResult, TArgs...>* subscriber) = 0;
        virtual bool Unsubscribe(SubscriberInterface<TResult, TArgs...>* subscriber) = 0;
    };

    template<class... TArgs>
    class HandlersChainInterface<void, TArgs...> : public HandlersChainBaseInterface
    {
    public:
        std::shared_ptr<Unsubscriber> operator|=(const std::function<void(TArgs..., NextHandlerInterface<void, TArgs...>*)>& func);
        std::shared_ptr<Unsubscriber> operator+=(const std::function<void(TArgs...)>& func);

    public:
        ~HandlersChainInterface() override = default;

        virtual void AttachHook(GenericHookInterface<void, TArgs...>* hook) = 0;
        [[nodiscard]] virtual GenericHookInterface<void, TArgs...>* GetHook() const = 0;

        virtual bool AddHandler(HandlerInterface<void, TArgs...>* handler) = 0;
        virtual bool RemoveHandler(HandlerInterface<void, TArgs...>* handler) = 0;
        virtual bool Subscribe(SubscriberInterface<void, TArgs...>* subscriber) = 0;
        virtual bool Unsubscribe(SubscriberInterface<void, TArgs...>* subscriber) = 0;
    };
}