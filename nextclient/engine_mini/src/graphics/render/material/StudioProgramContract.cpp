#include "graphics/render/material/StudioProgramContract.h"

#include <algorithm>
#include <cmath>

#include <ncl_material/MaterialResolve.h>

#include "graphics/render/material/material_manifests.h"

namespace
{
    using ncl_material::ParamSchema;
    using ncl_material::ParamType;
    using render::StudioParameterRole;

    constexpr const char* kContractName = "studio_effects_v1";

    ParamSchema Scalar(const char* name, float fallback, float minimum, float maximum, bool instance = false)
    {
        ParamSchema schema;
        schema.name = name;
        schema.type = ParamType::Scalar;
        schema.instance = instance;
        schema.has_range = true;
        schema.min_value = minimum;
        schema.max_value = maximum;
        schema.has_default = true;
        schema.default_value.type = ParamType::Scalar;
        schema.default_value.scalar = fallback;

        return schema;
    }

    ParamSchema Vector(const char* name, std::array<float, 4> fallback, bool ranged)
    {
        ParamSchema schema;
        schema.name = name;
        schema.type = ParamType::Vec4;
        schema.has_range = ranged;
        schema.max_value = ranged ? 1.0f : 0.0f;
        schema.has_default = true;
        schema.default_value.type = ParamType::Vec4;
        schema.default_value.vec4 = fallback;

        return schema;
    }

    ParamSchema Sampler(const char* name)
    {
        ParamSchema schema;
        schema.name = name;
        schema.type = ParamType::TextureRef;

        return schema;
    }

    const render::StudioProgramContract kStudioEffectsV1{
        render::StudioCoverageProfile::DissolveNoiseGlitchV1,
        true,
        true,
        {Vector("uBaseTint", {1, 1, 1, 1}, true),
         Scalar("uOccludedAmount", 0, 0, 1, true),
         Vector("uOccludedColor", {1, 0.2f, 0.05f, 0.5f}, true),
         Scalar("uOutlineMode", 0, 0, 2, true),
         Scalar("uOutlineWidth", 0, 0, 16, true),
         Vector("uOutlineColor", {1, 0.5f, 0.1f, 1}, true),
         Scalar("uGlowRadius", 8, 0, 64),
         Scalar("uGlowStrength", 0, 0, 4, true),
         Sampler("uDissolveMap"),
         Vector("uDissolveTransform", {1, 1, 0, 0}, false),
         Scalar("uDissolve", 0, 0, 1, true),
         Scalar("uDissolveSpace", 0, 0, 1, false),
         Scalar("uNoise", 0, 0, 1, true),
         Scalar("uNoiseSpace", 0, 0, 1, false),
         Scalar("uNoiseScale", 256, 1, 1024),
         Scalar("uNoiseSeed", 0, 0, 32767, true),
         Scalar("uNoiseSpeed", 0, 0, 60, true),
         Scalar("uGlitchAmount", 0, 0, 1, true),
         Scalar("uGlitchDisplacement", 12, 0, 64, true),
         Scalar("uGlitchBands", 24, 1, 128, true),
         Scalar("uGlitchRgbSplit", 2, 0, 16, true),
         Scalar("uGlitchSpeed", 12, 0, 60, true),
         Scalar("uGlitchSeed", 0, 0, 32767, true)},
        {StudioParameterRole::Dissolve,
         StudioParameterRole::DissolveSpace,
         StudioParameterRole::Noise,
         StudioParameterRole::NoiseSpace,
         StudioParameterRole::NoiseScale,
         StudioParameterRole::NoiseSeed,
         StudioParameterRole::NoiseSpeed,
         StudioParameterRole::GlitchAmount,
         StudioParameterRole::GlitchDisplacement,
         StudioParameterRole::GlitchBands,
         StudioParameterRole::GlitchRgbSplit,
         StudioParameterRole::GlitchSpeed,
         StudioParameterRole::GlitchSeed},
        {StudioParameterRole::OccludedAmount, StudioParameterRole::OutlineWidth}
    };

    bool MatchesDefault(const ParamSchema& actual, const ParamSchema& expected, bool restricted_activation)
    {
        if (actual.has_default != expected.has_default)
        {
            return false;
        }
        if (!expected.has_default)
        {
            return true;
        }
        if (actual.default_value.type != expected.type)
        {
            return false;
        }
        if (restricted_activation)
        {
            // Session policy writes explicit zero overrides even for positive embedded defaults.
            return std::isfinite(actual.default_value.scalar);
        }
        if (expected.type == ParamType::Scalar)
        {
            return actual.default_value.scalar == expected.default_value.scalar;
        }
        if (expected.type == ParamType::Vec4)
        {
            return actual.default_value.vec4 == expected.default_value.vec4;
        }
        return actual.default_value.texture_ref == expected.default_value.texture_ref;
    }
} // namespace

