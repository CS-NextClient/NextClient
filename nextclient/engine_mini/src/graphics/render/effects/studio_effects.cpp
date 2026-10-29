#include "graphics/render/effects/studio_effects.h"

#include "graphics/render/RendererState.h"

render::StudioEffectsStats render::StudioEffects_LastStats()
{
    return GetRendererState().effects.get_last_stats();
}

bool render::StudioEffects_IsEnabled(const StudioDrawPacket& packet)
{
    return GetRendererState().effects.CheckEnabled(packet);
}

bool render::StudioEffects_IsEnabled(const ncl_material::Material& material)
{
    return GetRendererState().effects.CheckEnabled(material);
}

bool render::StudioEffects_Contains(uintptr_t entity)
{
    return GetRendererState().effects.Contains(entity);
}

std::array<int, 4> render::StudioEffects_ProjectBounds(const StudioGpuModel& model, const StudioDrawPacket& packet)
{
    return GetRendererState().effects.ProjectBounds(model, packet);
}

void render::StudioEffects_BeginView()
{
    GetRendererState().effects.BeginView();
}

void render::StudioEffects_Capture(
    uintptr_t entity,
    const StudioDrawPacket& packet,
    const std::array<int, 4>& bounds,
    bool include_occluders
)
{
    GetRendererState().effects.Capture(entity, packet, bounds, include_occluders);
}

void render::StudioEffects_EndView()
{
    GetRendererState().effects.EndView();
}

void render::StudioEffects_Clear()
{
    GetRendererState().effects.Clear();
}

size_t render::StudioEffects_TargetCount()
{
    return GetRendererState().effects.get_target_count();
}
