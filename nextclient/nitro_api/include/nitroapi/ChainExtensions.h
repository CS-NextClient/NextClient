#pragma once

#include "chain/Unsubscriber.h"
#include "chain/HandlersChainInterface.h"
#include "utility/ContainerWithHandles.h"
#include <memory>

namespace nitroapi
{
    utility::ContainerWithHandles<int, void*>& HandlersStorage();

    /* --------------------------
     * Proxies
     * --------------------------
     */

    template<class TResult, class... TArgs>
    class ProxyHandler : public HandlerInterface<TResult, TArgs...>
    {
        std::function<TResult(TArgs..., NextHandlerInterface<TResult, TArgs...>*)> callback_;

    public:
        explicit ProxyHandler(const std::function<TResult(TArgs..., NextHandlerInterface<TResult, TArgs...>*)>& callback) :
            callback_(callback)
        { }

        TResult Handle(TArgs... args, NextHandlerInterface<TResult, TArgs...>* next) override
        {
            return callback_(std::forward<TArgs>(args)..., next);
        }
    };

    template<class TResult, class... TArgs>
    class ProxySubscriber : public SubscriberInterface<TResult, TArgs...>
    {
        std::function<void(TArgs..., TResult)> callback_;

    public:
        explicit ProxySubscriber(const std::function<void(TArgs..., TResult)>& callback) :
            callback_(callback)
        { }

        void OnEvent(TArgs... args, TResult result) override
        {
            return callback_(std::forward<TArgs>(args)..., result);
        }
    };

    template<class... TArgs>
    class ProxySubscriber<void, TArgs...> : public SubscriberInterface<void, TArgs...>
    {
        std::function<void(TArgs...)> callback_;

    public:
        explicit ProxySubscriber(const std::function<void(TArgs...)>& callback) :
            callback_(callback)
        { }

        void OnEvent(TArgs... args, VoidResult result) override
        {
            return callback_(std::forward<TArgs>(args)...);
        }
    };


    /* --------------------------
     * Operators for handlers
     * --------------------------
     */

    template<class TResult, class... TArgs>
    std::shared_ptr<Unsubscriber> HandlersChainInterface<TResult, TArgs...>::operator|=
        (const std::function<TResult(TArgs..., NextHandlerInterface<TResult, TArgs...>*)>& func)
    {
        auto proxy_handler = new ProxyHandler<TResult, TArgs...>(func);

        int handle_id = HandlersStorage().Add((void*)proxy_handler);
        this->AddHandler(proxy_handler);

        return std::make_shared<Unsubscriber>([this, handle_id]() {
            if (HandlersStorage().contains(handle_id))
            {
                auto proxy_handler = (ProxyHandler<TResult, TArgs...>*)HandlersStorage().at(handle_id);
                this->RemoveHandler(proxy_handler);

                delete proxy_handler;
            }

            HandlersStorage().Remove(handle_id);
        });
    }

    template<class... TArgs>
    std::shared_ptr<Unsubscriber> HandlersChainInterface<void, TArgs...>::operator|=
        (const std::function<void(TArgs..., NextHandlerInterface<void, TArgs...>*)>& func)
    {
        auto proxy_handler = new ProxyHandler<void, TArgs...>(func);

        int handle_id = HandlersStorage().Add((void*)proxy_handler);
        this->AddHandler(proxy_handler);

        return std::make_shared<Unsubscriber>([this, handle_id]() {
            if (HandlersStorage().contains(handle_id))
            {
                auto proxy_handler = (ProxyHandler<void, TArgs...>*)HandlersStorage().at(handle_id);
                this->RemoveHandler(proxy_handler);

                delete proxy_handler;
            }

            HandlersStorage().Remove(handle_id);
        });
    }

    /* --------------------------
    *  Operators for subscribers
    * --------------------------
    */

    template<class TResult, class... TArgs>
    std::shared_ptr<Unsubscriber> HandlersChainInterface<TResult, TArgs...>::operator+=
        (const std::function<void(TArgs..., TResult)>& func)
    {
        auto proxy_subscriber = new ProxySubscriber<TResult, TArgs...>(func);

        int handle_id = HandlersStorage().Add((void*)proxy_subscriber);
        this->Subscribe(proxy_subscriber);

        return std::make_shared<Unsubscriber>([this, handle_id]() {
            if (HandlersStorage().contains(handle_id))
            {
                auto proxy_subscriber = (ProxySubscriber<TResult, TArgs...>*)HandlersStorage().at(handle_id);
                this->Unsubscribe(proxy_subscriber);

                delete proxy_subscriber;
            }

            HandlersStorage().Remove(handle_id);
        });
    }

    template<class... TArgs>
    std::shared_ptr<Unsubscriber> HandlersChainInterface<void, TArgs...>::operator+=
        (const std::function<void(TArgs...)>& func)
    {
        auto proxy_subscriber = new ProxySubscriber<void, TArgs...>(func);

        int handle_id = HandlersStorage().Add((void*)proxy_subscriber);
        this->Subscribe(proxy_subscriber);

        return std::make_shared<Unsubscriber>([this, handle_id]() {
            if (HandlersStorage().contains(handle_id))
            {
                auto proxy_subscriber = (ProxySubscriber<void, TArgs...>*)HandlersStorage().at(handle_id);
                this->Unsubscribe(proxy_subscriber);

                delete proxy_subscriber;
            }

            HandlersStorage().Remove(handle_id);
        });
    }
}