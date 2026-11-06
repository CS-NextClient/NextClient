#pragma once

#include "graphics/render/material/MaterialManifestCatalog.h"
#include "graphics/render/material/MaterialTextureCache.h"
#include "graphics/render/ShaderManager.h"

namespace render
{
    class RendererResources
    {
    public:
        RenderDevice device;
        ShaderManager shaders;
        MaterialManifestCatalog manifests;
        MaterialTextureCache textures;

        RendererResources(ShaderBackend& shader_backend, ManifestBackend& manifest_backend, MaterialImageLoader& images) :
            shaders(device, shader_backend),
            manifests(shaders, manifest_backend),
            textures(device, images)
        {}
        RendererResources(const RendererResources&) = delete;
        RendererResources& operator=(const RendererResources&) = delete;
    };
} // namespace render
