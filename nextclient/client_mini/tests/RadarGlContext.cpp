#include "RadarGlContext.h"

#include <cstdint>
#include <unordered_map>

#ifdef _WIN32
#include <Windows.h>
#elif defined(__linux__)
#include <dlfcn.h>
#endif

#include <glad/glad.h>

namespace
{
#ifdef _WIN32
    constexpr wchar_t kWindowClassName[] = L"NextClientRadarShaderTest";

    void* LoadGl(const char* name)
    {
        PROC address = wglGetProcAddress(name);
        std::intptr_t value = reinterpret_cast<std::intptr_t>(address);
        if (!address || value == 1 || value == 2 || value == 3 || value == -1)
        {
            address = GetProcAddress(GetModuleHandleW(L"opengl32.dll"), name);
        }
        return reinterpret_cast<void*>(address);
    }

    bool WindowsError(std::string& error, const char* operation)
    {
        error = std::string(operation) + " failed (Windows error " + std::to_string(GetLastError()) + ")";
        return false;
    }
#elif defined(__linux__)
    using egl_boolean_t = unsigned int;
    using egl_enum_t = unsigned int;
    using egl_display_t = void*;
    using egl_config_t = void*;
    using egl_surface_t = void*;
    using egl_context_t = void*;
    using egl_proc_t = void (*)();
    using egl_get_proc_address_t = egl_proc_t (*)(const char*);
    using egl_get_platform_display_t = egl_display_t (*)(egl_enum_t, void*, const int*);
    using egl_initialize_t = egl_boolean_t (*)(egl_display_t, int*, int*);
    using egl_bind_api_t = egl_boolean_t (*)(egl_enum_t);
    using egl_choose_config_t = egl_boolean_t (*)(egl_display_t, const int*, egl_config_t*, int, int*);
    using egl_create_surface_t = egl_surface_t (*)(egl_display_t, egl_config_t, const int*);
    using egl_create_context_t = egl_context_t (*)(egl_display_t, egl_config_t, egl_context_t, const int*);
    using egl_make_current_t = egl_boolean_t (*)(egl_display_t, egl_surface_t, egl_surface_t, egl_context_t);
    using egl_destroy_context_t = egl_boolean_t (*)(egl_display_t, egl_context_t);
    using egl_destroy_surface_t = egl_boolean_t (*)(egl_display_t, egl_surface_t);
    using egl_terminate_t = egl_boolean_t (*)(egl_display_t);
    using egl_get_error_t = int (*)();
    using egl_current_context_t = egl_context_t (*)();
    using egl_current_display_t = egl_display_t (*)();
    using egl_current_surface_t = egl_surface_t (*)(int);
    std::unordered_map<egl_display_t, unsigned int> g_EglDisplayOwners;

    constexpr egl_enum_t kEglPlatformSurfaceless = 0x31DD;
    constexpr egl_enum_t kEglOpenGlApi = 0x30A2;
    constexpr int kEglNone = 0x3038;
    constexpr int kEglDraw = 0x3059;
    constexpr int kEglRead = 0x305A;
    constexpr int kEglSurfaceType = 0x3033;
    constexpr int kEglPbufferBit = 0x0001;
    constexpr int kEglRenderableType = 0x3040;
    constexpr int kEglOpenGlBit = 0x0008;
    constexpr int kEglRedSize = 0x3024;
    constexpr int kEglGreenSize = 0x3023;
    constexpr int kEglBlueSize = 0x3022;
    constexpr int kEglAlphaSize = 0x3021;
    constexpr int kEglWidth = 0x3057;
    constexpr int kEglHeight = 0x3056;

    // GLAD's loader has no user data; this callback state exists only during its synchronous load.
    egl_get_proc_address_t g_EglGetProcAddress{};

    void* LoadGl(const char* name)
    {
        return reinterpret_cast<void*>(g_EglGetProcAddress(name));
    }

    template <typename Function>
    bool LoadEglFunction(void* library, const char* name, Function& function, std::string& error)
    {
        function = reinterpret_cast<Function>(dlsym(library, name));
        if (!function)
        {
            error = std::string("libEGL.so.1 does not export ") + name;
            return false;
        }
        return true;
    }
#endif
} // namespace

