#ifdef _WIN32
#define PERF_TEST_EXPORT __declspec(dllexport)
#else
#define PERF_TEST_EXPORT __attribute__((visibility("default")))
#endif

extern "C" void SDL_GL_SwapWindow(void*);
extern "C" PERF_TEST_EXPORT int PerfTest_DependentSwap()
{
    SDL_GL_SwapWindow(nullptr);
    return 1;
}
