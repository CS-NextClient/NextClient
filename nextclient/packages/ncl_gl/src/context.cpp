#include <ncl_gl/context.h>

#ifdef _WIN32
#include <Windows.h>
#else
#include <dlfcn.h>
#endif

#ifndef _WIN32
namespace
{
    // EGL procedure resolvers can return stubs for GLX queries.
    void* CurrentFromLibrary(const char* library_name, const char* function_name)
    {
        void* library = dlopen(library_name, RTLD_NOLOAD | RTLD_LAZY);
        if (library == nullptr)
        {
            return nullptr;
        }

        using current_context_fn_t = void* (*)();
        current_context_fn_t query = reinterpret_cast<current_context_fn_t>(dlsym(library, function_name));
        void* context = query != nullptr ? query() : nullptr;
        dlclose(library);
        return context;
    }
} // namespace
#endif

void* ncl_gl::CurrentContext()
{
#ifdef _WIN32
    return wglGetCurrentContext();
#else
    if (void* context = CurrentFromLibrary("libGLX.so.0", "glXGetCurrentContext"))
    {
        return context;
    }

    if (void* context = CurrentFromLibrary("libGL.so.1", "glXGetCurrentContext"))
    {
        return context;
    }

    return CurrentFromLibrary("libEGL.so.1", "eglGetCurrentContext");
#endif
}
