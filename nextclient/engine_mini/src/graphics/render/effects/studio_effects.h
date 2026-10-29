#pragma once

#include "graphics/render/effects/StudioEffects.h"

namespace ncl_material
{
    struct Material;
}

namespace render
{
    struct StudioDrawPacket;
    struct StudioGpuModel;

    StudioEffectsStats StudioEffects_LastStats();

    bool StudioEffects_IsEnabled(const StudioDrawPacket& packet);
    bool StudioEffects_IsEnabled(const ncl_material::Material& material);
    bool StudioEffects_Contains(uintptr_t entity);
    std::array<int, 4> StudioEffects_ProjectBounds(const StudioGpuModel& model, const StudioDrawPacket& packet);
    void StudioEffects_BeginView();
    void StudioEffects_Capture(
        uintptr_t entity,
        const StudioDrawPacket& packet,
        const std::array<int, 4>& bounds,
        bool include_occluders = false
    );
    void StudioEffects_EndView();
    void StudioEffects_Clear();
    size_t StudioEffects_TargetCount();
} // namespace render
