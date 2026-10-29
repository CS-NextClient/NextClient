#include "Overview.h"

#include <algorithm>
#include <cstring>
#include <cstdlib>
#include <vector>

#include "main.h"

#include <strtools.h>
#include <ncl_gl/loader.h>
#include <goldsrc/client_api.h>
#include <goldsrc/engine/model.h>

#include "hud/radar/RadarLimits.h"
#include "hud/radar/map/layers/RadarLayerContext.h"
#include "hud/radar/integration/RadarUtil.h"

namespace
{
    constexpr float kRuntimeZoom = 0.85f;
} // namespace

void RadarOverview::ResetLayerImages(bool release)
{
    layer_image_ = nullptr;
    layer_renderer_.Reset(release);
    layer_materials_.clear();
    layer_gl_failed_ = false;
    layer_waiting_reason_.clear();
    runtime_failed_ = false;
}

void RadarOverview::Invalidate(bool release)
{
    map_sprite_ = nullptr;
    ResetLayerImages(release);
    layer_layout_ready_ = false;
    runtime_layout_ready_ = false;
    runtime_layers_ready_ = false;
    runtime_failed_ = false;
    if (release)
    {
        MapRender_Free(runtime_);
    }
    else
    {
        runtime_ = {};
    }
    data_ = overview_t{};
    loaded_map_[0] = '\0';
    loaded_mode_ = RadarMapMode::None;
}

RadarMapStatus RadarOverview::EnsureLoaded(RadarMapMode mode, const RadarMapLayers* layers, bool prepare_only)
{
    const char* level_name = gEngfuncs.pfnGetLevelName();

    if (!level_name || !level_name[0])
    {
        Invalidate();

        return RadarMapStatus::Waiting;
    }

    char map_name[64];
    RadarUtil_ExtractMapName(level_name, map_name);

    if (std::strcmp(map_name, loaded_map_) == 0 && mode == loaded_mode_)
    {
        if (runtime_failed_ && !RadarLayerContext_IsLayeredMode(mode))
        {
            return RadarMapStatus::Failed;
        }

        if (map_sprite_ || (runtime_.valid && !RadarLayerContext_IsLayeredMode(mode)) || layer_layout_ready_ ||
            (prepare_only && runtime_layout_ready_))
        {
            return RadarMapStatus::Ready;
        }
    }
    else
    {
        Invalidate();
    }

    if (RadarLayerContext_IsLayeredMode(mode))
    {
        V_strcpy_safe(loaded_map_, map_name);
        loaded_mode_ = mode;
        data_.zoom = kRuntimeZoom;

        if (!layers || layers->get_analysis().status != RadarMapStatus::Ready)
        {
            goldsrc::model_t* world = gEngfuncs.hudGetModelByIndex(1);

            if (world)
            {
                data_.origin[0] = (world->mins[0] + world->maxs[0]) * 0.5f;
                data_.origin[1] = (world->mins[1] + world->maxs[1]) * 0.5f;
            }

            return RadarMapStatus::Waiting;
        }

        ncl_math::Vector2 mid = layers->get_bounds().Center();
        data_.origin[0] = mid.x;
        data_.origin[1] = mid.y;
        layer_layout_ready_ = true;

        return RadarMapStatus::Ready;
    }

    bool use_overview = mode == RadarMapMode::OverviewTextured || mode == RadarMapMode::OverviewFlat;
    bool textured = mode == RadarMapMode::OverviewTextured || mode == RadarMapMode::RuntimeTextured || mode == RadarMapMode::SliceTextured;

    if (use_overview)
    {
        char info_path[128];
        Q_snprintf(info_path, sizeof(info_path), "overviews/%s.txt", map_name);

        if (ParseInfo(info_path) && data_.image[0])
        {
            map_sprite_ = gEngfuncs.LoadMapSprite(data_.image);

            if (map_sprite_)
            {
                V_strcpy_safe(loaded_map_, map_name);
                loaded_mode_ = mode;

                return RadarMapStatus::Ready;
            }
        }
    }

    // The world model exists before its surfaces are parsed on the first frames after a
    // level change; leave the map uncommitted and retry each frame until it is ready, so
    // the radar fills in by itself instead of needing a mode toggle.
    goldsrc::model_t* world = gEngfuncs.hudGetModelByIndex(1);

    if (!world || world->numsurfaces <= 0 || !world->surfaces)
    {
        return RadarMapStatus::Waiting;
    }

    data_ = overview_t{};
    data_.origin[0] = (world->mins[0] + world->maxs[0]) * 0.5f;
    data_.origin[1] = (world->mins[1] + world->maxs[1]) * 0.5f;
    data_.zoom = kRuntimeZoom;
    runtime_layout_ready_ = true;
    V_strcpy_safe(loaded_map_, map_name);
    loaded_mode_ = mode;

    if (prepare_only)
    {
        return RadarMapStatus::Ready;
    }

    RadarMapStatus status;
    runtime_ = MapRender_Generate(world, textured, false, &status);
    runtime_failed_ = status == RadarMapStatus::Failed;

    return status;
}