struct RadarGlContext::Implementation
{
#ifdef _WIN32
    HINSTANCE instance{};
    ATOM window_class{};
    HWND window{};
    HDC device_context{};
    HGLRC context{};
    HDC previous_device_context{};
    HGLRC previous_context{};
    bool current{};
#elif defined(__linux__)
    void* library{};
    egl_display_t display{};
    egl_surface_t surface{};
    egl_context_t context{};
    egl_get_proc_address_t get_proc_address{};
    egl_make_current_t make_current{};
    egl_destroy_context_t destroy_context{};
    egl_destroy_surface_t destroy_surface{};
    egl_terminate_t terminate{};
    egl_current_context_t get_current_context{};
    egl_display_t previous_display{};
    egl_surface_t previous_draw{};
    egl_surface_t previous_read{};
    egl_context_t previous_context{};
    bool initialized{};
    bool current{};
#endif

    ~Implementation()
    {
#ifdef _WIN32
        if (current && wglGetCurrentContext() == context)
        {
            wglMakeCurrent(previous_device_context, previous_context);
        }
        if (context)
        {
            wglDeleteContext(context);
        }
        if (device_context)
        {
            ReleaseDC(window, device_context);
        }
        if (window)
        {
            DestroyWindow(window);
        }
        if (window_class)
        {
            UnregisterClassW(kWindowClassName, instance);
        }
#elif defined(__linux__)
        if (current && get_current_context() == context)
        {
            make_current(previous_display != nullptr ? previous_display : display, previous_draw, previous_read, previous_context);
        }
        if (context)
        {
            destroy_context(display, context);
        }
        if (surface)
        {
            destroy_surface(display, surface);
        }
        if (initialized)
        {
            auto owner = g_EglDisplayOwners.find(display);

            if (--owner->second == 0)
            {
                terminate(display);
                g_EglDisplayOwners.erase(owner);
            }
        }
        if (library)
        {
            dlclose(library);
        }
#endif
    }

