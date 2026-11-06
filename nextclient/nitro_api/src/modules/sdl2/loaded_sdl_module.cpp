#include "modules/sdl2/loaded_sdl_module.h"

#ifdef _WIN32
#include <Windows.h>
#else
#include <dlfcn.h>
#endif

namespace nitroapi::sdl2
{
    nitro_utils::SysModule FindLoadedModule(nitro_utils::SysModule engine_module)
    {
        if (!engine_module)
        {
            return nullptr;
        }

#ifdef _WIN32
        return GetModuleHandleA("sdl2.dll");
#else
        void* swap_address = dlsym(engine_module, "SDL_GL_SwapWindow");
        Dl_info information{};
        if (!swap_address || !dladdr(swap_address, &information))
        {
            return nullptr;
        }

        void* sdl_module = dlopen(information.dli_fname, RTLD_NOW | RTLD_NOLOAD);
        if (sdl_module)
        {
            // The engine's dependency retains this object; balance the discovery reference.
            dlclose(sdl_module);
        }

        return sdl_module;
#endif
    }
} // namespace nitroapi::sdl2
