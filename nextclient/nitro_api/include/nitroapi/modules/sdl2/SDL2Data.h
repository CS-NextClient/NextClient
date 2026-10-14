#pragma once

#include "../../chain/HandlersChainInterface.h"
#include "../../NitroFunction.h"

namespace nitroapi
{
    struct SDL2Data
    {
        // void SDL_DestroyWindow(SDL_Window* window);
        NitroFunctionVoid<void*>                                         SDL_DestroyWindowFunc;

        // SDL_Window* SDL_CreateWindow(const char *title, int x, int y, int w, int h, Uint32 flags);
        NitroFunction<void*, const char*, int, int, int, int, uint32_t>  SDL_CreateWindowFunc;

        // const char* SDL_GetError()
        NitroFunction<const char*>                                       SDL_GetErrorFunc;
        // void* SDL_GL_GetProcAddress(const char* proc);
        NitroFunction<void*, const char*>                                SDL_GL_GetProcAddress;
    };
}