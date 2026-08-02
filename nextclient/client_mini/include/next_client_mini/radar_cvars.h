#pragma once

// Single source of truth for the advanced-radar cvar definitions: the client_mini
// registration and the gameui settings dialog (defaults, slider ranges, clamps)
// all read this table, so names, defaults and ranges cannot drift apart.

struct RadarCvarDef
{
    const char* name;
    const char* default_value;
    float min_value;
    float max_value;
};

inline constexpr RadarCvarDef kRadarCvarDefs[] = {
    {"hud_radar_mode", "1", 0.0f, 1.0f},
    {"hud_radar_scale", "1.0", 0.5f, 3.0f},
    {"hud_radar_map_scale", "1.0", 0.5f, 3.5f},
    {"hud_radar_opacity", "100", 0.0f, 100.0f},
    {"hud_radar_round", "1", 0.0f, 1.0f},
    {"hud_radar_fullmap", "0", 0.0f, 1.0f},
    {"hud_radar_rotate", "1", 0.0f, 1.0f},
    {"hud_radar_names", "0", 0.0f, 1.0f},
    {"hud_radar_health", "0", 0.0f, 1.0f},
    {"hud_radar_tracks", "0", 0.0f, 1.0f},
    {"hud_radar_objectives", "1", 0.0f, 1.0f},
    {"hud_radar_map_mode", "1", 1.0f, 6.0f},
    {"hud_radar_map_mask", "1", 0.0f, 1.0f},
    {"hud_radar_bg", "0", 0.0f, 1.0f},
    {"hud_radar_health_size", "1.0", 0.3f, 4.0f},
    {"hud_radar_name_size", "1.0", 0.6f, 2.0f},
    // 1: show the teammates the server reveals (it withholds them in free-for-all
    // modes), 0: always gate teammates by line of sight, like enemies. Only ever
    // tightens what the radar shows, so it is a diagnostic switch rather than a setting.
    {"hud_radar_allies", "1", 0.0f, 1.0f},
};

// Lookup by cvar name; a miss returns a def with a null name. In a constexpr
// context pair it with a static_assert(def.name != nullptr) to catch typos at
// compile time.
constexpr RadarCvarDef RadarCvarDef_Get(const char* name)
{
    for (const RadarCvarDef& def : kRadarCvarDefs)
    {
        const char* a = def.name;
        const char* b = name;

        while (*a && *a == *b)
        {
            a++;
            b++;
        }

        if (*a == *b)
        {
            return def;
        }
    }

    return RadarCvarDef{};
}
