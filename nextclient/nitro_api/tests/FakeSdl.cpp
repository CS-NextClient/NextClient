#ifdef _WIN32
#define PERF_TEST_EXPORT __declspec(dllexport)
#else
#define PERF_TEST_EXPORT __attribute__((visibility("default")))
#endif

namespace
{
    void* volatile g_LastSwapWindow{};
    void* volatile g_DeletedContext{};
} // namespace

extern "C" PERF_TEST_EXPORT void SDL_GL_SwapWindow(void* window)
{
    g_LastSwapWindow = window;
}
extern "C" PERF_TEST_EXPORT void* SDL_GL_CreateContext(void* window)
{
    return window;
}
extern "C" PERF_TEST_EXPORT void SDL_GL_DeleteContext(void* context)
{
    g_DeletedContext = context;
}

extern "C" PERF_TEST_EXPORT void* PerfTest_DeletedContext()
{
    return g_DeletedContext;
}

#ifndef PERF_TEST_LEGACY_SDL
extern "C" PERF_TEST_EXPORT void* SDL_GL_GetCurrentContext()
{
    return nullptr;
}
extern "C" PERF_TEST_EXPORT void* SDL_GL_GetCurrentWindow()
{
    return nullptr;
}

extern "C" PERF_TEST_EXPORT void SDL_GL_GetDrawableSize(void*, int* width, int* height)
{
    *width = 640;
    *height = 480;
}
#endif

extern "C" PERF_TEST_EXPORT void SDL_GetWindowSize(void*, int* width, int* height)
{
    *width = 640;
    *height = 480;
}

extern "C" PERF_TEST_EXPORT void SDL_GetVersion(void* version)
{
    unsigned char* bytes = static_cast<unsigned char*>(version);
    bytes[0] = 2;
    bytes[1] = 0;
    bytes[2] = 0;
}
