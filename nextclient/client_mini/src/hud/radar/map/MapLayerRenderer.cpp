#include "MapRender.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <limits>
#include <sstream>
#include <utility>
#include <vector>

#include <glad/glad.h>
#include <ncl_math/scalar.h>

#include "hud/radar/map/MapImageTone.h"
#include "hud/radar/map/MapRenderState.h"
#include "hud/radar/integration/Performance.h"

namespace
{
    constexpr float kMinimumHeightSpan = 1.0f;
    constexpr float kLowerLayerBrightness = 0.6f;

    std::string GlFailure(const char* operation, const char* code_name, GLenum code)
    {
        std::ostringstream reason;
        reason << operation << " (" << code_name << "=0x" << std::hex << code << ')';

        return reason.str();
    }

    RadarMapStatus Wait(std::string* reason, std::string message)
    {
        if (reason)
        {
            *reason = std::move(message);
        }

        return RadarMapStatus::Waiting;
    }

    bool AreMaterialsReady(
        std::span<const RadarMapSurface> surfaces,
        std::span<const MapLayerMaterial> materials,
        int layer,
        std::string* reason
    )
    {
        for (const RadarMapSurface& surface : surfaces)
        {
            if (surface.surface_id < 0 || (size_t)surface.surface_id >= materials.size())
            {
                Wait(
                    reason,
                    "stage=material_index layer=" + std::to_string(layer) + " surface=" + std::to_string(surface.surface_id) +
                        " material_count=" + std::to_string(materials.size())
                );

                return false;
            }

            const MapLayerMaterial& material = materials[surface.surface_id];
            bool base_valid = glIsTexture(material.base_texture) != GL_FALSE;
            bool lightmap_checked = base_valid && material.needs_lightmap;
            bool lightmap_valid = !material.needs_lightmap || (lightmap_checked && glIsTexture(material.lightmap_texture) != GL_FALSE);

            if (!base_valid || !lightmap_valid)
            {
                if (reason)
                {
                    const char* lightmap_state = "unchecked";

                    if (lightmap_checked)
                    {
                        lightmap_state = lightmap_valid ? "1" : "0";
                    }

                    std::ostringstream message;
                    message << "stage=material_texture layer=" << layer << " surface=" << surface.surface_id
                            << " material=" << surface.surface_id << " sky=" << surface.sky << " tiled=" << surface.tiled
                            << " base_texture=" << material.base_texture << " lightmap_texture=" << material.lightmap_texture
                            << " needs_lightmap=" << material.needs_lightmap << " base_is_texture=" << base_valid
                            << " lightmap_is_texture=" << lightmap_state << " failed_texture=" << (base_valid ? "lightmap" : "base");
                    *reason = message.str();
                }

                return false;
            }
        }

        return true;
    }

    void BeginPass(const ncl_math::Aabb2& bounds, float min_z, float max_z, bool clear_color)
    {
        glUseProgram(0);
        glViewport(0, 0, kMapImageSize, kMapImageSize);

        glDisable(GL_BLEND);
        glDisable(GL_ALPHA_TEST);
        glDisable(GL_CULL_FACE);
        glDisable(GL_SCISSOR_TEST);
        glDisable(GL_LIGHTING);
        glDisable(GL_FOG);

        glEnable(GL_DEPTH_TEST);
        glDepthMask(GL_TRUE);
        glDepthFunc(GL_LEQUAL);
        glClearDepth(1.0);

        glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
        glShadeModel(GL_SMOOTH);

        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();
        glOrtho(bounds.min.x, bounds.max.x, bounds.min.y, bounds.max.y, -max_z, -min_z);

        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();

        glClearColor(0, 0, 0, 0);
        // Each floor keeps its own top-down depth ordering while nearer floors replace farther ones.
        glClear(GL_DEPTH_BUFFER_BIT | (clear_color ? GL_COLOR_BUFFER_BIT : 0));
    }

    void DrawMesh(
        std::span<const RadarMapSurface> surfaces,
        std::span<const MapLayerMaterial> materials,
        bool textured,
        float min_z,
        float span_z,
        float brightness
    )
    {
        for (const RadarMapSurface& surface : surfaces)
        {
            glActiveTexture(GL_TEXTURE1);

            if (textured && materials[surface.surface_id].needs_lightmap)
            {
                glEnable(GL_TEXTURE_2D);
                glBindTexture(GL_TEXTURE_2D, materials[surface.surface_id].lightmap_texture);
                glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
            }
            else
            {
                glDisable(GL_TEXTURE_2D);
            }

            glActiveTexture(GL_TEXTURE0);

            if (textured)
            {
                glEnable(GL_TEXTURE_2D);
                glBindTexture(GL_TEXTURE_2D, materials[surface.surface_id].base_texture);
                glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
            }
            else
            {
                glDisable(GL_TEXTURE_2D);
            }

            glBegin(GL_POLYGON);

            for (const RadarMapVertex& vertex : surface.vertices)
            {
                float shade = ncl_math::Saturate((vertex.pos.z - min_z) / span_z);
                float color =
                    (textured ? ncl_math::Lerp(0.55f, 1.0f, shade) : ncl_math::Lerp(kMapFlatMinBrightness, kMapFlatMaxBrightness, shade)) *
                    brightness;
                float red_blue = textured ? color : color * 0.82f;

                glColor4f(red_blue, color, red_blue, 1);

                if (textured)
                {
                    glMultiTexCoord2f(GL_TEXTURE0, vertex.uv.x, vertex.uv.y);
                    glMultiTexCoord2f(GL_TEXTURE1, vertex.lightmap_uv.x, vertex.lightmap_uv.y);
                }

                glVertex3f(vertex.pos.x, vertex.pos.y, vertex.pos.z);
            }

            glEnd();
        }
    }
} // namespace

