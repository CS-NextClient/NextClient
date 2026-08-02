#include "Overview.h"
#include "RadarUtil.h"
#include "../../main.h"

#include <strtools.h>

#include <algorithm>
#include <cstring>
#include <cstdlib>

namespace
{
    constexpr float kRuntimeZoom = 0.85f; // zoom baked into the runtime-rendered BSP map
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
    RadarUtil_ExtractMapName(level_name, map_name, sizeof(map_name));

    if (std::strcmp(map_name, loaded_map_) == 0 && mode == loaded_mode_)
    {
        return;
    }

    // Also clears loaded_map_/loaded_mode_, so a not-ready retry path (below) that
    // returns early keeps being re-entered instead of matching the compare above.
    Invalidate();

    bool use_overview = (mode == RadarMapMode::OverviewTextured || mode == RadarMapMode::OverviewFlat);
    bool textured =
        (mode == RadarMapMode::OverviewTextured || mode == RadarMapMode::RuntimeTextured || mode == RadarMapMode::SliceTextured);
    bool slice = (mode == RadarMapMode::SliceTextured || mode == RadarMapMode::SliceFlat);

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

    // Runtime image: a top-down render of the BSP (textured or flat-shaded). It is
    // the fallback for modes 1/2 and the only source for modes 3/4/5/6. The world
    // model exists before its surfaces are parsed on the first frames after a level
    // change; leave the map uncommitted and retry each frame until it is ready, so
    // the radar fills in by itself instead of needing a mode toggle.
    model_t* world = gEngfuncs.hudGetModelByIndex(1);

    if (!world || world->numsurfaces <= 0 || !world->surfaces)
    {
        return;
    }

    data_ = overview_t{};
    runtime_ = MapRender_Generate(world, textured, slice);

    if (!runtime_.valid)
    {
        return; // not ready yet (e.g. lightmap atlas still building): retry next frame
    }

    ncl_math::Vector2 mid = runtime_.bounds.Center();
    data_.origin[0] = mid.x;
    data_.origin[1] = mid.y;
    data_.zoom = kRuntimeZoom;

    V_strcpy_safe(loaded_map_, map_name);
    loaded_mode_ = mode;
}

bool RadarOverview::ParseInfo(const char* file_name)
{
    int length = 0;
    char* buffer = reinterpret_cast<char*>(gEngfuncs.COM_LoadFile(const_cast<char*>(file_name), 5, &length));

    if (!buffer)
    {
        return false;
    }

    char* parse_pos = buffer;
    char token[2048]; // must match the engine COM_ParseFile token buffer size
    bool ok = false;

    while (true)
    {
        parse_pos = gEngfuncs.COM_ParseFile(parse_pos, token);

        if (!parse_pos)
        {
            break;
        }

        if (!stricmp(token, "global"))
        {
            parse_pos = gEngfuncs.COM_ParseFile(parse_pos, token);

            if (!parse_pos || strcmp(token, "{"))
            {
                goto done;
            }

            while (true)
            {
                parse_pos = gEngfuncs.COM_ParseFile(parse_pos, token);

                if (!parse_pos)
                {
                    goto done;
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
                    break;
                }
                else
                {
                    // Tolerate unknown keys (consume their value) so custom or
                    // future overview keys don't abort the whole parse.
                    parse_pos = gEngfuncs.COM_ParseFile(parse_pos, token);
                }
            }
        }
        else if (!stricmp(token, "layer"))
        {
            parse_pos = gEngfuncs.COM_ParseFile(parse_pos, token);

            if (!parse_pos || strcmp(token, "{"))
            {
                goto done;
            }

            while (true)
            {
                parse_pos = gEngfuncs.COM_ParseFile(parse_pos, token);

                if (!parse_pos)
                {
                    goto done;
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
                    break;
                }
                else
                {
                    goto done;
                }
            }
        }
        else
        {
            goto done;
        }
    }

    // Stock default; also covers files with a missing or unparsable zoom key, which
    // would otherwise leave zoom at 0 and collapse every marker onto the centre.
    if (data_.zoom <= 0.0f)
    {
        data_.zoom = 1.0f;
    }

    ok = true;

done:
    gEngfuncs.COM_FreeFile(buffer);
    return ok;
}
