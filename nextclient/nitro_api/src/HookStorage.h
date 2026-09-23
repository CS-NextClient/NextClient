#pragma once

#include "hook/GenericHookJit.h"
#include "hook/GenericHookStatic.h"
#include "hook/GenericHookVaformatStatic.h"
#include <easylogging++.h>
#include <unordered_map>

namespace nitroapi
{
    class HookStorage
    {
        std::unordered_map<uint32_t, HookInterface*> hooks_;

    public:
        ~HookStorage()
        {
            int i = 0;
            for (auto& hook : hooks_)
            {
                i++;
                delete hook.second;
            }

            LOG_IF(i > 0, WARNING) << "HookStorage::~HookStorage Auto-removed hooks count: " << i;
        }

        template<StaticHookId HookId, CallingConventions TConv, class TResult, class... TArgs>
        GenericHookInterface<TResult, TArgs...>* GetStaticHook(uint32_t address)
        {
            auto result = hooks_.find(address);
            if (result != hooks_.end())
                return dynamic_cast<GenericHookInterface<TResult, TArgs...>*>(result->second);

            auto hook = new GenericHookStatic<HookId, TConv, TResult, TArgs...>(address);
            auto empl_result = hooks_.try_emplace(address, hook);

            if (!empl_result.second)
                return nullptr;

            return hook;
        }

        template<StaticHookId HookId, class TResult, class... TArgs>
        GenericHookInterface<TResult, TArgs...>* GetStaticVaformatHook(uint32_t address)
        {
            auto result = hooks_.find(address);
            if (result != hooks_.end())
                return dynamic_cast<GenericHookInterface<TResult, TArgs...>*>(result->second);

            auto hook = new GenericHookVaformatStatic<HookId, TResult, TArgs...>(address);
            auto empl_result = hooks_.try_emplace(address, hook);

            if (!empl_result.second)
                return nullptr;

            return hook;
        }

        template<CallingConventions TConv, class TResult, class... TArgs>
        GenericHookInterface<TResult, TArgs...>* GetJitHook(uint32_t address)
        {
            auto result = hooks_.find(address);
            if (result != hooks_.end())
                return dynamic_cast<GenericHookInterface<TResult, TArgs...>*>(result->second);

            auto hook = new GenericHookJit<TConv, TResult, TArgs...>(address);
            auto empl_result = hooks_.try_emplace(address, hook);

            if (!empl_result.second)
                return nullptr;

            return hook;
        }

        void RemoveHook(uint32_t address)
        {
            auto result = hooks_.find(address);
            if (result == hooks_.end())
                return;

            delete result->second;

            hooks_.erase(result);
        }
    };
}