RadarMapStatus MapLayerRenderer::Ensure(
    const RadarMapLayers& layers,
    std::span<const MapLayerMaterial> materials,
    int layer,
    bool textured,
    uint64_t frame,
    std::string* waiting_reason
)
{
    performance::PerformanceMonitorInterface* monitor = performance_.get_monitor();
    performance::PerformanceCpuScope cpu(monitor, performance::PerfStage::RadarLayer);
    if (waiting_reason)
    {
        waiting_reason->clear();
    }

    if (layers_ != &layers || textured_ != textured)
    {
        Reset();
        layers_ = &layers;
        textured_ = textured;
    }

    if (failed_)
    {
        return RadarMapStatus::Failed;
    }

    if (get_map(layer))
    {
        performance_.AddCounter(performance::PerfCounterId::RadarLayerCacheHits, 1);
        cache_.Request(layer, frame);

        return RadarMapStatus::Ready;
    }

    std::span<const RadarMapSurface> surfaces = layers.get_surfaces(layer);

    std::span<const RadarLayerInfo> available_layers = layers.get_layers();
    bool layer_exists =
        std::any_of(available_layers.begin(), available_layers.end(), [layer](const RadarLayerInfo& info) { return info.id == layer; });
    bool has_mesh = std::any_of(available_layers.begin(), available_layers.end(), [&layers](const RadarLayerInfo& info) {
        return !layers.get_surfaces(info.id).empty();
    });
    if (!layer_exists || !has_mesh)
    {
        return Wait(waiting_reason, "stage=layer_mesh layer=" + std::to_string(layer) + " surfaces=0");
    }

    if (!glGenFramebuffers)
    {
        return Fail(layers, textured, "glGenFramebuffers unavailable");
    }

    if (!glBindFramebuffer)
    {
        return Fail(layers, textured, "glBindFramebuffer unavailable");
    }

    if (!glGenRenderbuffers)
    {
        return Fail(layers, textured, "glGenRenderbuffers unavailable");
    }

    if (!glCheckFramebufferStatus)
    {
        return Fail(layers, textured, "glCheckFramebufferStatus unavailable");
    }

    if (textured)
    {
        if (!AreMaterialsReady(surfaces, materials, layer, waiting_reason))
        {
            return RadarMapStatus::Waiting;
        }
        for (const RadarLayerInfo& info : layers.get_layers())
        {
            if (info.id != layer && !AreMaterialsReady(layers.get_surfaces(info.id), materials, info.id, waiting_reason))
            {
                return RadarMapStatus::Waiting;
            }
        }
    }

    MapLayerRequest request = cache_.Request(layer, frame);

    if (!request.render)
    {
        return Wait(waiting_reason, "stage=image_budget layer=" + std::to_string(layer) + " frame=" + std::to_string(frame));
    }

    performance::PerformanceGpuScope gpu(monitor, performance::PerfStage::RadarLayer);
    MapRenderState saved;
    RuntimeMap& image = maps_[request.slot];

    if (image.texture)
    {
        glDeleteTextures(1, &image.texture);
        image = {};
    }

    glActiveTexture(GL_TEXTURE0);
    glGenTextures(1, &image.texture);
    glBindTexture(GL_TEXTURE_2D, image.texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, kMapImageSize, kMapImageSize, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    if (!image.texture)
    {
        return Fail(layers, textured, "texture allocation returned zero");
    }

    GLenum error = glGetError();

    if (error != GL_NO_ERROR)
    {
        return Fail(layers, textured, GlFailure("texture storage failed", "GL error", error));
    }

    if (!depth_buffer_)
    {
        glGenRenderbuffers(1, &depth_buffer_);
        glBindRenderbuffer(GL_RENDERBUFFER, depth_buffer_);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, kMapImageSize, kMapImageSize);

        if (!depth_buffer_)
        {
            return Fail(layers, textured, "depth buffer allocation returned zero");
        }

        error = glGetError();

        if (error != GL_NO_ERROR)
        {
            return Fail(layers, textured, GlFailure("depth buffer storage failed", "GL error", error));
        }
    }

    if (!framebuffer_)
    {
        glGenFramebuffers(1, &framebuffer_);

        if (!framebuffer_)
        {
            return Fail(layers, textured, "framebuffer allocation returned zero");
        }
    }

    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer_);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, image.texture, 0);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depth_buffer_);
    GLenum framebuffer_status = glCheckFramebufferStatus(GL_FRAMEBUFFER);

    if (framebuffer_status != GL_FRAMEBUFFER_COMPLETE)
    {
        return Fail(layers, textured, GlFailure("framebuffer incomplete", "status", framebuffer_status));
    }

    error = glGetError();

    if (error != GL_NO_ERROR)
    {
        return Fail(layers, textured, GlFailure("framebuffer attachment failed", "GL error", error));
    }

    std::vector<RadarLayerInfo> draw_layers(layers.get_layers().begin(), layers.get_layers().end());
    auto active = std::find_if(draw_layers.begin(), draw_layers.end(), [layer](const RadarLayerInfo& value) { return value.id == layer; });
    assert(active != draw_layers.end());
    float active_height = active->height;
    std::sort(draw_layers.begin(), draw_layers.end(), [layer, active_height](const RadarLayerInfo& first, const RadarLayerInfo& second) {
        if (first.id == layer || second.id == layer)
        {
            return second.id == layer && first.id != layer;
        }
        float first_distance = std::abs(first.height - active_height);
        float second_distance = std::abs(second.height - active_height);
        if (first_distance != second_distance)
        {
            return first_distance > second_distance;
        }
        return first.height > second.height;
    });

    bool clear_color = true;
    const RadarLayerAnalysis& analysis = layers.get_analysis();
    for (const RadarLayerInfo& info : draw_layers)
    {
        std::span<const RadarMapSurface> mesh = layers.get_surfaces(info.id);
        if (mesh.empty())
        {
            continue;
        }
        float min_z = std::numeric_limits<float>::max();
        float max_z = std::numeric_limits<float>::lowest();
        for (const RadarMapSurface& surface : mesh)
        {
            for (const RadarMapVertex& vertex : surface.vertices)
            {
                min_z = std::min(min_z, vertex.pos.z);
                max_z = std::max(max_z, vertex.pos.z);
            }
        }
        max_z = std::max(max_z, min_z + kMinimumHeightSpan);
        BeginPass(layers.get_bounds(), min_z, max_z, clear_color);
        clear_color = false;
        auto group = std::find_if(analysis.layers.begin(), analysis.layers.end(), [&info](const RadarLayerGroupAnalysis& value) {
            return value.layer == info.id;
        });
        assert(group != analysis.layers.end());
        float shade_span = std::max(kMinimumHeightSpan, group->max_height - group->min_height);
        float brightness = info.height < active_height ? kLowerLayerBrightness : 1.0f;
        DrawMesh(mesh, materials, textured, group->min_height, shade_span, brightness);
    }

    MapImageTone_PremultiplyAlpha();

    if (textured)
    {
        MapImageTone_BrightenTextured();
    }

    error = glGetError();

    if (error != GL_NO_ERROR)
    {
        return Fail(layers, textured, GlFailure("layer mesh rendering failed", "GL error", error));
    }

    image.bounds = layers.get_bounds();
    image.valid = true;
    cache_.Commit(request.slot, layer);
    performance_.AddCounter(performance::PerfCounterId::RadarLayerImages, 1);

    return RadarMapStatus::Ready;
}

