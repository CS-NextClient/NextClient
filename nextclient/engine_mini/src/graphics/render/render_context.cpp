#include "graphics/render/render_context.h"

#include <ncl_gl/context.h>
#include <nitroapi/NitroApiInterface.h>

#include "engine.h"

namespace render
{
    void* context::Current()
    {
#ifndef _WIN32
        if (g_NitroApi != nullptr)
        {
            nitroapi::SDL2Data* sdl = g_NitroApi->GetSDL2Data();

            if (sdl != nullptr && sdl->SDL_GL_GetCurrentContext.GetHook() &&
                sdl->SDL_GL_GetCurrentContext.GetHook()->GetOriginalFunctionAddress())
            {
                if (void* context = sdl->SDL_GL_GetCurrentContext())
                {
                    return context;
                }
            }
        }

#endif
        return ncl_gl::CurrentContext();
    }
} // namespace render
