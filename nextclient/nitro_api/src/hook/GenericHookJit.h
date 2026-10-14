#pragma once

#include <utility>
#include <funchook.h>
#include <xbyak.h>
#include <easylogging++.h>
#include <platform.h>
#include <nitroapi/hook/GenericHookInterface.h>
#include <nitroapi/hook/CallingConventions.h>
#include "../seh_platform.h"

namespace nitroapi
{
    // !!! PROBABLY HIGH RATE OF DETECTION BY ANTIVIRUS!
    // Supports only cdecl and stdcall conventions
    template<CallingConventions TConv, class TResult, class... TArgs>
    class GenericHookJit : public GenericHookInterface<TResult, TArgs...>
    {
        class Jit : public Xbyak::CodeGenerator
        {
        public:
            uint32_t original_return_address_[8]{};
            uint32_t recursion_depth_ = sizeof(original_return_address_) / sizeof(uint32_t);

        public:
            explicit Jit(GenericHookJit<TConv, TResult, TArgs...>* generic_hook) :
                Xbyak::CodeGenerator(256)
            {
                Xbyak::Label label_ret;

                SaveReturnAddressFromTopStack();

                // set new return address, and add generic_hook argument
                mov(eax, label_ret);
                push(eax);
                mov(dword [esp + 4], (uint32_t)generic_hook);

                jmp(&GenericHookJit<TConv, TResult, TArgs...>::CallHook);
                L(label_ret);

                push(eax); // store eax so as not to lose return value
                LoadReturnAddressToEax();
                mov(dword [esp + 4], eax);
                pop(eax);

                // jump to original return address
                if constexpr (TConv == CallingConventions::Stdcall)
                    ret(sizeof...(TArgs) * 4);
                else
                    ret();
            }

            void SaveReturnAddressFromTopStack()
            {
                mov(edx, dword [&recursion_depth_]);
                dec(edx);
                mov(dword [&recursion_depth_], edx);

                lea(ecx, dword [&original_return_address_]);

                mov(eax, dword [esp]);
                mov(dword [ecx + edx * sizeof(uint32_t)], eax);
            }

            void LoadReturnAddressToEax()
            {
                mov(edx, dword [&recursion_depth_]);
                lea(eax, dword [&original_return_address_]);

                mov(eax, dword [eax + edx * sizeof(uint32_t)]);

                inc(edx);
                mov(dword [&recursion_depth_], edx);
            }
        };

        typedef TResult (__cdecl *FuncTypeCdecl)(TArgs...);
        typedef TResult (__stdcall *FuncTypeStdcall)(TArgs...);

        Jit jit_;
        funchook_t* funchook_;

        uint32_t hook_address_ = 0;
        uint32_t trampoline_address_ = 0;
        bool hook_created_ = false;
        bool hook_active_ = false;

        std::function<TResult(TArgs...)> hook_callback_;

        #pragma warning(disable:4715)
        static TResult __cdecl CallHook(GenericHookJit<TConv, TResult, TArgs...>* generic_hook, TArgs... args)
        {
            SEH_TRY
            {
                return generic_hook->hook_callback_(std::forward<TArgs>(args)...);
            }
            SEH_CATCH_GLOBAL { }
        }
        #pragma warning(default:4715)

    public:
        explicit GenericHookJit(uint32_t hook_address) :
            hook_address_(hook_address),
            jit_(this)
        {
            int status;

            if (hook_address != 0)
            {
                void* generated_hook_func = jit_.template getCode<void*>();

                funchook_ = funchook_create();

                if (funchook_ == nullptr)
                {
                    hook_created_ = false;
                    status = -100;
                }
                else
                {
                    status = funchook_prepare(funchook_, (void**)&hook_address, generated_hook_func);
                    trampoline_address_ = hook_address;
                }
            }
            else
            {
                status = -200;
            }

            hook_created_ = status == FUNCHOOK_ERROR_SUCCESS;
            LOG_IF(!hook_created_, WARNING) << "[jit hook] Can't create hook for address: " << hook_address_ << ". Status: " << status;
        }

        ~GenericHookJit() override
        {
            if (!hook_created_)
                return;

            TryDisableHook();

            hook_created_ = false;

            int status = funchook_destroy(funchook_);
            LOG_IF(status != FUNCHOOK_ERROR_SUCCESS, WARNING) << "[jit hook] Can't remove hook for address: " << hook_address_ << ". Status: " << status;
        }

        void SetCallbackAndEnableHook(std::function<TResult(TArgs...)>&& hook_callback) override
        {
            if (!hook_created_)
            {
                LOG(ERROR) << "[jit hook] Can't enable hook for address: " << hook_address_ << ". Reason: Hook not created.";
                return;
            }

            hook_callback_ = std::move(hook_callback);

            LOG_IF(hook_active_, WARNING) << "[jit hook] Hook callback changed";

            TryEnableHook();
        }

        void UnsetCallbackAndDisableHook() override
        {
            if (!hook_created_)
                return;

            TryDisableHook();

            hook_callback_ = nullptr;
        }

        TResult InvokeOriginal(TArgs... args) override
        {
            uint32_t address = hook_active_ ? trampoline_address_ : hook_address_;

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
            static_assert(TConv != CallingConventions::Fastcall, "Unsupported CallingConventions::Fastcall");
            static_assert(TConv != CallingConventions::Thiscall, "Unsupported CallingConventions::Thiscall");

            if constexpr (TConv == CallingConventions::Cdecl)
                return ((FuncTypeCdecl)address)(std::forward<TArgs>(args)...);

            return ((FuncTypeStdcall)address)(std::forward<TArgs>(args)...);
        }

        void TryDisableHook()
        {
            if (!hook_active_)
                return;

            int status = funchook_uninstall(funchook_, 0);

            if (status == FUNCHOOK_ERROR_SUCCESS)
                hook_active_ = false;
            else
                LOG(WARNING) << "[jit hook] Can't disable hook for address: " << hook_address_ << ". Status: " << status;
        }

        void TryEnableHook()
        {
            if (hook_active_)
                return;

            int status = funchook_install(funchook_, 0);

            if (status == FUNCHOOK_ERROR_SUCCESS)
                hook_active_ = true;
            else
                LOG(WARNING) << "[jit hook] Can't enable hook for address: " << hook_address_ << ". Status: " << status;
        }
    };

    template<class TResult, class... TArgs>
    class GenericCdeclJitHook : public GenericHookJit<CallingConventions::Cdecl, TResult, TArgs...>
    {
    public:
        explicit GenericCdeclJitHook(uint32_t hook_address) :
            GenericHookJit<CallingConventions::Cdecl, TResult, TArgs...>(hook_address)
        { }
    };

    template<class TResult, class... TArgs>
    class GenericStdcallJitHook : public GenericHookJit<CallingConventions::Stdcall, TResult, TArgs...>
    {
    public:
        explicit GenericStdcallJitHook(uint32_t hook_address) :
            GenericHookJit<CallingConventions::Stdcall, TResult, TArgs...>(hook_address)
        { }
    };
}