#include <easylogging++.h>
#include "SDL2AddressProvider.h"

namespace nitroapi
{
    const std::unordered_map<StaticHookId, SearchConfig> SDL2AddressProvider::addresses_hook_id_
    {
        { StaticHookId::SDL_DestroyWindow,      SearchConfig("SDL_DestroyWindow", SearchType::ExportFunc) },
        { StaticHookId::SDL_CreateWindow,       SearchConfig("SDL_CreateWindow", SearchType::ExportFunc) },
        { StaticHookId::SDL_GetError,           SearchConfig("SDL_GetError", SearchType::ExportFunc) },
        { StaticHookId::SDL_GL_GetProcAddress,      SearchConfig("SDL_GL_GetProcAddress", SearchType::ExportFunc) },
    };

    const std::unordered_map<std::string, SearchConfig> SDL2AddressProvider::addresses_str_{{}};
}