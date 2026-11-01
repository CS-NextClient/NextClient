#include "graphics/render/effects/StudioEffects.h"

#include <algorithm>
#include <iterator>

#include <glad/glad.h>

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

    ReleaseEntities(*view, device_.IsContextCurrent());
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
    return std::any_of(view.entities.begin(), view.entities.end(), [entity](const Entity& candidate) {
        return candidate.identity == entity;
    });
}

void render::StudioEffects::Capture(
    uintptr_t identity,
    const StudioDrawPacket& packet,
    const effects::Rect_t& bounds,
    bool include_occluders,
    bool allow_deferred
)
{
    if (views_.empty() || !coverage_.is_texture_revision(packet.texture_revision) || packet.glow_scale != 0 || packet.viewport[2] <= 0 ||
        packet.viewport[3] <= 0 || bounds[2] <= 0 || bounds[3] <= 0)
    {
        return;
    }

    if (!Contains(identity) && !CheckEnabled(packet) && !include_occluders)
    {
        return;
    }

    if (!device_.IsContextCurrent())
    {
        return;
    }

    GLint samples = 0;
    GLint output = 0;
    glGetIntegerv(GL_SAMPLES, &samples);
    glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &output);

    View& view = views_.back();
    if (view.entities.empty())
    {
        view.viewport = packet.viewport;
        view.samples = samples;
        view.output = output;
        view.generation = device_.get_generation();
    }

    if (view.viewport != packet.viewport || view.samples != samples || view.output != output)
    {
        return;
    }

    auto found = std::find_if(view.entities.begin(), view.entities.end(), [identity](const Entity& entity) {
        return entity.identity == identity;
    });
    if (found == view.entities.end())
    {
        view.entities.emplace_back();
        found = std::prev(view.entities.end());
        found->identity = identity;
        found->bounds = bounds;
    }
    else
    {
        const effects::Rect_t old = found->bounds;
        const int left = std::min(old[0], bounds[0]);
        const int bottom = std::min(old[1], bounds[1]);
        found->bounds = {
            left,
            bottom,
            std::max(old[0] + old[2], bounds[0] + bounds[2]) - left,
            std::max(old[1] + old[3], bounds[1] + bounds[3]) - bottom
        };
    }

    found->packets.push_back(packet);
    StudioDrawPacket& snapshot = found->packets.back();
    snapshot.rigid_bindings = nullptr;

    for (StudioQueuedMesh& mesh : snapshot.meshes)
    {
        mesh.program = nullptr;
        mesh.material_bindings = nullptr;

        const ncl_material::Material* material = mesh.material ? &*mesh.material : nullptr;
        const float width = effects::Scalar(material, mesh.images.manifest, StudioParameterRole::OutlineWidth);
        const bool glow = effects::Scalar(material, mesh.images.manifest, StudioParameterRole::GlowStrength) > 0 &&
                          effects::Scalar(material, mesh.images.manifest, StudioParameterRole::GlowRadius) > 0;

        found->outline = found->outline || width > 0;
        found->distance = found->distance || width > 2 || (width > 0 && glow);
    }

    if (!allow_deferred)
    {
        ScopedGlState saved;
        CaptureEntity(view, *found);
    }
}

bool render::StudioEffects::CaptureEntity(View& view, Entity& entity, StudioEffectTargets::ObjectDepthStorage depth_storage)
{
    if (entity.packets.empty())
    {
        return !entity.masks.empty();
    }

    if (std::any_of(entity.packets.begin(), entity.packets.end(), [this](const StudioDrawPacket& packet) {
            return !coverage_.is_texture_revision(packet.texture_revision);
        }))
    {
        entity.packets.clear();
        targets_.ReleaseMasks(entity.masks, true);
        return false;
    }

    const ncl_gl::Program* program = passes_.PrepareCapture();
    if (program == nullptr || !targets_.EnsureCaptureDepth(view.viewport, view.samples))
    {
        entity.packets.clear();
        targets_.ReleaseMasks(entity.masks, true);
        return false;
    }

    bool reused = false;
    const bool reversed = entity.packets.front().raster.depth_range[0] > entity.packets.front().raster.depth_range[1];
    StudioEffectTargets::Mask* mask =
        targets_.AcquireMask(entity.masks, entity.identity, entity.bounds, reversed, view.samples, reused, depth_storage);
    view.stats.reused_targets += reused ? 1 : 0;
    if (mask == nullptr)
    {
        entity.packets.clear();
        targets_.ReleaseMasks(entity.masks, true);
        return false;
    }

    if (depth_storage == StudioEffectTargets::ObjectDepthStorage::Transient)
    {
        targets_.ClearCaptureDepth(*mask, view.viewport);
    }

    for (const StudioDrawPacket& packet : entity.packets)
    {
        for (const StudioQueuedMesh& mesh : packet.meshes)
        {
            const ncl_material::Material* material = mesh.material ? &*mesh.material : nullptr;
            const ncl_material::ProgramManifest* manifest = mesh.images.manifest;
            mask->IncludeStyle(
                effects::Scalar(material, manifest, StudioParameterRole::OutlineWidth),
                effects::Scalar(material, manifest, StudioParameterRole::GlowRadius),
                effects::Scalar(material, manifest, StudioParameterRole::GlowStrength)
            );
        }

        passes_.CaptureMask(*mask, packet, *program);
        passes_.CaptureDepth(*mask, packet, *program);
        ++view.stats.packets;
    }

    entity.packets.clear();

    return true;
}

