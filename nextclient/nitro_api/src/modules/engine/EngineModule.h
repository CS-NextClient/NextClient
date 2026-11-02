#pragma once

#include <nitroapi/modules/engine/EngineData.h>
#include <icommandline.h>
#include <iregistry.h>
#include "../../LibraryHookListener.h"
#include "../../chain/HandlersChain.h"
#include "../../AddressProviderBase.h"
#include "../ModuleBase.h"
#include <nitroapi/NitroApiInterface.h>
#include "modules/engine/HostAbortAdapter.h"

namespace nitroapi
{
    class EngineModule : public ModuleBase
    {
        std::shared_ptr<EngineData> engine_data_;
        NitroApiInterface* nitro_api_;
        ICommandLine* command_line_;
        IFileSystem* file_system_;
        IRegistry* registry_;
        HostAbortAdapter abort_adapter_;

    public:
        explicit EngineModule(
            std::shared_ptr<HookStorage> hook_storage,
            std::shared_ptr<AddressProviderBase> addr_provider,
            NitroApiInterface* nitro_api,
            ICommandLine* command_line,
            IFileSystem* file_system,
            IRegistry* registry,
            FrameLifecycleService& frame_lifecycle
        );

        void OnLibraryLoaded(nitro_utils::SysModule module) override;
        void OnLibraryUnload() override;

        [[nodiscard]] inline std::shared_ptr<EngineData> GetEngineData() { return engine_data_; }
    };
}
