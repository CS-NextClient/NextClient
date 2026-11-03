#pragma once

#include <memory>
#include <vector>

#include <nitroapi/modules/client/ClientData.h>
#include <nitroapi/modules/client/StudioAnimationData.h>
#include <nitroapi/modules/engine/EngineData.h>

#include "LibraryHookListener.h"
#include "chain/HandlersChain.h"
#include "modules/ModuleBase.h"

namespace nitroapi
{
    class ClientModule : public ModuleBase
    {
        std::shared_ptr<ClientData> client_data_;
        std::vector<std::shared_ptr<nitroapi::Unsubscriber>> unsubscribers_;
        StudioAnimationData studio_animation_;

    public:
        explicit ClientModule(std::shared_ptr<HookStorage> hook_storage, std::shared_ptr<AddressProviderBase> addr_provider);

        ~ClientModule() override;

        [[nodiscard]] inline std::shared_ptr<ClientData> GetClientData()
        {
            return client_data_;
        }

        void OnLibraryLoaded(nitro_utils::SysModule hModule) override;
        void OnLibraryUnload() override;

    private:
        template <StaticHookId HookId, class TResult, class... TArgs>
        void RegisterStudioMethod(NitroFunctionBase<TResult, TArgs...>* method)
        {
#ifdef _WIN32
            RegisterFuncThiscall<HookId>(method);
#else
            RegisterFuncCdecl<HookId>(method);
#endif
        }
    };
} // namespace nitroapi
