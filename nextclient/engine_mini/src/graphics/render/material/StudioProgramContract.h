#pragma once

#include <array>

#include <ncl_material/ProgramManifest.h>

namespace render
{
    enum class StudioCoverageProfile
    {
        DissolveNoiseGlitchV1
    };

    enum class StudioParameterRole
    {
        BaseAlpha,
        OccludedAmount,
        OccludedColor,
        OutlineMode,
        OutlineWidth,
        OutlineColor,
        GlowRadius,
        GlowStrength,
        DissolveMap,
        DissolveTransform,
        Dissolve,
        DissolveSpace,
        Noise,
        NoiseSpace,
        NoiseScale,
        NoiseSeed,
        NoiseSpeed,
        GlitchAmount,
        GlitchDisplacement,
        GlitchBands,
        GlitchRgbSplit,
        GlitchSpeed,
        GlitchSeed,
        Count
    };

    struct StudioProgramContract
    {
        const StudioCoverageProfile coverage_profile;
        const bool supports_outline;
        const bool supports_occlusion;
        const std::array<ncl_material::ParamSchema, static_cast<size_t>(StudioParameterRole::Count)> parameters;
        const std::array<StudioParameterRole, 13> coverage_scalars;
        const std::array<StudioParameterRole, 2> restricted_activations;

        const ncl_material::ParamSchema& get_parameter(StudioParameterRole role) const;
    };

    ncl_utils::Result<> StudioContract_Validate(const ncl_material::ProgramManifest& manifest);
    const StudioProgramContract& StudioContract_EffectsV1();
    const StudioProgramContract* StudioContract_For(const ncl_material::Material* material);
    const StudioProgramContract* StudioContract_For(const ncl_material::Material* material, const ncl_material::ProgramManifest* manifest);
    float StudioContract_PickingAlpha(const ncl_material::Material* material, float packet_blend);
    float StudioContract_PickingAlpha(
        const ncl_material::Material* material,
        const ncl_material::ProgramManifest* manifest,
        float packet_blend
    );
    void StudioContract_DisableRestrictedEffects(ncl_material::Material& material);
    void StudioContract_DisableRestrictedEffects(ncl_material::Material& material, const ncl_material::ProgramManifest* manifest);
} // namespace render
