#include <ncl_gl/loader.h>

#include <cstddef>

#if defined(_WIN32)
#include <windows.h>
#else
#include <dlfcn.h>
#include <SDL_video.h>
#endif
#include <glad/glad.h>

void* ncl_gl::LoadProc(const char* name)
{
#if defined(_WIN32)
    void* p = reinterpret_cast<void*>(wglGetProcAddress(name));

    if (p == nullptr || p == reinterpret_cast<void*>(1) || p == reinterpret_cast<void*>(2) || p == reinterpret_cast<void*>(3) ||
        p == reinterpret_cast<void*>(-1))
    {
        static HMODULE gl = LoadLibraryA("opengl32.dll");
        p = reinterpret_cast<void*>(GetProcAddress(gl, name));
    }

    return p;
#else
    void* address = SDL_GL_GetProcAddress(name);
    if (address != nullptr)
    {
        return address;
    }

    void* library = dlopen("libEGL.so.1", RTLD_NOLOAD | RTLD_LAZY);
    if (library == nullptr)
    {
        return nullptr;
    }

    using gl_proc_t = void (*)();
    using get_proc_address_fn_t = gl_proc_t (*)(const char*);
    get_proc_address_fn_t get_proc_address = reinterpret_cast<get_proc_address_fn_t>(dlsym(library, "eglGetProcAddress"));
    address = get_proc_address != nullptr ? reinterpret_cast<void*>(get_proc_address(name)) : nullptr;
    dlclose(library);
    return address;
#endif
}

namespace
{
    bool g_LoadDone;
    bool g_LoadOk;
}

bool ncl_gl::EnsureLoaded()
{
    if (g_LoadDone)
    {
        return g_LoadOk;
    }

    return Reload();
}

bool ncl_gl::Reload()
{
    g_LoadDone = true;
    g_LoadOk = gladLoadGLLoader(reinterpret_cast<GLADloadproc>(LoadProc)) != 0;

    return g_LoadOk;
}

bool ncl_gl::IsLoaded()
{
    return g_LoadDone && g_LoadOk;
}