RadarMapStatus RadarOverview::EnsureTopImage(RadarMapMode mode, bool prepare_only, const RadarMapLayers* layers)
{
    layer_image_ = nullptr;
    data_.zoom = kRuntimeZoom;

    bool layers_ready = layers && layers->get_analysis().status == RadarMapStatus::Ready;
    if (layers_ready && !runtime_layers_ready_)
    {
        if (prepare_only)
        {
            return RadarMapStatus::Waiting;
        }

        MapRender_Free(runtime_);
        runtime_failed_ = false;
        runtime_layers_ready_ = true;
    }

    if (!runtime_.valid)
    {
        if (runtime_failed_)
        {
            return RadarMapStatus::Failed;
        }

        if (prepare_only)
        {
            return RadarMapStatus::Waiting;
        }

        goldsrc::model_t* world = gEngfuncs.hudGetModelByIndex(1);
        RadarMapStatus status;
        bool textured = RadarLayerContext_TopMode(mode) == RadarMapMode::RuntimeTextured;
        std::span<const int> excluded =
            layers_ready ? std::span<const int>{layers->get_analysis().reachability.excluded_surfaces} : std::span<const int>{};
        runtime_ = MapRender_Generate(world, textured, false, &status, excluded);
        runtime_failed_ = status == RadarMapStatus::Failed;

        if (status != RadarMapStatus::Ready)
        {
            return status;
        }

    }

    ncl_math::Vector2 center = runtime_.bounds.Center();
    data_.origin[0] = center.x;
    data_.origin[1] = center.y;

    return RadarMapStatus::Ready;
}

RadarMapStatus RadarOverview::EnsureLayerImage(const RadarMapLayers& layers, int layer, uint64_t frame)
{
    layer_image_ = nullptr;
    layer_waiting_reason_.clear();
    ncl_math::Vector2 center = layers.get_bounds().Center();
    data_.origin[0] = center.x;
    data_.origin[1] = center.y;

    if (!ncl_gl::EnsureLoaded())
    {
        layer_gl_failed_ = true;

        return RadarMapStatus::Failed;
    }

    bool textured = loaded_mode_ == RadarMapMode::SliceTextured;
    std::span<const MapLayerMaterial> materials;

    if (textured && !layer_renderer_.is_cached(layers, layer, textured))
    {
        goldsrc::model_t* world = gEngfuncs.hudGetModelByIndex(1);

        if (!world || !world->surfaces || world->numsurfaces <= 0)
        {
            layer_waiting_reason_ = "stage=image_world layer=" + std::to_string(layer) + " world=" + (world ? "present" : "missing") +
                                    " surfaces=" + (world && world->surfaces ? "present" : "missing") +
                                    " surface_count=" + std::to_string(world ? world->numsurfaces : 0);

            return RadarMapStatus::Waiting;
        }

        layer_materials_.assign((size_t)world->numsurfaces, {});
        const int (*lightmap_textures)[64] = eng()->lightmap_textures;

        for (int i = 0; i < world->numsurfaces; i++)
        {
            const goldsrc::msurface_t& surface = world->surfaces[i];
            MapLayerMaterial& material = layer_materials_[i];

            if (surface.texinfo && surface.texinfo->texture)
            {
                material.base_texture = surface.texinfo->texture->gl_texturenum;
            }

            material.needs_lightmap = (surface.flags & (goldsrc::SURF_DRAWSKY | goldsrc::SURF_DRAWTILED)) == 0;

            if (lightmap_textures && surface.lightmaptexturenum >= 0 && (size_t)surface.lightmaptexturenum < std::size(*lightmap_textures))
            {
                material.lightmap_texture = (*lightmap_textures)[surface.lightmaptexturenum];
            }
        }

        materials = layer_materials_;
    }

    RadarMapStatus status = layer_renderer_.Ensure(layers, materials, layer, textured, frame, &layer_waiting_reason_);

    if (status == RadarMapStatus::Ready)
    {
        layer_image_ = layer_renderer_.get_map(layer);
    }

    return status;
}

