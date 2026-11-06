#include <easylogging++.h>
#include "SDL2AddressProvider.h"

namespace nitroapi
{
    const std::unordered_map<StaticHookId, SearchConfig> SDL2AddressProvider::addresses_hook_id_{
        {StaticHookId::SDL_DestroyWindow, SearchConfig("SDL_DestroyWindow", SearchType::ExportFunc)},
        {StaticHookId::SDL_CreateWindow, SearchConfig("SDL_CreateWindow", SearchType::ExportFunc)},
        {StaticHookId::SDL_GetError, SearchConfig("SDL_GetError", SearchType::ExportFunc)},
        {StaticHookId::SDL_GL_GetProcAddress, SearchConfig("SDL_GL_GetProcAddress", SearchType::ExportFunc)},
        {StaticHookId::SDL_GL_SwapWindow, SearchConfig("SDL_GL_SwapWindow", SearchType::ExportFunc)},
        {StaticHookId::SDL_GL_GetDrawableSize, SearchConfig("SDL_GL_GetDrawableSize", SearchType::ExportFunc)},
        {StaticHookId::SDL_GL_GetCurrentWindow, SearchConfig("SDL_GL_GetCurrentWindow", SearchType::ExportFunc)},
        {StaticHookId::SDL_GetWindowSize, SearchConfig("SDL_GetWindowSize", SearchType::ExportFunc)},
        {StaticHookId::SDL_GetVersion, SearchConfig("SDL_GetVersion", SearchType::ExportFunc)},
        {StaticHookId::SDL_GL_CreateContext, SearchConfig("SDL_GL_CreateContext", SearchType::ExportFunc)},
        {StaticHookId::SDL_GL_DeleteContext, SearchConfig("SDL_GL_DeleteContext", SearchType::ExportFunc)},
        {StaticHookId::SDL_GL_GetCurrentContext, SearchConfig("SDL_GL_GetCurrentContext", SearchType::ExportFunc)},
    };

    const std::unordered_map<std::string, SearchConfig> SDL2AddressProvider::addresses_str_{};
}
