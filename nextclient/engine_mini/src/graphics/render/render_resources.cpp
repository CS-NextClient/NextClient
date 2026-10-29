#include "graphics/render/render_resources.h"

namespace render
{
    RendererResources& GetResources()
    {
        // Static resource owners register through this accessor, so the registry outlives them.
        static RendererResources resources(GetShaderBackend(), GetManifestBackend(), GetMaterialImageLoader());
        return resources;
    }
} // namespace render
