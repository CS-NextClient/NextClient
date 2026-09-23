#pragma once
#include <nitroapi/hook/GenericHookInterface.h>
#include <nitroapi/chain/HandlersChainInterface.h>
#include <easylogging++.h>
#include <algorithm>
#include <cstdint>
#include <ios>
#include <sstream>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace nitroapi
{
    template <class TResult, class... TArgs>
    class HandlersChain : public HandlersChainInterface<TResult, TArgs...>
    {
        class InvocationScope
        {
            HandlersChain* chain_;

        public:
            explicit InvocationScope(HandlersChain* chain) :
                chain_(chain)
            {
                chain_->invocation_depth_++;
            }

            ~InvocationScope()
            {
                chain_->invocation_depth_--;
            }
        };

        class NextHandler : public NextHandlerInterface<TResult, TArgs...>
        {
            HandlersChain* chain_;
            std::size_t* handler_index_;
            std::size_t handlers_count_;

        public:
            NextHandler(HandlersChain* chain, std::size_t* handler_index, std::size_t handlers_count) :
                chain_(chain),
                handler_index_(handler_index),
                handlers_count_(handlers_count)
            {}

            TResult Invoke(TArgs... args) override
            {
                while (*handler_index_ < handlers_count_)
                {
                    auto handler = chain_->handlers_[(*handler_index_)++];
                    if (handler != nullptr)
                    {
                        if constexpr (std::is_void_v<TResult>)
                        {
                            handler->Handle(std::forward<TArgs>(args)..., this);
                            return;
                        }
                        else
                        {
                            return handler->Handle(std::forward<TArgs>(args)..., this);
                        }
                    }
                }

                if constexpr (std::is_void_v<TResult>)
                {
                    chain_->hook_->InvokeOriginal(std::forward<TArgs>(args)...);
                    return;
                }
                else
                {
                    return chain_->hook_->InvokeOriginal(std::forward<TArgs>(args)...);
                }
            }
        };

        GenericHookInterface<TResult, TArgs...>* hook_{};
        std::vector<HandlerInterface<TResult, TArgs...>*> handlers_{};
        std::vector<SubscriberInterface<TResult, TArgs...>*> subscribers_{};

        std::string description_{};
        uint32_t invocation_depth_{};

    public:
        explicit HandlersChain(std::string description = {}) :
            description_(std::move(description))
        {}

        ~HandlersChain() override = default;

        void AttachHook(GenericHookInterface<TResult, TArgs...>* hook) override
        {
            DetachHook();

            hook_ = hook;
            hook->SetCallbackAndEnableHook([this](TArgs... args) -> TResult {
                return GenericHook(std::forward<TArgs>(args)...);
            });

            LogChainEvent("AttachHook", hook_, handlers_.size());
        }

        void DetachHook() override
        {
            if (hook_ == nullptr)
                return;

            LogChainEvent("DetachHook", hook_, handlers_.size());

            hook_->UnsetCallbackAndDisableHook();
            hook_ = nullptr;
        }

        [[nodiscard]] bool IsHookAttached() override
        {
            return hook_ != nullptr;
        }

        [[nodiscard]] GenericHookInterface<TResult, TArgs...>* GetHook() const override
        {
            return hook_;
        }

        [[nodiscard]] uint32_t GetSubscribersCount() const override
        {
            return CountActiveSubscribers();
        }

        [[nodiscard]] uint32_t GetHandlersCount() const override
        {
            return CountActiveHandlers();
        }

        bool AddHandler(HandlerInterface<TResult, TArgs...>* handler) override
        {
            if (handler == nullptr)
            {
                LOG(WARNING) << "HandlersChain AddHandler ignored null handler chain=" << (const void*)this;
                return false;
            }

            size_t index = FindItemIndex(handlers_, handler);
            if (index != handlers_.size())
            {
                LogChainEvent("AddHandler duplicate", handler, index);
                return false;
            }

            index = StoreItem(handlers_, handler, invocation_depth_ == 0);

            LogReentrantMutationWarning("AddHandler", handler, index);
            LogChainEvent("AddHandler", handler, index);
            return true;
        }

        bool RemoveHandler(HandlerInterface<TResult, TArgs...>* handler) override
        {
            if (handler == nullptr)
            {
                LOG(WARNING) << "HandlersChain RemoveHandler ignored null handler chain=" << (const void*)this;
                return false;
            }

            size_t index = FindItemIndex(handlers_, handler);
            if (index == handlers_.size())
            {
                LogChainEvent("RemoveHandler not_found", handler, index);
                return false;
            }

            handlers_[index] = nullptr;

            LogReentrantMutationWarning("RemoveHandler", handler, index);
            LogChainEvent("RemoveHandler", handler, index);
            return true;
        }

        bool Subscribe(SubscriberInterface<TResult, TArgs...>* subscriber) override
        {
            if (subscriber == nullptr)
            {
                LOG(WARNING) << "HandlersChain Subscribe ignored null subscriber chain=" << (const void*)this;
                return false;
            }

            size_t index = FindItemIndex(subscribers_, subscriber);
            if (index != subscribers_.size())
            {
                LogChainEvent("Subscribe duplicate", subscriber, index);
                return false;
            }

            index = StoreItem(subscribers_, subscriber, invocation_depth_ == 0);

            LogReentrantMutationWarning("Subscribe", subscriber, index);
            LogChainEvent("Subscribe", subscriber, index);
            return true;
        }

        bool Unsubscribe(SubscriberInterface<TResult, TArgs...>* subscriber) override
        {
            if (subscriber == nullptr)
            {
                LOG(WARNING) << "HandlersChain Unsubscribe ignored null subscriber chain=" << (const void*)this;
                return false;
            }

            size_t index = FindItemIndex(subscribers_, subscriber);
            if (index == subscribers_.size())
            {
                LogChainEvent("Unsubscribe not_found", subscriber, index);
                return false;
            }

            subscribers_[index] = nullptr;

            LogReentrantMutationWarning("Unsubscribe", subscriber, index);
            LogChainEvent("Unsubscribe", subscriber, index);
            return true;
        }

    private:
        TResult GenericHook(TArgs... args)
        {
            InvocationScope invocation_scope(this);

            std::size_t handlers_count = handlers_.size();
            std::size_t subscribers_count = subscribers_.size();

            std::size_t handler_index = 0;
            NextHandler next_handler(this, &handler_index, handlers_count);

            if constexpr (std::is_void_v<TResult>)
            {
                next_handler.Invoke(args...);

                for (int i = (int)subscribers_count - 1; i >= 0; i--)
                {
                    auto subscriber = subscribers_[i];
                    if (subscriber != nullptr)
                        subscriber->OnEvent(args..., VoidResult{});
                }
            }
            else
            {
                TResult result = next_handler.Invoke(args...);

                for (int i = (int)subscribers_count - 1; i >= 0; i--)
                {
                    auto subscriber = subscribers_[i];
                    if (subscriber != nullptr)
                        subscriber->OnEvent(args..., result);
                }

                return result;
            }
        }
        
        template <class TItem>
        static std::size_t FindItemIndex(const std::vector<TItem*>& items, TItem* item)
        {
            auto iterator = std::find(items.begin(), items.end(), item);
            if (iterator == items.end())
                return items.size();

            return (std::size_t)(iterator - items.begin());
        }

        template <class TItem>
        static std::size_t StoreItem(std::vector<TItem*>& items, TItem* item, bool reuse_empty_slots)
        {
            if (reuse_empty_slots)
            {
                auto iterator = std::find(items.begin(), items.end(), nullptr);
                if (iterator != items.end())
                {
                    *iterator = item;
                    return (std::size_t)(iterator - items.begin());
                }
            }

            items.push_back(item);
            return items.size() - 1;
        }

        template <class TItem>
        static uint32_t CountActiveItems(const std::vector<TItem*>& items)
        {
            return (uint32_t)std::count_if(items.begin(), items.end(), [](TItem* item) { return item != nullptr; });
        }

        [[nodiscard]] uint32_t CountActiveHandlers() const
        {
            return CountActiveItems(handlers_);
        }

        [[nodiscard]] uint32_t CountActiveSubscribers() const
        {
            return CountActiveItems(subscribers_);
        }

        std::string BuildChainStateLogMessage(const void* item, std::size_t index) const
        {
            std::ostringstream stream;
            stream << " hook_name=\"" << description_ << "\""
                   << " chain=" << (const void*)this << " item=" << item << " index=" << index << " invocation_depth=" << invocation_depth_
                   << " active_handlers=" << CountActiveHandlers() << " handler_slots=" << handlers_.size()
                   << " active_subscribers=" << CountActiveSubscribers() << " subscriber_slots=" << subscribers_.size()
                   << " hook=" << (const void*)hook_ << " hook_active=" << (hook_ != nullptr && hook_->IsHookActive())
                   << " hook_original=0x" << std::hex << (hook_ != nullptr ? hook_->GetOriginalFunctionAddress() : 0) << std::dec
                   << " hook_trampoline=0x" << std::hex << (hook_ != nullptr ? hook_->GetTrampolineAddress() : 0) << std::dec;

            return stream.str();
        }

        void LogChainEvent(const char* event, const void* item, std::size_t index) const
        {
            LOG(INFO) << "HandlersChain " << event << BuildChainStateLogMessage(item, index);
        }

        void LogReentrantMutationWarning(const char* event, const void* item, std::size_t index) const
        {
            if (invocation_depth_ == 0)
                return;

            LOG(WARNING) << "HandlersChain reentrant mutation event=" << event << BuildChainStateLogMessage(item, index);
        }
    };
} // namespace nitroapi
