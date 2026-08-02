#include "engine.h"

#include <algorithm>
#include <vector>

#include <glad/glad.h>
#include <ncl_gl/loader.h>

#include "console/console.h"
#include "graphics/render/RenderDevice.h"

namespace
{
    // Function-local so a resource with static storage in another translation unit can
    // register from its constructor regardless of initialization order.
    std::vector<render::IDeviceResource*>& Resources()
    {
        static std::vector<render::IDeviceResource*> resources;
        return resources;
    }

    uint32_t g_Generation = 1;
    bool g_Probed;
    bool g_Available;
    bool g_ContextAlive;

    // Over a copy, because a handler may destroy other registered resources: a cache
    // that drops its entries takes their buffers with it, and every ~GpuBuffer unregisters
    // itself, which would leave this walking a vector that shrank underneath it.
    void NotifyLost()
    {
        std::vector<render::IDeviceResource*> snapshot = Resources();

        for (render::IDeviceResource* resource : snapshot)
        {
            // Still registered: one handler may have taken another off the list already.
            const std::vector<render::IDeviceResource*>& live = Resources();

            if (std::find(live.begin(), live.end(), resource) != live.end())
            {
                resource->OnDeviceLost();
            }
        }
    }

    void Probe()
    {
        g_Probed = true;
        g_Available = false;

        if (!ncl_gl::EnsureLoaded())
        {
            Con_DPrintf(ConLogType::Warning, "render: failed to resolve GL entry points\n");
            return;
        }

        g_Available = GLAD_GL_VERSION_3_3 != 0;

        Con_DPrintf(
            ConLogType::Info,
            "render: GL %s / GLSL %s, shader pipeline %s\n",
            render::Device_GlString(GL_VERSION, "<unavailable>"),
            render::Device_GlString(GL_SHADING_LANGUAGE_VERSION, "<unavailable>"),
            g_Available ? "available" : "unavailable, needs GL 3.3"
        );
    }
}

void render::Device_Register(IDeviceResource* resource)
{
    std::vector<IDeviceResource*>& resources = Resources();

    if (std::find(resources.begin(), resources.end(), resource) == resources.end())
    {
        resources.push_back(resource);
    }
}

void render::Device_Unregister(IDeviceResource* resource)
{
    std::erase(Resources(), resource);
}

void render::Device_OnContextCreated()
{
    // Cleared first, so that a handler which destroys owned resources cannot reach a
    // glDelete guarded on it: those names belong to the context that is gone, and deleting
    // them against the new one would take out whatever now holds them.
    g_ContextAlive = false;

    // Resources drop their handles without touching GL: any handle still held here came
    // from a context that no longer exists.
    NotifyLost();

    g_Generation++;
    g_Probed = false;
    g_ContextAlive = true;

    ncl_gl::Reload();
    Probe();
}

void render::Device_OnContextDestroyed()
{
    NotifyLost();

    g_ContextAlive = false;
    g_Probed = false;
    g_Available = false;
}

bool render::Device_Available()
{
    if (!g_Probed)
    {
        Probe();
    }

    return g_Available;
}

bool render::Device_ContextAlive()
{
    return g_ContextAlive;
}

uint32_t render::Device_Generation()
{
    return g_Generation;
}

const char* render::Device_GlString(unsigned int name, const char* fallback)
{
    const GLubyte* value = glGetString(name);
    return value != nullptr ? reinterpret_cast<const char*>(value) : fallback;
}
