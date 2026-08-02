#pragma once

#include <cstdint>

namespace render
{
    // The shader pipeline targets a GL 3.3 compatibility context. Stages gate on this
    // rather than on the presence of individual entry points, so a partially exposed
    // driver cannot half-enable a stage.
    inline constexpr int kMinVersionMajor = 3;
    inline constexpr int kMinVersionMinor = 3;

    class IDeviceResource
    {
    public:
        virtual ~IDeviceResource() = default;

        // The context that owns the handles is gone. Drop them WITHOUT calling any
        // glDelete*: the names are meaningless outside the context they came from, and
        // deleting them against a different one would take out somebody else's objects.
        virtual void OnDeviceLost() = 0;
    };

    void Device_Register(IDeviceResource* resource);
    void Device_Unregister(IDeviceResource* resource);

    // Called from the GL_SetMode post-hook. This engine creates the GL context once per
    // process (sys_dll2 calls GL_SetMode at startup) and a resolution change only resizes
    // the window through SDL_SetWindowSize, so in practice this runs exactly once; it
    // still notifies resources and re-resolves the entry points so the protocol stays
    // correct if a context is ever recreated.
    void Device_OnContextCreated();

    // Called while the context is still current. Marks the device as gone so destructors
    // running later do not issue GL calls into it.
    void Device_OnContextDestroyed();

    // Whether the pipeline may run against the current context. Probes lazily, so it is
    // safe to call before the first Device_OnContextCreated.
    bool Device_Available();

    // Whether GL calls may still be issued. False between Device_OnContextDestroyed and
    // the next Device_OnContextCreated; a resource destructor must check this before
    // calling any glDelete*.
    bool Device_ContextAlive();

    // Incremented on every context creation. A handle captured under an older
    // generation must not be used.
    uint32_t Device_Generation();

    // glGetString with a caller-chosen fallback for a null answer. GLenum spelled as
    // unsigned int so this header does not drag in a GL loader.
    const char* Device_GlString(unsigned int name, const char* fallback);
}
