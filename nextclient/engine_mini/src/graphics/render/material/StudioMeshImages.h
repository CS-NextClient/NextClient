#pragma once

#include <array>

#include "graphics/render/StudioBindings.h"
#include "graphics/render/material/MaterialTextureCache.h"

namespace ncl_material
{
    struct ProgramManifest;
}

namespace render
{
    struct StudioMeshImages
    {
        const ncl_material::ProgramManifest* manifest{};
        std::array<unsigned int, kStudioMaterialUnits> material{};
        int material_count{};
        unsigned int coverage_dissolve_map{};
        MaterialImage base;
        bool base_image_uv{};
    };
} // namespace render
