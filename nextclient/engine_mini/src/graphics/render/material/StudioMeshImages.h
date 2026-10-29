#pragma once

#include <array>

#include "graphics/render/StudioBindings.h"

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
    };
} // namespace render
