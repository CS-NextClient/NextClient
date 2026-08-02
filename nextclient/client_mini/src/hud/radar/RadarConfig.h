#pragma once

typedef struct cvar_s cvar_t;

// Each field is the cvar hud_radar_<field name>.
struct RadarConfig
{
    cvar_t* mode; // 0 classic, 1 advanced
    cvar_t* scale;
    cvar_t* map_scale;
    cvar_t* opacity; // 0..100
    cvar_t* round;
    cvar_t* fullmap;
    cvar_t* rotate;
    cvar_t* names;
    cvar_t* health;
    cvar_t* tracks;
    cvar_t* map_mode; // 1 ovr+tex, 2 ovr+flat, 3 tex, 4 flat, 5 tex slice, 6 flat slice
    cvar_t* map_mask;

    cvar_t* bg; // 0 black, 1 blurred backdrop

    cvar_t* health_size;
    cvar_t* name_size;
    cvar_t* allies; // 0 forces teammates through the line-of-sight gate
};

extern RadarConfig g_RadarCfg;

void RadarConfig_Init();

bool RadarConfig_IsAdvancedSelected();

// As RadarConfig_IsAdvancedSelected, but also false when the GL shader path the advanced
// radar needs is unavailable.
bool RadarConfig_CheckAdvancedActive();
