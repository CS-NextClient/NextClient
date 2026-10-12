#pragma once

#include <nitroapi/NitroApiInterface.h>
#include <nitroapi/modules/sdl2/SDL2Data.h>
#include "../../HookStorage.h"
#include "../ModuleBase.h"

namespace nitroapi
{
    class SDL2Module : public ModuleBase
    {
        std::shared_ptr<SDL2Data> sdl2_data_;

    public:
        explicit SDL2Module(std::shared_ptr<HookStorage> hook_storage,
                            std::shared_ptr<AddressProviderBase> addr_provider);

        [[nodiscard]] inline std::shared_ptr<SDL2Data> GetSDL2Data() { return sdl2_data_; }
    };
}