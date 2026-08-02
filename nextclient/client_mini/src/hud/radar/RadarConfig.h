#pragma once

typedef struct cvar_s cvar_t;

struct RadarConfig
{
    cvar_t* mode; // hud_radar_mode: 0 classic, 1 advanced
    cvar_t* scale; // hud_radar_scale: on-screen radar size
    cvar_t* map_scale; // hud_radar_map_scale: map zoom inside radar
    cvar_t* opacity; // hud_radar_opacity: 0..100
    cvar_t* round;
    cvar_t* fullmap;
    cvar_t* rotate;
    cvar_t* names;
    cvar_t* health;
    cvar_t* tracks;
    cvar_t* objectives;
    cvar_t* map_mode; // hud_radar_map_mode: 1 ovr+tex, 2 ovr+flat, 3 tex, 4 flat, 5 tex slice, 6 flat slice
    cvar_t* map_mask; // hud_radar_map_mask: diagnostic toggle for the map mask shader

    cvar_t* bg; // hud_radar_bg: 0 black, 1 blurred backdrop

    // Hidden tuning cvars (not exposed in the settings dialog).
    cvar_t* health_size;
    cvar_t* name_size;
    cvar_t* allies; // hud_radar_allies: 0 forces teammates through the line-of-sight gate
};

extern RadarConfig g_RadarCfg;

void RadarConfig_Init();

// True once cvars are registered and hud_radar_mode selects the advanced radar.
bool RadarConfig_AdvancedSelected();

// Like RadarConfig_AdvancedSelected, but also false when the GL shader path the
// advanced radar needs is unavailable, so the caller draws the classic radar instead.
bool RadarConfig_AdvancedActive();
