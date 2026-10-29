#pragma once

#include <cstdint>

#define NITRO_FRAME_LIFECYCLE_INTERFACE_VERSION "NITRO_FRAME_LIFECYCLE_001"

namespace nitroapi
{
    class FrameLifecycleObserverInterface
    {
    public:
        virtual ~FrameLifecycleObserverInterface() = default;
        virtual void OnHostEntry() = 0;
        virtual void OnFrameAccepted() = 0;
        virtual void OnHostExit(bool completed) = 0;
    };

    using FrameLifecycleSubscription_t = uint64_t;

    // Registrations stay on the host thread and must end before the observer is destroyed.
    class FrameLifecycleInterface
    {
    public:
        virtual ~FrameLifecycleInterface() = default;
        virtual FrameLifecycleSubscription_t Subscribe(FrameLifecycleObserverInterface* observer) = 0;
        virtual void Unsubscribe(FrameLifecycleSubscription_t subscription) = 0;
    };
} // namespace nitroapi