void render::StudioEffects::FlushPendingCaptures()
{
    if (views_.empty() || !device_.IsContextCurrent())
    {
        return;
    }

    const bool pending = std::any_of(views_.begin(), views_.end(), [](const View& view) {
        return std::any_of(view.entities.begin(), view.entities.end(), [](const Entity& entity) {
            return !entity.packets.empty();
        });
    });
    if (!pending)
    {
        return;
    }

    ScopedGlState saved;
    for (View& view : views_)
    {
        for (Entity& entity : view.entities)
        {
            CaptureEntity(view, entity);
        }
    }
}

void render::StudioEffects::DiscardCaptures()
{
    for (View& view : views_)
    {
        ReleaseEntities(view, device_.IsContextCurrent());
        view.stats = {};
    }
}

void render::StudioEffects::EndView()
{
    if (views_.empty() || !device_.IsContextCurrent())
    {
        return;
    }

    View view = std::move(views_.back());
    views_.pop_back();
    if (view.entities.empty())
    {
        return;
    }

    GLint output = 0;
    GLint samples = 0;
    glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &output);
    glGetIntegerv(GL_SAMPLES, &samples);
    if (view.generation != device_.get_generation() || output != view.output || samples != view.samples)
    {
        ReleaseEntities(view, true);
        return;
    }

    ScopedGlState saved;
    targets_.EnsureGeometry();
    StudioEffectPasses::Composition composition;
    const bool outline = std::any_of(view.entities.begin(), view.entities.end(), [](const Entity& entity) {
        return entity.outline;
    });
    const bool distance = std::any_of(view.entities.begin(), view.entities.end(), [](const Entity& entity) {
        return entity.distance;
    });
    if (!passes_.PrepareComposition(view.viewport, outline, distance, composition))
    {
        ReleaseEntities(view, true);
        return;
    }

    view.stats.depth_format = targets_.get_scene_format();
    view.stats.samples = targets_.get_scene_samples();

    for (Entity& entity : view.entities)
    {
        const StudioEffectTargets::ObjectDepthStorage depth_storage = !entity.packets.empty() && entity.masks.empty()
                                                                          ? StudioEffectTargets::ObjectDepthStorage::Transient
                                                                          : StudioEffectTargets::ObjectDepthStorage::Retained;
        if (!CaptureEntity(view, entity, depth_storage))
        {
            continue;
        }

        const StudioEffectTargets::Mask& mask = *entity.masks.front();
        const StudioEffectTargets::ObjectDepthSource object_depth = targets_.ObjectDepthFor(mask, view.viewport);
        ++view.stats.entities;
        view.stats.mask_pixels += static_cast<uint64_t>(mask.get_rect()[2]) * mask.get_rect()[3];

        const effects::Rect_t area = passes_.InfluenceRect(mask, view.viewport);
        if (area[2] > 0 && area[3] > 0)
        {
            passes_.PrepareCompositionState();
            int seed_index = -1;
            if (mask.get_outline_width() > 0)
            {
                passes_.Seed(mask, object_depth, area, composition);
                seed_index = mask.get_outline_width() <= 2 && !mask.has_glow() ? 0 : passes_.PropagateSeeds(area, composition);
            }

            passes_.Composite(mask, object_depth, view.viewport, area, seed_index, composition);
        }

        targets_.RecycleWorkingMasks(entity.masks);
    }

    StudioEffectTargets::Masks_t empty;
    targets_.RecycleMasks(empty);
    last_stats_ = view.stats;
}

void render::StudioEffects::ReleaseEntities(View& view, bool release_gl)
{
    std::vector<Entity> entities = std::move(view.entities);
    for (Entity& entity : entities)
    {
        entity.packets.clear();
        targets_.ReleaseMasks(entity.masks, release_gl);
    }
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
        ReleaseEntities(view, release_gl);
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
    return views_.empty() ? 0 : views_.back().entities.size();
}
