#pragma once

namespace ncl_gl
{
    // Uses SDL or an already loaded EGL resolver on Linux and an opengl32.dll fallback
    // for core entry points on Windows after the driver resolver.
    void* LoadProc(const char* name);

    // Requires a current GL context on the calling thread.
    bool EnsureLoaded();

    bool IsLoaded();

    // Requires a current GL context and resolves its entry points again.
    bool Reload();
}
