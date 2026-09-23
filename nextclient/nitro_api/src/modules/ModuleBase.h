#pragma once

#include <nitroapi/hook/StaticHookId.h>
#include <nitroapi/NitroFunction.h>
#include <nitroapi/Concepts.h>
#include "../LibraryHookListener.h"
#include "../AddressProviderBase.h"
#include "../chain/HandlersChain.h"
#include "../hook/GenericHookStatic.h"
#include "../HookStorage.h"

namespace nitroapi
{
    class ModuleBase : public LibraryHookListener
    {
        std::vector<std::function<void(nitro_utils::SysModule)>> on_library_loaded_;
        std::vector<std::function<void()>> on_library_unloaded_;

    protected:
        std::shared_ptr<HookStorage> hook_storage_;
        std::shared_ptr<AddressProviderBase> addr_provider_;

    public:
        explicit ModuleBase(std::shared_ptr<HookStorage> hook_storage, std::shared_ptr<AddressProviderBase> addr_provider);

        void OnLibraryLoaded(nitro_utils::SysModule hModule) override;
        void OnLibraryUnload() override;

        template<Pointer TVar>
        void RegisterVar(TVar* var, const std::string& key)
        {
            on_library_loaded_.emplace_back([this, key, var](nitro_utils::SysModule hModule) {
                *var = (TVar)addr_provider_->FindAddress(hModule, key);
            });

            on_library_unloaded_.emplace_back([this, var]() {
                *var = nullptr;
            });
        }

// region STATIC HOOKS
        template<StaticHookId HookId, class TResult, class... TArgs>
        void RegisterFuncThiscall(NitroFunctionBase<TResult, TArgs...>* nitro_function)
        {
            auto on_lib_loaded = [this, nitro_function](nitro_utils::SysModule hModule) {
                auto* hook = hook_storage_->GetStaticHook<HookId, CallingConventions::Thiscall, TResult, TArgs...>(addr_provider_->FindAddress(hModule, HookId));
                nitro_function->SetHook(hook);
            };

            RegisterFunc(nitro_function, std::move(on_lib_loaded), std::string(magic_enum::enum_name(HookId)));
        }

        template<StaticHookId HookId, class TResult, class... TArgs>
        void RegisterFuncCdecl(NitroFunctionBase<TResult, TArgs...>* nitro_function)
        {
            auto on_lib_loaded = [this, nitro_function](nitro_utils::SysModule hModule) {
                auto* hook = hook_storage_->GetStaticHook<HookId, CallingConventions::Cdecl, TResult, TArgs...>(addr_provider_->FindAddress(hModule, HookId));
                nitro_function->SetHook(hook);
            };

            RegisterFunc(nitro_function, std::move(on_lib_loaded), std::string(magic_enum::enum_name(HookId)));
        }

        template<StaticHookId HookId, class TResult, class... TArgs>
        void RegisterFuncStdcall(NitroFunctionBase<TResult, TArgs...>* nitro_function)
        {
            auto on_lib_loaded = [this, nitro_function](nitro_utils::SysModule hModule) {
                auto* hook = hook_storage_->GetStaticHook<HookId, CallingConventions::Stdcall, TResult, TArgs...>(addr_provider_->FindAddress(hModule, HookId));
                nitro_function->SetHook(hook);
            };

            RegisterFunc(nitro_function, std::move(on_lib_loaded), std::string(magic_enum::enum_name(HookId)));
        }

        template<StaticHookId HookId, class TResult, class... TArgs>
        void RegisterFuncCdecl(NitroFunctionBase<TResult, TArgs...>* nitro_function, std::function<uint32_t(nitro_utils::SysModule)>&& pointer_getter)
        {
            RegisterFunc<HookId, CallingConventions::Cdecl>(nitro_function, std::move(pointer_getter));
        }

        template<StaticHookId HookId, class TResult, class... TArgs>
        void RegisterFuncStdcall(NitroFunctionBase<TResult, TArgs...>* nitro_function, std::function<uint32_t(nitro_utils::SysModule)>&& pointer_getter)
        {
            RegisterFunc<HookId, CallingConventions::Stdcall>(nitro_function, std::move(pointer_getter));
        }