char* RadarOverview::ParseGlobalBlock(char* parse_pos)
{
    char token[kComParseTokenSize];

    parse_pos = gEngfuncs.COM_ParseFile(parse_pos, token);

    if (!parse_pos || strcmp(token, "{"))
    {
        return nullptr;
    }

    while (true)
    {
        parse_pos = gEngfuncs.COM_ParseFile(parse_pos, token);

        if (!parse_pos)
        {
            return nullptr;
        }

        if (!stricmp(token, "zoom"))
        {
            parse_pos = gEngfuncs.COM_ParseFile(parse_pos, token);
            data_.zoom = (float)atof(token);
        }
        else if (!stricmp(token, "origin"))
        {
            parse_pos = gEngfuncs.COM_ParseFile(parse_pos, token);
            data_.origin[0] = (float)atof(token);
            parse_pos = gEngfuncs.COM_ParseFile(parse_pos, token);
            data_.origin[1] = (float)atof(token);
            parse_pos = gEngfuncs.COM_ParseFile(parse_pos, token);
            data_.origin[2] = (float)atof(token);
        }
        else if (!stricmp(token, "rotated"))
        {
            parse_pos = gEngfuncs.COM_ParseFile(parse_pos, token);
            data_.rotated = atoi(token) != 0;
        }
        else if (!stricmp(token, "}"))
        {
            return parse_pos;
        }
        else
        {
            // Consume the value of an unknown key so custom or future overview keys do
            // not abort the parse; the layer block deliberately rejects them instead.
            parse_pos = gEngfuncs.COM_ParseFile(parse_pos, token);
        }
    }
}

char* RadarOverview::ParseLayerBlock(char* parse_pos)
{
    char token[kComParseTokenSize];

    parse_pos = gEngfuncs.COM_ParseFile(parse_pos, token);

    if (!parse_pos || strcmp(token, "{"))
    {
        return nullptr;
    }

    while (true)
    {
        parse_pos = gEngfuncs.COM_ParseFile(parse_pos, token);

        if (!parse_pos)
        {
            return nullptr;
        }

        if (!stricmp(token, "image"))
        {
            parse_pos = gEngfuncs.COM_ParseFile(parse_pos, token);
            V_strcpy_safe(data_.image, token);
        }
        else if (!stricmp(token, "height"))
        {
            parse_pos = gEngfuncs.COM_ParseFile(parse_pos, token);
        }
        else if (!stricmp(token, "}"))
        {
            return parse_pos;
        }
        else
        {
            return nullptr;
        }
    }
}

bool RadarOverview::ParseOverviewBody(char* parse_pos)
{
    char token[kComParseTokenSize];

    while (true)
    {
        parse_pos = gEngfuncs.COM_ParseFile(parse_pos, token);

        if (!parse_pos)
        {
            break;
        }

        if (!stricmp(token, "global"))
        {
            parse_pos = ParseGlobalBlock(parse_pos);
        }
        else if (!stricmp(token, "layer"))
        {
            parse_pos = ParseLayerBlock(parse_pos);
        }
        else
        {
            return false;
        }

        if (!parse_pos)
        {
            return false;
        }
    }

    // A missing or unparsable zoom key leaves zoom at 0, which collapses every marker
    // onto the center.
    if (data_.zoom <= 0.0f)
    {
        data_.zoom = 1.0f;
    }

    return true;
}

bool RadarOverview::ParseInfo(const char* file_name)
{
    int length = 0;
    char* buffer = reinterpret_cast<char*>(gEngfuncs.COM_LoadFile(const_cast<char*>(file_name), 5, &length));

    if (!buffer)
    {
        return false;
    }

    bool ok = ParseOverviewBody(buffer);
    gEngfuncs.COM_FreeFile(buffer);

    return ok;
}
