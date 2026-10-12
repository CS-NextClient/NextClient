#pragma once

#include <cstdarg>
#include <type_traits>
#include <utility>
#include <funchook.h>
#include <easylogging++.h>
#include <strtools.h>
#include <nitroapi/hook/GenericHookInterface.h>
#include <nitroapi/hook/CallingConventions.h>
#include <nitroapi/hook/StaticHookId.h>
#include <platform.h>
#include "../seh_platform.h"

#ifdef NITROAPI_USE_PROFILER
#include "../profiler.h"
#endif

namespace nitroapi
{
    template<StaticHookId HookId, class TResult, class... TArgs>
    class GenericHookVaformatStatic : public GenericHookInterface<TResult, TArgs...>
    {
        static_assert(sizeof...(TArgs) == 1 && std::is_same_v<std::remove_reference_t<TArgs>..., const char*>, "Only one argument allowed: const char*");

        typedef TResult (__cdecl *FuncTypeCdecl)(TArgs...);

        funchook_t* funchook_;

        uint32_t hook_address_ = 0;
        uint32_t trampoline_address_ = 0;
        bool hook_created_ = false;
        bool hook_active_ = false;

        static std::function<TResult(const char*)> s_callback_;

    public:
        explicit GenericHookVaformatStatic(uint32_t hook_address) :
                hook_address_(hook_address)
        {
            int status;

            if (hook_address != 0)
            {
                funchook_ = funchook_create();

                if (funchook_ == nullptr)
                {
                    hook_created_ = false;
                    status = -100;
                }
                else
                {
                    status = funchook_prepare(funchook_, (void**)&hook_address, (void*)CallHookCdecl);
                    trampoline_address_ = hook_address;
                }
            }
            else
            {
                status = -200;
            }


            hook_created_ = status == FUNCHOOK_ERROR_SUCCESS;
            LOG_IF(!hook_created_, WARNING) << "[vaformat hook] Can't create hook for " << magic_enum::enum_name(HookId) << ". Address: " << hook_address_ << ". Status: " << status;
        }

        ~GenericHookVaformatStatic() override
        {
            if (!hook_created_)
                return;

            TryDisableHook();

            hook_created_ = false;
            s_callback_ = nullptr;

            int status = funchook_destroy(funchook_);
            LOG_IF(status != FUNCHOOK_ERROR_SUCCESS, WARNING) << "[vaformat hook] Can't remove hook for " << magic_enum::enum_name(HookId) << " address: " << hook_address_ << ". Status: " << status;
        }

        void SetCallbackAndEnableHook(std::function<TResult(TArgs...)>&& hook_callback) override
        {
            if (!hook_created_)
            {
                LOG(ERROR) << "[vaformat hook] Can't enable hook for " << magic_enum::enum_name(HookId) << ". Address: " << hook_address_ << ". Reason: Hook not created.";
                return;
            }

#ifdef NITROAPI_USE_PROFILER
            s_callback_ = [hook_callback = std::move(hook_callback)](TArgs... args) -> TResult {
                if (!g_ProfilerInsideFrame)
                    return hook_callback(std::forward<TArgs>(args)...);

                static char event_name[64] = {'\0'};
                if (event_name[0] == '\0')
                    V_snprintf(event_name, sizeof(event_name), "Hook %s", magic_enum::enum_name(HookId));

                OPTICK_EVENT(event_name);

                return hook_callback(std::forward<TArgs>(args)...);
            };
#else
            s_callback_ = std::move(hook_callback);
#endif

            LOG_IF(hook_active_, WARNING) << "[vaformat hook] Hook callback changed for " << magic_enum::enum_name(HookId);

            TryEnableHook();
        }

        void UnsetCallbackAndDisableHook() override
        {
            if (!hook_created_)
                return;

            TryDisableHook();

            s_callback_ = nullptr;
        }

        TResult InvokeOriginal(TArgs... args) override
        {
            uint32_t address = hook_active_ ? trampoline_address_ : hook_address_;

#ifdef NITROAPI_USE_PROFILER
            if (g_ProfilerInsideFrame && hook_active_)
            {
                static char event_name[64] = {'\0'};
                if (event_name[0] == '\0')
                    V_snprintf(event_name, sizeof(event_name), "Original %s", magic_enum::enum_name(HookId));

                OPTICK_EVENT(event_name);

                return InvokeInternal(address, std::forward<TArgs>(args)...);
            }
#endif

            return InvokeInternal(address, std::forward<TArgs>(args)...);
        }

        TResult Invoke(TArgs... args) override
        {
            return InvokeInternal(hook_address_, std::forward<TArgs>(args)...);
        }

        [[nodiscard]] uint32_t GetTrampolineAddress() const override { return trampoline_address_; }

        [[nodiscard]] uint32_t GetOriginalFunctionAddress() const override { return hook_address_; }

        [[nodiscard]] bool IsHookActive() const override { return hook_active_; }

    private:
        TResult InvokeInternal(uint32_t address, TArgs... args)
        {
            return ((FuncTypeCdecl)address)(std::forward<TArgs>(args)...);
        }

        void TryDisableHook()
        {
            if (!hook_active_)
                return;

            int status = funchook_uninstall(funchook_, 0);

            if (status == FUNCHOOK_ERROR_SUCCESS)
                hook_active_ = false;
            else
                LOG(WARNING) << "[vaformat hook] Can't disable hook for " << magic_enum::enum_name(HookId) << ". Address: " << hook_address_ << ". Status: " << status;
        }

        void TryEnableHook()
        {
            if (hook_active_)
                return;

            int status = funchook_install(funchook_, 0);

            if (status == FUNCHOOK_ERROR_SUCCESS)
                hook_active_ = true;
            else
                LOG(WARNING) << "[vaformat hook] Can't enable hook for " << magic_enum::enum_name(HookId) << " address: " << hook_address_ << ". Status: " << status;
        }

#pragma warning(disable:4715)
        static TResult __cdecl CallHookCdecl(const char* format, ...)
        {
            SEH_TRY {
                char text[4096];

                va_list params;
                va_start(params, format);
                int result = V_vsnprintf(text, sizeof(text) - 1, format, params);
                va_end(params);

                return s_callback_(text);
            }
            SEH_CATCH_GLOBAL { }
        }
#pragma warning(default:4715)
    };

    template<StaticHookId HookId, class TResult, class... TArgs>
    std::function<TResult(const char*)> GenericHookVaformatStatic<HookId, TResult, TArgs...>::s_callback_;
}