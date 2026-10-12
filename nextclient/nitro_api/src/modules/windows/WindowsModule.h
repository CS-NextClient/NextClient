#pragma once

#include <nitroapi/NitroApiInterface.h>
#include <nitroapi/modules/windows/WindowsData.h>
#include "../../HookStorage.h"
#include "../ModuleBase.h"

namespace nitroapi
{
    class WindowsModule : public ModuleBase
    {
        std::shared_ptr<WindowsData> windows_data_;

    public:
        explicit WindowsModule(std::shared_ptr<HookStorage> hook_storage,
                               std::shared_ptr<AddressProviderBase> addr_provider);

        [[nodiscard]] inline std::shared_ptr<WindowsData> GetWindowsData() { return windows_data_; }
    };
}