        template<StaticHookId HookId, CallingConventions Convention, FunctionPointer TFunc, class TResult, class... TArgs>
        void RegisterFuncCdecl(NitroFunctionBase<TResult, TArgs...>* nitro_function, TFunc* pointer_getter)
        {
            auto on_lib_loaded = [this, pointer_getter, nitro_function](nitro_utils::SysModule hModule) {
                auto* hook = hook_storage_->GetStaticHook<HookId, Convention, TResult, TArgs...>((uint32_t)*pointer_getter);
                nitro_function->SetHook(hook);
            };

            RegisterFunc(nitro_function, std::move(on_lib_loaded), std::string(magic_enum::enum_name(HookId)));
        }

        template<StaticHookId HookId, CallingConventions Convention, class TResult, class... TArgs>
        void RegisterFunc(NitroFunctionBase<TResult, TArgs...>* nitro_function, std::function<uint32_t(nitro_utils::SysModule)>&& pointer_getter)
        {
            auto on_lib_loaded = [this, pointer_getter = std::move(pointer_getter), nitro_function](nitro_utils::SysModule hModule) {
                auto* hook = hook_storage_->GetStaticHook<HookId, Convention, TResult, TArgs...>((uint32_t)pointer_getter(hModule));
                nitro_function->SetHook(hook);
            };

            RegisterFunc(nitro_function, std::move(on_lib_loaded), std::string(magic_enum::enum_name(HookId)));
        }
// region end STATIC HOOKS

// region VAFORMAT HOOKS
        template<StaticHookId HookId, class TResult, class... TArgs>
        void RegisterFuncVaformat(NitroFunctionBase<TResult, TArgs...>* nitro_function)
        {
            auto on_lib_loaded = [this, nitro_function](nitro_utils::SysModule hModule) {
                auto* hook = hook_storage_->GetStaticVaformatHook<HookId, TResult, TArgs...>(addr_provider_->FindAddress(hModule, HookId));
                nitro_function->SetHook(hook);
            };

            RegisterFunc(nitro_function, std::move(on_lib_loaded), std::string(magic_enum::enum_name(HookId)));
        }
// region end VAFORMAT HOOKS

// region JIT HOOKS
        template<class TResult, class... TArgs>
        void RegisterFuncStdcall(NitroFunctionBase<TResult, TArgs...>* nitro_function, std::function<uint32_t(nitro_utils::SysModule)>&& pointer_getter)
        {
            RegisterFunc<CallingConventions::Stdcall>(nitro_function, std::move(pointer_getter), "[JIT] " + typeid(nitro_function).name());
        }

        template<CallingConventions Convention, class TResult, class... TArgs>
        void RegisterFunc(NitroFunctionBase<TResult, TArgs...>* nitro_function, std::function<uint32_t(nitro_utils::SysModule)>&& pointer_getter)
        {
            auto on_lib_loaded = [this, pointer_getter = std::move(pointer_getter), nitro_function](nitro_utils::SysModule hModule) {
                auto* hook = hook_storage_->GetJitHook<Convention, TResult, TArgs...>((uint32_t)pointer_getter(hModule));
                nitro_function->SetHook(hook);
            };

            RegisterFunc(nitro_function, std::move(on_lib_loaded), "[JIT] " + typeid(nitro_function).name());
        }
// region end JIT HOOKS

        // Common function
        template<class TResult, class... TArgs>
        void RegisterFunc(NitroFunctionBase<TResult, TArgs...>* nitro_function, std::function<void(nitro_utils::SysModule)>&& on_lib_loaded, const std::string& description)
        {
            auto* chain = new HandlersChain<TResult, TArgs...>(description);
            nitro_function->SetChain(chain);

            on_library_loaded_.emplace_back(std::move(on_lib_loaded));

            on_library_unloaded_.emplace_back([this, nitro_function] {
                auto* hook = nitro_function->GetHook();
                nitro_function->DetachHook();

                if (hook != nullptr)
                    hook_storage_->RemoveHook(hook->GetOriginalFunctionAddress());
            });
        }
    };
}
