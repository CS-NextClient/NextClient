#include "graphics/render/render_context.h"

#include <ncl_gl/loader.h>
#include <nitroapi/NitroApiInterface.h>

#ifdef _WIN32
#include <Windows.h>
#else
#include <dlfcn.h>
#endif

#include "engine.h"

namespace render
{
    void* context::Current()
    {
#ifdef _WIN32
        return wglGetCurrentContext();
#else
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

        using current_context_fn_t = void* (*)();
        current_context_fn_t glx = reinterpret_cast<current_context_fn_t>(ncl_gl::LoadProc("glXGetCurrentContext"));

        if (glx != nullptr)
        {
            if (void* context = glx())
            {
                return context;
            }
        }

        // Surfaceless owners may have loaded EGL locally without an SDL or GLX context.
        void* library = dlopen("libEGL.so.1", RTLD_NOLOAD | RTLD_LAZY);

        if (library == nullptr)
        {
            return nullptr;
        }

        current_context_fn_t egl = reinterpret_cast<current_context_fn_t>(dlsym(library, "eglGetCurrentContext"));
        void* context = egl != nullptr ? egl() : nullptr;
        dlclose(library);
        return context;
#endif
    }
} // namespace render
