#pragma once

#include <array>
#include <string_view>

#include "graphics/render/material/StudioProgramContract.h"

namespace render::effects
{
    const ncl_material::ParamValue* Parameter(
        const ncl_material::Material* material,
        const ncl_material::ProgramManifest* manifest,
        std::string_view name
    );
    float Scalar(const ncl_material::Material* material, const ncl_material::ProgramManifest* manifest, StudioParameterRole role);
    std::array<float, 4> Color(
        const ncl_material::Material* material,
        const ncl_material::ProgramManifest* manifest,
        StudioParameterRole role
    );
    bool IsEnabled(const ncl_material::Material& material, const ncl_material::ProgramManifest* manifest);
} // namespace render::effects
