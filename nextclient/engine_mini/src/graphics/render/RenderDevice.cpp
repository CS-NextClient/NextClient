#include "graphics/render/RenderDevice.h"

#include <algorithm>

#include <glad/glad.h>
#include <ncl_gl/loader.h>

#include "console/console.h"
#include "graphics/render/render_context.h"

namespace render
{
    void RenderDevice::Register(IDeviceResource* resource)
    {
        if (std::find(resources_.begin(), resources_.end(), resource) == resources_.end())
        {
            resources_.push_back(resource);
        }
    }

    void RenderDevice::Unregister(IDeviceResource* resource)
    {
        std::erase(resources_, resource);
    }

    void RenderDevice::NotifyLost()
    {
        // A callback may destroy and unregister a later resource.
        std::vector<IDeviceResource*> snapshot = resources_;

        for (IDeviceResource* resource : snapshot)
        {
            if (std::find(resources_.begin(), resources_.end(), resource) != resources_.end())
            {
                resource->OnDeviceLost();
            }
        }
    }

    void RenderDevice::Probe()
    {
        probed_ = true;
        available_ = false;

        if (!ncl_gl::EnsureLoaded())
        {
            Con_DPrintf(ConLogType::Warning, "render: failed to resolve GL entry points\n");
            return;
        }

        available_ = GLAD_GL_VERSION_3_3 != 0;
        Con_DPrintf(
            ConLogType::Info,
            "render: GL %s / GLSL %s, shader pipeline %s\n",
            Device_GlString(GL_VERSION, "<unavailable>"),
            Device_GlString(GL_SHADING_LANGUAGE_VERSION, "<unavailable>"),
            available_ ? "available" : "unavailable, needs GL 3.3"
        );
    }

    void RenderDevice::OnContextCreated()
    {
        void* current = context::Current();

        if (IsContext(current))
        {
            if (!ncl_gl::IsLoaded())
            {
                ncl_gl::Reload();
                probed_ = false;
                Probe();
            }

            return;
        }

        OnContextDestroyed();

        if (current == nullptr)
        {
            return;
        }

        ++generation_;
        context_identity_ = current;
        context_alive_ = true;
        ncl_gl::Reload();
        Probe();
    }

    void RenderDevice::OnContextDestroyed()
    {
        bool notify = context_alive_ || context_identity_ != nullptr;
        context_alive_ = false;
        context_identity_ = nullptr;
        probed_ = false;
        available_ = false;

        if (notify)
        {
            NotifyLost();
        }
    }

    bool RenderDevice::IsContext(void* context) const
    {
        return context_alive_ && context != nullptr && context_identity_ == context;
    }

    bool RenderDevice::IsContextCurrent()
    {
        return IsContext(context::Current()) && ncl_gl::IsLoaded();
    }

    bool RenderDevice::CheckAvailable()
    {
        if (!IsContextCurrent())
        {
            return false;
        }

        if (!probed_)
        {
            Probe();
        }

        return available_;
    }
} // namespace render
