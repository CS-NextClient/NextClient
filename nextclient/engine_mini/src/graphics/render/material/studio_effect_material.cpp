#include "graphics/render/material/studio_effect_material.h"

namespace render::effects
{
    const ncl_material::ParamValue* Parameter(
        const ncl_material::Material* material,
        const ncl_material::ProgramManifest* manifest,
        std::string_view name
    )
    {
        if (material == nullptr)
        {
            return nullptr;
        }

        if (const ncl_material::ParamValue* value = material->FindParam(name))
        {
            return value;
        }

        const ncl_material::ParamSchema* schema = manifest != nullptr ? manifest->FindParam(name) : nullptr;
        return schema != nullptr && schema->has_default ? &schema->default_value : nullptr;
    }

    float Scalar(const ncl_material::Material* material, const ncl_material::ProgramManifest* manifest, StudioParameterRole role)
    {
        const StudioProgramContract* contract = StudioContract_For(material, manifest);
        const ncl_material::ParamValue* value =
            contract != nullptr ? Parameter(material, manifest, contract->get_parameter(role).name) : nullptr;
        return value != nullptr && value->type == ncl_material::ParamType::Scalar ? value->scalar : 0.0f;
    }

    std::array<float, 4> Color(
        const ncl_material::Material* material,
        const ncl_material::ProgramManifest* manifest,
        StudioParameterRole role
    )
    {
        const StudioProgramContract* contract = StudioContract_For(material, manifest);
        const ncl_material::ParamValue* value =
            contract != nullptr ? Parameter(material, manifest, contract->get_parameter(role).name) : nullptr;
        return value != nullptr && value->type == ncl_material::ParamType::Vec4 ? value->vec4 : std::array<float, 4>{};
    }

    bool IsEnabled(const ncl_material::Material& material, const ncl_material::ProgramManifest* manifest)
    {
        const StudioProgramContract* contract = StudioContract_For(&material, manifest);
        if (contract == nullptr)
        {
            return false;
        }

        return (contract->supports_occlusion && Scalar(&material, manifest, StudioParameterRole::OccludedAmount) > 0 &&
                Color(&material, manifest, StudioParameterRole::OccludedColor)[3] > 0) ||
               (contract->supports_outline && Scalar(&material, manifest, StudioParameterRole::OutlineWidth) > 0 &&
                Color(&material, manifest, StudioParameterRole::OutlineColor)[3] > 0);
    }
} // namespace render::effects