const ncl_material::ParamSchema& render::StudioProgramContract::get_parameter(StudioParameterRole role) const
{
    return parameters[static_cast<size_t>(role)];
}

const render::StudioProgramContract& render::StudioContract_EffectsV1()
{
    return kStudioEffectsV1;
}

ncl_utils::Result<> render::StudioContract_Validate(const ncl_material::ProgramManifest& manifest)
{
    if (manifest.render_contract.empty())
    {
        return {};
    }
    if (manifest.render_contract != kContractName)
    {
        return ncl_utils::ResultError("unknown render contract: " + manifest.render_contract);
    }

    ncl_material::Material restricted_defaults;
    restricted_defaults.program = manifest.program;

    for (size_t index = 0; index < kStudioEffectsV1.parameters.size(); index++)
    {
        const ParamSchema& expected = kStudioEffectsV1.parameters[index];
        const ParamSchema* actual = manifest.FindParam(expected.name);
        const StudioParameterRole role = static_cast<StudioParameterRole>(index);
        const bool restricted =
            std::find(kStudioEffectsV1.restricted_activations.begin(), kStudioEffectsV1.restricted_activations.end(), role) !=
            kStudioEffectsV1.restricted_activations.end();

        if (actual == nullptr || actual->type != expected.type || actual->instance != expected.instance ||
            actual->has_range != expected.has_range ||
            (expected.has_range && (actual->min_value != expected.min_value || actual->max_value != expected.max_value)) ||
            !MatchesDefault(*actual, expected, restricted))
        {
            return ncl_utils::ResultError("render contract " + manifest.render_contract + ": incompatible parameter " + expected.name);
        }

        if (restricted)
        {
            restricted_defaults.params.push_back({actual->name, actual->default_value});
        }
    }

    return ncl_material::Validate(restricted_defaults, manifest);
}

const render::StudioProgramContract* render::StudioContract_For(const ncl_material::Material* material)
{
    if (material == nullptr || material->program == ncl_material::kLegacyProgram)
    {
        return nullptr;
    }

    const ncl_material::ProgramManifest* manifest = Manifest_Get(material->program);
    return StudioContract_For(material, manifest);
}

const render::StudioProgramContract* render::StudioContract_For(
    const ncl_material::Material* material,
    const ncl_material::ProgramManifest* manifest
)
{
    if (material == nullptr || material->program == ncl_material::kLegacyProgram)
    {
        return nullptr;
    }

    return manifest != nullptr && manifest->render_contract == kContractName ? &kStudioEffectsV1 : nullptr;
}

float render::StudioContract_PickingAlpha(const ncl_material::Material* material, float packet_blend)
{
    const ncl_material::ProgramManifest* manifest =
        material != nullptr && material->program != ncl_material::kLegacyProgram ? Manifest_Get(material->program) : nullptr;
    return StudioContract_PickingAlpha(material, manifest, packet_blend);
}

float render::StudioContract_PickingAlpha(
    const ncl_material::Material* material,
    const ncl_material::ProgramManifest* manifest,
    float packet_blend
)
{
    const StudioProgramContract* contract = StudioContract_For(material, manifest);
    if (contract == nullptr)
    {
        return packet_blend;
    }

    const std::string& name = contract->get_parameter(StudioParameterRole::BaseAlpha).name;
    const ncl_material::ParamValue* tint = material->FindParam(name);
    if (tint == nullptr)
    {
        const ncl_material::ParamSchema* schema = manifest->FindParam(name);
        tint = schema != nullptr && schema->has_default ? &schema->default_value : nullptr;
    }
    return tint != nullptr && tint->type == ParamType::Vec4 ? packet_blend * tint->vec4[3] : packet_blend;
}

void render::StudioContract_DisableRestrictedEffects(ncl_material::Material& material)
{
    const ncl_material::ProgramManifest* manifest =
        material.program != ncl_material::kLegacyProgram ? Manifest_Get(material.program) : nullptr;
    StudioContract_DisableRestrictedEffects(material, manifest);
}

void render::StudioContract_DisableRestrictedEffects(ncl_material::Material& material, const ncl_material::ProgramManifest* manifest)
{
    const StudioProgramContract* contract = StudioContract_For(&material, manifest);
    if (contract == nullptr)
    {
        return;
    }

    for (StudioParameterRole role : contract->restricted_activations)
    {
        const std::string& name = contract->get_parameter(role).name;
        bool found = false;

        for (ncl_material::Param& param : material.params)
        {
            if (param.name == name)
            {
                param.value = {};
                found = true;
                break;
            }
        }

        if (!found)
        {
            material.params.push_back(ncl_material::Param{name, {}});
        }
    }
}
