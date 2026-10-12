#pragma once

namespace nitroapi
{
    class HandlersChainBaseInterface
    {
    public:
        virtual ~HandlersChainBaseInterface() = default;

        virtual void DetachHook() = 0;
        virtual bool IsHookAttached() = 0;

        [[nodiscard]] virtual uint32_t GetSubscribersCount() const = 0;
        [[nodiscard]] virtual uint32_t GetHandlersCount() const = 0;
    };
}