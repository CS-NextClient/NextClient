#pragma once

namespace ncl_gl
{
    // Falls back to opengl32.dll for the GL 1.1 entry points some drivers do not
    // return through wglGetProcAddress. void* keeps glad out of this header.
    void* LoadProc(const char* name);

    // Requires a current GL context on the calling thread.
    bool EnsureLoaded();

    bool IsLoaded();
}
