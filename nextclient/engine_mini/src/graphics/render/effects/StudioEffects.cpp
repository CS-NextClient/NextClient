#include "graphics/render/effects/StudioEffects.h"

#include <algorithm>

#include <glad/glad.h>

#include "console/console.h"
#include "graphics/render/ScopedGlState.h"
#include "graphics/render/material/MaterialCoverageRuntime.h"
#include "graphics/render/RenderDevice.h"
#include "graphics/render/StudioQueue.h"
#include "graphics/render/material/studio_effect_material.h"

render::StudioEffects::StudioEffects(RenderDevice& device, ShaderManager& shaders, MaterialCoverageRuntime& coverage) :
    device_(device),
    coverage_(coverage),
    targets_(device),
    passes_(targets_, shaders, coverage)
{
    device_.Register(this);
}

render::StudioEffects::~StudioEffects()
{
    Reset(false);
    device_.Unregister(this);
}

bool render::StudioEffects::CheckEnabled(const ncl_material::Material& material) const
{
    return effects::IsEnabled(material, coverage_.ManifestFor(&material));
}

bool render::StudioEffects::CheckEnabled(const StudioDrawPacket& packet) const
{
    return std::any_of(packet.meshes.begin(), packet.meshes.end(), [](const StudioQueuedMesh& mesh) {
        return mesh.material && effects::IsEnabled(*mesh.material, mesh.images.manifest);
    });
}

render::effects::Rect_t render::StudioEffects::ProjectBounds(const StudioGpuModel& model, const StudioDrawPacket& packet) const
{
    float radius = 2;
    for (const StudioQueuedMesh& mesh : packet.meshes)
    {
        const ncl_material::Material* material = mesh.material ? &*mesh.material : nullptr;
        const ncl_material::ProgramManifest* manifest = mesh.images.manifest;
        const float displacement = effects::Scalar(material, manifest, StudioParameterRole::GlitchAmount) > 0
                                       ? effects::Scalar(material, manifest, StudioParameterRole::GlitchDisplacement)
                                       : 0;
        const float width = effects::Scalar(material, manifest, StudioParameterRole::OutlineWidth);
        const float glow = width > 0 && effects::Scalar(material, manifest, StudioParameterRole::GlowStrength) > 0
                               ? effects::Scalar(material, manifest, StudioParameterRole::GlowRadius)
                               : 0;
        radius = std::max(radius, width + glow + displacement + 2);
    }

    return effects::ProjectBounds(model, packet, radius);
}

uint64_t render::StudioEffects::BeginView()
{
    views_.emplace_back();
    views_.back().identity = ++next_view_identity_;
    return views_.back().identity;
}

void render::StudioEffects::CancelView(uint64_t identity)
{
    const auto view =
        std::find_if(views_.begin(), views_.end(), [identity](const View& candidate) { return candidate.identity == identity; });
    if (view == views_.end())
    {
        return;
    }

    targets_.ReleaseMasks(view->masks, device_.IsContextCurrent());
    views_.erase(view);
}

size_t render::StudioEffects::get_view_count() const
{
    return views_.size();
}

bool render::StudioEffects::Contains(uintptr_t entity) const
{
    if (views_.empty())
    {
        return false;
    }

    const View& view = views_.back();
    return std::any_of(view.masks.begin(), view.masks.end(), [entity](const std::unique_ptr<StudioEffectTargets::Mask>& mask) {
        return mask->get_entity() == entity;
    });
}

void render::StudioEffects::Capture(uintptr_t entity, const StudioDrawPacket& packet, const effects::Rect_t& bounds, bool include_occluders)
{
    if (!coverage_.is_texture_revision(packet.texture_revision))
    {
        return;
    }
    if (views_.empty() || packet.glow_scale != 0 || packet.viewport[2] <= 0 || packet.viewport[3] <= 0 || bounds[2] <= 0 || bounds[3] <= 0)
    {
        return;
    }

    if (!Contains(entity) && !CheckEnabled(packet) && !include_occluders)
    {
        return;
    }

    ScopedGlState saved;
    GLint samples = 0;
    glGetIntegerv(GL_SAMPLES, &samples);

    const ncl_gl::Program* mask_program = passes_.PrepareCapture();
    if (mask_program == nullptr)
    {
        return;
    }

    View& view = views_.back();
    bool reused = false;
    const bool reversed = packet.raster.depth_range[0] > packet.raster.depth_range[1];
    StudioEffectTargets::Mask* mask = targets_.AcquireMask(view.masks, entity, bounds, reversed, samples, reused);

    view.stats.reused_targets += reused ? 1 : 0;
    if (mask == nullptr)
    {
        return;
    }

    if (!targets_.EnsureCaptureDepth(packet.viewport, samples))
    {
        Con_DPrintf(ConLogType::Warning, "render: material effect depth target is incomplete\n");
        return;
    }

    view.viewport = packet.viewport;
    view.stats.packets++;

    passes_.CaptureMask(*mask, packet, *mask_program);
    passes_.CaptureDepth(*mask, packet, *mask_program);
}

void render::StudioEffects::EndView()
{
    if (views_.empty())
    {
        return;
    }

    View view = std::move(views_.back());
    views_.pop_back();
    if (view.masks.empty())
    {
        return;
    }

    ScopedGlState saved;
    StudioEffectPasses::Composition composition;
    if (!passes_.PrepareComposition(view.viewport, composition))
    {
        targets_.ReleaseMasks(view.masks, device_.IsContextCurrent());
        return;
    }

    view.stats.entities = static_cast<uint32_t>(view.masks.size());
    view.stats.depth_format = targets_.get_scene_format();
    view.stats.samples = targets_.get_scene_samples();
    for (const std::unique_ptr<StudioEffectTargets::Mask>& mask : view.masks)
    {
        view.stats.mask_pixels += static_cast<uint64_t>(mask->get_rect()[2]) * mask->get_rect()[3];
        passes_.Seed(*mask, view.viewport, composition);
        const int seed_index = passes_.PropagateSeeds(view.viewport, composition);
        passes_.Composite(*mask, view.viewport, seed_index, composition);
    }

    last_stats_ = view.stats;
    targets_.RecycleMasks(view.masks);
}

void render::StudioEffects::Clear()
{
    Reset(device_.IsContextCurrent());
}

void render::StudioEffects::OnDeviceLost()
{
    Reset(false);
}

void render::StudioEffects::Reset(bool release_gl)
{
    for (View& view : views_)
    {
        targets_.ReleaseMasks(view.masks, release_gl);
    }

    views_.clear();
    targets_.Clear(release_gl);
    last_stats_ = {};
}

render::StudioEffectsStats render::StudioEffects::get_last_stats() const
{
    return last_stats_;
}

size_t render::StudioEffects::get_target_count() const
{
    return views_.empty() ? 0 : views_.back().masks.size();
}