RadarMapStatus MapLayerRenderer::Fail(const RadarMapLayers& layers, bool textured, std::string reason)
{
    Reset();
    layers_ = &layers;
    textured_ = textured;
    failed_ = true;
    failure_reason_ = std::move(reason);

    return RadarMapStatus::Failed;
}

const std::string& MapLayerRenderer::get_failure_reason() const
{
    return failure_reason_;
}

bool MapLayerRenderer::is_cached(const RadarMapLayers& layers, int layer, bool textured) const
{
    return layers_ == &layers && textured_ == textured && get_map(layer) != nullptr;
}

const RuntimeMap* MapLayerRenderer::get_map(int layer) const
{
    std::span<const MapLayerEntry> entries = cache_.get_entries();

    for (size_t i = 0; i < entries.size(); i++)
    {
        if (entries[i].valid && entries[i].layer == layer)
        {
            return &maps_[i];
        }
    }

    return nullptr;
}

const MapLayerCache& MapLayerRenderer::get_cache() const
{
    return cache_;
}

void MapLayerRenderer::Reset(bool release)
{
    for (RuntimeMap& map : maps_)
    {
        if (release && map.texture)
        {
            glDeleteTextures(1, &map.texture);
        }

        map = {};
    }

    if (release && framebuffer_)
    {
        glDeleteFramebuffers(1, &framebuffer_);
    }

    if (release && depth_buffer_)
    {
        glDeleteRenderbuffers(1, &depth_buffer_);
    }

    framebuffer_ = 0;
    depth_buffer_ = 0;
    cache_.Reset();
    layers_ = nullptr;
    textured_ = false;
    failed_ = false;
    failure_reason_.clear();
}
