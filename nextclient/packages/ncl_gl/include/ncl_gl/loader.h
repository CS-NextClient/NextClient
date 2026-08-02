#pragma once

namespace ncl_gl
{
    // Resolves a GL entry point via wglGetProcAddress and falls back to
    // opengl32.dll for the GL 1.1 functions that wglGetProcAddress does not
    // return on some drivers. Returned as a raw pointer so this header does
    // not depend on glad; callers cast it to GLADloadproc.
    void* LoadProc(const char* name);

    // One-shot lazy glad initialization over LoadProc. Returns whether glad
    // loaded; subsequent calls return the cached result. Call only with a
    // current GL context on the calling thread.
    bool EnsureLoaded();

    // Whether EnsureLoaded already ran and succeeded. Never triggers loading.
    bool IsLoaded();
}
