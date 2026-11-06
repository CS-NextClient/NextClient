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

    // Adopts a new native identity, or retries entry-point preparation for the same identity.
    void Device_OnContextCreated();

    // Marks the device unavailable before notifying registered resources. No GL calls.
    void Device_OnContextDestroyed();

    // Probes GL support on first use and caches the result until the context changes.
    bool Device_CheckAvailable();

    // Stored lifecycle state; this does not establish native current-context ownership.
    bool Device_IsContextAlive();
    bool Device_IsContext(void* context);
    // Requires a matching non-null native identity and initialized GL entry points.
    bool Device_IsContextCurrent();

    // Incremented when a context is adopted. A handle captured under an older
    // generation must not be used.
    uint32_t Device_Generation();

    // glGetString with a caller-chosen fallback for a null answer. GLenum spelled as
    // unsigned int so this header does not drag in a GL loader.
    const char* Device_GlString(unsigned int name, const char* fallback);
} // namespace render
