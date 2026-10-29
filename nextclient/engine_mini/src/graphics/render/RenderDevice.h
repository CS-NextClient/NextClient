#pragma once

#include <cstdint>
#include <vector>

#include "graphics/render/render_device.h"

namespace render
{
    class RenderDevice
    {
    private:
        std::vector<IDeviceResource*> resources_;
        uint32_t generation_ = 1;
        bool probed_{};
        bool available_{};
        bool context_alive_{};
        void* context_identity_{};

    public:
        RenderDevice() = default;
        RenderDevice(const RenderDevice&) = delete;
        RenderDevice& operator=(const RenderDevice&) = delete;
        void Register(IDeviceResource* resource);
        void Unregister(IDeviceResource* resource);
        void OnContextCreated();
        void OnContextDestroyed();
        bool CheckAvailable();
        bool IsContext(void* context) const;
        bool IsContextCurrent();

        void* get_context_identity() const
        {
            return context_identity_;
        }

        bool is_context_alive() const
        {
            return context_alive_;
        }

        bool is_available() const
        {
            return context_alive_ && probed_ && available_;
        }

        uint32_t get_generation() const
        {
            return generation_;
        }

    private:
        void NotifyLost();
        void Probe();
    };

} // namespace render