    bool Initialize(std::string& error, bool double_buffer, int samples, int width, int height)
    {
#ifdef _WIN32
        instance = GetModuleHandleW(nullptr);
        WNDCLASSW definition{};
        definition.style = CS_OWNDC;
        definition.lpfnWndProc = DefWindowProcW;
        definition.hInstance = instance;
        definition.lpszClassName = kWindowClassName;
        window_class = RegisterClassW(&definition);
        if (!window_class && GetLastError() != ERROR_CLASS_ALREADY_EXISTS)
        {
            return WindowsError(error, "RegisterClassW");
        }
        window = CreateWindowExW(0, kWindowClassName, L"", WS_POPUP, 0, 0, width, height, nullptr, nullptr, instance, nullptr);
        if (!window)
        {
            return WindowsError(error, "CreateWindowExW");
        }
        device_context = GetDC(window);
        if (!device_context)
        {
            return WindowsError(error, "GetDC");
        }
        PIXELFORMATDESCRIPTOR format{};
        format.nSize = sizeof(format);
        format.nVersion = 1;
        format.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL;
        if (double_buffer)
        {
            format.dwFlags |= PFD_DOUBLEBUFFER;
        }
        format.iPixelType = PFD_TYPE_RGBA;
        format.cColorBits = 24;
        format.cAlphaBits = 8;
        int pixel_format = ChoosePixelFormat(device_context, &format);
        if (!pixel_format)
        {
            return WindowsError(error, "ChoosePixelFormat");
        }
        if (!SetPixelFormat(device_context, pixel_format, &format))
        {
            return WindowsError(error, "SetPixelFormat");
        }
        context = wglCreateContext(device_context);
        if (!context)
        {
            return WindowsError(error, "wglCreateContext");
        }
        previous_context = wglGetCurrentContext();
        previous_device_context = wglGetCurrentDC();
        if (!wglMakeCurrent(device_context, context))
        {
            return WindowsError(error, "wglMakeCurrent");
        }
        current = true;
        if (samples > 0)
        {
            using ChoosePixelFormat_t = BOOL(WINAPI*)(HDC, const int*, const FLOAT*, UINT, int*, UINT*);
            ChoosePixelFormat_t choose = reinterpret_cast<ChoosePixelFormat_t>(wglGetProcAddress("wglChoosePixelFormatARB"));
            if (choose == nullptr)
            {
                error = "WGL_ARB_pixel_format is required for multisampled window tests";
                return false;
            }
            const int attributes[] = {
                0x2001,
                TRUE, // WGL_DRAW_TO_WINDOW_ARB
                0x2010,
                TRUE, // WGL_SUPPORT_OPENGL_ARB
                0x2011,
                double_buffer ? TRUE : FALSE, // WGL_DOUBLE_BUFFER_ARB
                0x2013,
                0x202B, // WGL_PIXEL_TYPE_ARB, WGL_TYPE_RGBA_ARB
                0x2014,
                24, // WGL_COLOR_BITS_ARB
                0x201B,
                8, // WGL_ALPHA_BITS_ARB
                0x2022,
                24, // WGL_DEPTH_BITS_ARB
                0x2041,
                TRUE, // WGL_SAMPLE_BUFFERS_ARB
                0x2042,
                samples, // WGL_SAMPLES_ARB
                0
            };
            UINT count = 0;
            if (!choose(device_context, attributes, nullptr, 1, &pixel_format, &count) || count == 0)
            {
                error = "WGL has no matching multisampled window pixel format";
                return false;
            }
            HWND multisample_window = CreateWindowExW(0, kWindowClassName, L"", WS_POPUP, 0, 0, width, height, nullptr, nullptr, instance, nullptr);
            HDC multisample_dc = multisample_window ? GetDC(multisample_window) : nullptr;
            HGLRC multisample_context = nullptr;
            if (multisample_dc != nullptr && DescribePixelFormat(multisample_dc, pixel_format, sizeof(format), &format) != 0 &&
                SetPixelFormat(multisample_dc, pixel_format, &format))
            {
                multisample_context = wglCreateContext(multisample_dc);
            }
            if (multisample_context == nullptr || !wglMakeCurrent(multisample_dc, multisample_context))
            {
                WindowsError(error, "Create multisampled WGL context");
                if (multisample_context != nullptr)
                {
                    wglDeleteContext(multisample_context);
                }
                if (multisample_dc != nullptr)
                {
                    ReleaseDC(multisample_window, multisample_dc);
                }
                if (multisample_window != nullptr)
                {
                    DestroyWindow(multisample_window);
                }
                return false;
            }
            wglDeleteContext(context);
            ReleaseDC(window, device_context);
            DestroyWindow(window);
            context = multisample_context;
            device_context = multisample_dc;
            window = multisample_window;
        }
#elif defined(__linux__)
        library = dlopen("libEGL.so.1", RTLD_NOW | RTLD_LOCAL);
        if (!library)
        {
            const char* detail = dlerror();
            error = std::string("Cannot load libEGL.so.1: ") + (detail ? detail : "unknown loader error");
            return false;
        }
        egl_initialize_t initialize{};
        egl_bind_api_t bind_api{};
        egl_choose_config_t choose_config{};
        egl_create_surface_t create_surface{};
        egl_create_context_t create_context{};
        egl_get_error_t get_error{};
        egl_current_display_t get_current_display{};
        egl_current_surface_t get_current_surface{};
        if (!LoadEglFunction(library, "eglGetProcAddress", get_proc_address, error) ||
            !LoadEglFunction(library, "eglInitialize", initialize, error) || !LoadEglFunction(library, "eglBindAPI", bind_api, error) ||
            !LoadEglFunction(library, "eglChooseConfig", choose_config, error) ||
            !LoadEglFunction(library, "eglCreatePbufferSurface", create_surface, error) ||
            !LoadEglFunction(library, "eglCreateContext", create_context, error) ||
            !LoadEglFunction(library, "eglMakeCurrent", make_current, error) ||
            !LoadEglFunction(library, "eglDestroyContext", destroy_context, error) ||
            !LoadEglFunction(library, "eglDestroySurface", destroy_surface, error) ||
            !LoadEglFunction(library, "eglTerminate", terminate, error) || !LoadEglFunction(library, "eglGetError", get_error, error) ||
            !LoadEglFunction(library, "eglGetCurrentContext", get_current_context, error) ||
            !LoadEglFunction(library, "eglGetCurrentDisplay", get_current_display, error) ||
            !LoadEglFunction(library, "eglGetCurrentSurface", get_current_surface, error))
        {
            return false;
        }
        egl_get_platform_display_t get_display = reinterpret_cast<egl_get_platform_display_t>(get_proc_address("eglGetPlatformDisplayEXT"));
        if (!get_display)
        {
            error = "EGL does not provide eglGetPlatformDisplayEXT for the surfaceless display";
            return false;
        }
        const auto egl_error = [&](const char* operation) {
            error = std::string(operation) + " failed (EGL error " + std::to_string(get_error()) + ")";
            return false;
        };
        display = get_display(kEglPlatformSurfaceless, nullptr, nullptr);
        if (!display)
        {
            return egl_error("eglGetPlatformDisplayEXT(EGL_PLATFORM_SURFACELESS_MESA)");
        }
        int major = 0;
        int minor = 0;
        if (!initialize(display, &major, &minor))
        {
            return egl_error("eglInitialize");
        }
        initialized = true;
        ++g_EglDisplayOwners[display];
        previous_context = get_current_context();
        previous_display = get_current_display();
        previous_draw = get_current_surface(kEglDraw);
        previous_read = get_current_surface(kEglRead);
        if (!bind_api(kEglOpenGlApi))
        {
            return egl_error("eglBindAPI(EGL_OPENGL_API)");
        }
        const int config_attributes[] = {
            kEglSurfaceType,
            kEglPbufferBit,
            kEglRenderableType,
            kEglOpenGlBit,
            kEglRedSize,
            8,
            kEglGreenSize,
            8,
            kEglBlueSize,
            8,
            kEglAlphaSize,
            8,
            kEglNone
        };
        egl_config_t config = nullptr;
        int count = 0;
        if (!choose_config(display, config_attributes, &config, 1, &count))
        {
            return egl_error("eglChooseConfig");
        }
        if (!count)
        {
            error = "EGL has no RGBA8 pbuffer configuration for desktop OpenGL";
            return false;
        }
        const int surface_attributes[] = {kEglWidth, width, kEglHeight, height, kEglNone};
        surface = create_surface(display, config, surface_attributes);
        if (!surface)
        {
            return egl_error("eglCreatePbufferSurface");
        }
        const int context_attributes[] = {kEglNone};
        context = create_context(display, config, nullptr, context_attributes);
        if (!context)
        {
            return egl_error("eglCreateContext");
        }
        if (!make_current(display, surface, surface, context))
        {
            return egl_error("eglMakeCurrent");
        }
        current = true;
        g_EglGetProcAddress = get_proc_address;
#else
        error = "Radar shader tests support Windows WGL and Linux surfaceless EGL";
        return false;
#endif
#if defined(_WIN32) || defined(__linux__)
        int loaded = gladLoadGLLoader(LoadGl);
#ifdef __linux__
        g_EglGetProcAddress = nullptr;
#endif
        if (!loaded)
        {
            error = "GLAD cannot load desktop OpenGL functions for the offscreen context";
            return false;
        }
        if (!GLAD_GL_VERSION_3_0)
        {
            const GLubyte* version = glGetString(GL_VERSION);
            error = std::string("Radar shader tests require desktop OpenGL 3.0; available version: ") +
                    (version ? reinterpret_cast<const char*>(version) : "unknown");
            return false;
        }
        return true;
#endif
    }
};

