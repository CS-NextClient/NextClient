#pragma once

#include <unordered_map>
#include <utility>
#include <funchook.h>
#include <easylogging++.h>
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
    template<StaticHookId HookId, CallingConventions TConv, class TResult, class... TArgs>
    class GenericHookStatic : public GenericHookInterface<TResult, TArgs...>
    {
        typedef TResult (__cdecl *FuncTypeCdecl)(TArgs...);
        typedef TResult (__stdcall *FuncTypeStdcall)(TArgs...);
        typedef TResult (__thiscall *FuncTypeThiscall)(TArgs...);
        typedef TResult (__fastcall *FuncTypeFastcall)(TArgs...);

        funchook_t* funchook_;

        uint32_t hook_address_ = 0;
        uint32_t trampoline_address_ = 0;
        bool hook_created_ = false;
        bool hook_active_ = false;

        static std::function<TResult(TArgs...)> s_callback_;

    public:
        explicit GenericHookStatic(uint32_t hook_address) :
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
                    if constexpr (TConv == CallingConventions::Cdecl)
                        status = funchook_prepare(funchook_, (void**)&hook_address, (void*)CallHookCdecl);
                    else if constexpr (TConv == CallingConventions::Stdcall)
                        status = funchook_prepare(funchook_, (void**)&hook_address, (void*)CallHookStdcall);
                    else if constexpr (TConv == CallingConventions::Fastcall)
                        status = funchook_prepare(funchook_, (void**)&hook_address, (void*)CallHookFastcall);
                    else
                        status = funchook_prepare(funchook_, (void**)&hook_address, (void*)CallHookThiscall);

                    trampoline_address_ = hook_address;
                }
            }
            else
            {
                status = -200;
            }

            hook_created_ = status == FUNCHOOK_ERROR_SUCCESS;
            LOG_IF(!hook_created_, WARNING) << "[static hook] Can't create hook for " << magic_enum::enum_name(HookId) << ". Address: " << hook_address_ << ". Status: " << status;
        }

        ~GenericHookStatic() override
        {
            if (!hook_created_)
                return;

            TryDisableHook();

            hook_created_ = false;
            s_callback_ = nullptr;

            int status = funchook_destroy(funchook_);
            LOG_IF(status != FUNCHOOK_ERROR_SUCCESS, WARNING) << "[static hook] Can't remove hook for " << magic_enum::enum_name(HookId) << " address: " << hook_address_ << ". Status: " << status;
        }

        void SetCallbackAndEnableHook(std::function<TResult(TArgs...)>&& hook_callback) override
        {
            if (!hook_created_)
            {
                LOG(ERROR) << "[static hook] Can't enable hook for " << magic_enum::enum_name(HookId) << ". Address: " << hook_address_ << ". Reason: Hook not created.";
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

            LOG_IF(hook_active_, WARNING) << "[static hook] Hook callback changed for " << magic_enum::enum_name(HookId);

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
            if constexpr (TConv == CallingConventions::Cdecl)
                return ((FuncTypeCdecl)address)(std::forward<TArgs>(args)...);
            else if constexpr (TConv == CallingConventions::Stdcall)
                return ((FuncTypeStdcall)address)(std::forward<TArgs>(args)...);
            else if constexpr (TConv == CallingConventions::Fastcall)
                return ((FuncTypeFastcall)address)(std::forward<TArgs>(args)...);

            return ((FuncTypeThiscall)address)(std::forward<TArgs>(args)...);
        }

        void TryDisableHook()
        {
            if (!hook_active_)
                return;

            int status = funchook_uninstall(funchook_, 0);

            if (status == FUNCHOOK_ERROR_SUCCESS)
                hook_active_ = false;
            else
                LOG(WARNING) << "[static hook] Can't disable hook for " << magic_enum::enum_name(HookId) << ". Address: " << hook_address_ << ". Status: " << status;
        }

        void TryEnableHook()
        {
            if (hook_active_)
                return;

            int status = funchook_install(funchook_, 0);

            if (status == FUNCHOOK_ERROR_SUCCESS)
                hook_active_ = true;
            else
                LOG(WARNING) << "[static hook] Can't enable hook for " << magic_enum::enum_name(HookId) << " address: " << hook_address_ << ". Status: " << status;
        }

#pragma warning(disable:4715)
        static TResult __cdecl CallHookCdecl(TArgs... args)
        {
            SEH_TRY { return s_callback_(std::forward<TArgs>(args)...); }
            SEH_CATCH_GLOBAL { }
        }

        static TResult __stdcall CallHookStdcall(TArgs... args)
        {
            SEH_TRY { return s_callback_(std::forward<TArgs>(args)...); }
            SEH_CATCH_GLOBAL { }
        }

        static TResult __thiscall CallHookThiscall(TArgs... args)
        {
            SEH_TRY { return s_callback_(std::forward<TArgs>(args)...); }
            SEH_CATCH_GLOBAL { }
        }

        static TResult __fastcall CallHookFastcall(TArgs... args)
        {
            SEH_TRY { return s_callback_(std::forward<TArgs>(args)...); }
            SEH_CATCH_GLOBAL { }
        }
#pragma warning(default:4715)
    };

    template<StaticHookId HookId, CallingConventions TConv, class TResult, class... TArgs>
    std::function<TResult(TArgs...)> GenericHookStatic<HookId, TConv, TResult, TArgs...>::s_callback_;
}