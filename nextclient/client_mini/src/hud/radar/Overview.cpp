#include "Overview.h"

#include <algorithm>
#include <cstring>
#include <cstdlib>

#include "main.h"

#include <strtools.h>
#include <goldsrc/client_api.h>
#include <goldsrc/engine/model.h>

#include "RadarLimits.h"
#include "RadarUtil.h"

namespace
{
    constexpr float kRuntimeZoom = 0.85f;
} // namespace

RadarMapMode RadarMapMode_FromCvar(float value)
{
    int mode = std::clamp(
        static_cast<int>(value), static_cast<int>(RadarMapMode::OverviewTextured), static_cast<int>(RadarMapMode::SliceFlat)
    );

    return static_cast<RadarMapMode>(mode);
}

void RadarOverview::Invalidate()
{
    map_sprite_ = nullptr;
    MapRender_Free(runtime_);
    data_ = overview_t{};
    loaded_map_[0] = '\0';
    loaded_mode_ = RadarMapMode::None;
}

void RadarOverview::EnsureLoaded(RadarMapMode mode)
{
    const char* level_name = gEngfuncs.pfnGetLevelName();

    if (!level_name || !level_name[0])
    {
        Invalidate();
        return;
    }

    char map_name[64];
    RadarUtil_ExtractMapName(level_name, map_name);

    if (std::strcmp(map_name, loaded_map_) == 0 && mode == loaded_mode_)
    {
        return;
    }

    // Also clears loaded_map_/loaded_mode_, so the not-ready early returns below keep
    // being re-entered instead of matching the compare above.
    Invalidate();

    bool use_overview = mode == RadarMapMode::OverviewTextured || mode == RadarMapMode::OverviewFlat;
    bool textured = mode == RadarMapMode::OverviewTextured || mode == RadarMapMode::RuntimeTextured || mode == RadarMapMode::SliceTextured;
    bool slice = mode == RadarMapMode::SliceTextured || mode == RadarMapMode::SliceFlat;

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
                return;
            }
        }
    }

    // The world model exists before its surfaces are parsed on the first frames after a
    // level change; leave the map uncommitted and retry each frame until it is ready, so
    // the radar fills in by itself instead of needing a mode toggle.
    goldsrc::model_t* world = gEngfuncs.hudGetModelByIndex(1);

    if (!world || world->numsurfaces <= 0 || !world->surfaces)
    {
        return;
    }

    data_ = overview_t{};
    runtime_ = MapRender_Generate(world, textured, slice);

    if (!runtime_.valid)
    {
        return;
    }

    ncl_math::Vector2 mid = runtime_.bounds.Center();
    data_.origin[0] = mid.x;
    data_.origin[1] = mid.y;
    data_.zoom = kRuntimeZoom;

    V_strcpy_safe(loaded_map_, map_name);
    loaded_mode_ = mode;
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
    // onto the centre.
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