RadarGlContext::RadarGlContext() = default;

RadarGlContext::~RadarGlContext() = default;

bool RadarGlContext::Initialize(std::string& error, bool double_buffer, int samples, int width, int height)
{
    implementation_.reset();
    error.clear();
    implementation_ = std::make_unique<Implementation>();
    if (!implementation_->Initialize(error, double_buffer, samples, width, height))
    {
        implementation_.reset();
        return false;
    }
    return true;
}

bool RadarGlContext::MakeCurrent()
{
#ifdef _WIN32
    return wglMakeCurrent(implementation_->device_context, implementation_->context) != 0;
#elif defined(__linux__)
    return implementation_->make_current(
               implementation_->display, implementation_->surface, implementation_->surface, implementation_->context
           ) != 0;
#endif
}

bool RadarGlContext::ClearCurrent()
{
#ifdef _WIN32
    return wglMakeCurrent(nullptr, nullptr) != 0;
#elif defined(__linux__)
    return implementation_->make_current(implementation_->display, nullptr, nullptr, nullptr) != 0;
#endif
}

void* RadarGlContext::native_context() const
{
    return reinterpret_cast<void*>(implementation_->context);
}

void* RadarGlContext::Current()
{
#ifdef _WIN32
    return wglGetCurrentContext();
#elif defined(__linux__)
    void* library = dlopen("libEGL.so.1", RTLD_NOLOAD | RTLD_LAZY);
    if (library == nullptr)
    {
        return nullptr;
    }
    egl_current_context_t query = reinterpret_cast<egl_current_context_t>(dlsym(library, "eglGetCurrentContext"));
    void* context = query != nullptr ? query() : nullptr;
    dlclose(library);
    return context;
#endif
}
