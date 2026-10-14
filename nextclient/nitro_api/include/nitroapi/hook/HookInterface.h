#pragma once

#include <cstdint>

namespace nitroapi
{
    class HookInterface
    {
    public:
        virtual ~HookInterface() = default;
        virtual void UnsetCallbackAndDisableHook() = 0;
        [[nodiscard]] virtual uint32_t GetTrampolineAddress() const = 0;
        [[nodiscard]] virtual uint32_t GetOriginalFunctionAddress() const = 0;
        [[nodiscard]] virtual bool IsHookActive() const = 0;
    };
}