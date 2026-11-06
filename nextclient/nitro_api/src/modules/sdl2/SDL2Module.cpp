#include "SDL2Module.h"

namespace nitroapi
{
    SDL2Module::SDL2Module(std::shared_ptr<HookStorage> hook_storage,
                           std::shared_ptr<AddressProviderBase> addr_provider) :
        ModuleBase(std::move(hook_storage), std::move(addr_provider))
    {
        sdl2_data_ = std::make_shared<SDL2Data>();

        RegisterFuncCdecl<StaticHookId::SDL_DestroyWindow>(&sdl2_data_->SDL_DestroyWindowFunc);
        RegisterFuncCdecl<StaticHookId::SDL_CreateWindow>(&sdl2_data_->SDL_CreateWindowFunc);
        RegisterFuncCdecl<StaticHookId::SDL_GetError>(&sdl2_data_->SDL_GetErrorFunc);
        RegisterFuncCdecl<StaticHookId::SDL_GL_GetProcAddress>(&sdl2_data_->SDL_GL_GetProcAddress);
        RegisterFuncCdecl<StaticHookId::SDL_GL_SwapWindow>(&sdl2_data_->SDL_GL_SwapWindow);
        RegisterFuncCdecl<StaticHookId::SDL_GL_GetDrawableSize>(&sdl2_data_->SDL_GL_GetDrawableSize);
        RegisterFuncCdecl<StaticHookId::SDL_GL_GetCurrentWindow>(&sdl2_data_->SDL_GL_GetCurrentWindow);
        RegisterFuncCdecl<StaticHookId::SDL_GetWindowSize>(&sdl2_data_->SDL_GetWindowSize);
        RegisterFuncCdecl<StaticHookId::SDL_GetVersion>(&sdl2_data_->SDL_GetVersion);
        RegisterFuncCdecl<StaticHookId::SDL_GL_CreateContext>(&sdl2_data_->SDL_GL_CreateContext);
        RegisterFuncCdecl<StaticHookId::SDL_GL_DeleteContext>(&sdl2_data_->SDL_GL_DeleteContext);
        RegisterFuncCdecl<StaticHookId::SDL_GL_GetCurrentContext>(&sdl2_data_->SDL_GL_GetCurrentContext);
    }
}
