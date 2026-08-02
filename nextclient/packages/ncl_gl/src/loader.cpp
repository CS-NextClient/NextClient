#include <ncl_gl/loader.h>

#include <windows.h>
#include <glad/glad.h>

void* ncl_gl::LoadProc(const char* name)
{
    void* p = reinterpret_cast<void*>(wglGetProcAddress(name));

    if (p == nullptr || p == reinterpret_cast<void*>(1) || p == reinterpret_cast<void*>(2) || p == reinterpret_cast<void*>(3) ||
        p == reinterpret_cast<void*>(-1))
    {
        static HMODULE gl = LoadLibraryA("opengl32.dll");
        p = reinterpret_cast<void*>(GetProcAddress(gl, name));
    }

    return p;
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
