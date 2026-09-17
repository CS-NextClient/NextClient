#pragma once

#include <goldsrc/fwd.h>

// Each field is the cvar hud_radar_<field name>.
struct RadarConfig
{
    goldsrc::cvar_t* mode; // 0 classic, 1 advanced
    goldsrc::cvar_t* scale;
    goldsrc::cvar_t* map_scale;
    goldsrc::cvar_t* opacity; // 0..100
    goldsrc::cvar_t* round;
    goldsrc::cvar_t* fullmap;
    goldsrc::cvar_t* rotate;
    goldsrc::cvar_t* names;
    goldsrc::cvar_t* health;
    goldsrc::cvar_t* tracks;
    goldsrc::cvar_t* map_mode; // 1 ovr+tex, 2 ovr+flat, 3 tex, 4 flat, 5 tex slice, 6 flat slice
    goldsrc::cvar_t* map_mask;

    goldsrc::cvar_t* bg; // 0 black, 1 blurred backdrop

    goldsrc::cvar_t* health_size;
    goldsrc::cvar_t* name_size;
    goldsrc::cvar_t* allies; // 0 forces teammates through the line-of-sight gate
};

extern RadarConfig g_RadarCfg;

void RadarConfig_Init();

bool RadarConfig_IsAdvancedSelected();

// As RadarConfig_IsAdvancedSelected, but also false when the GL shader path the advanced
// radar needs is unavailable.
bool RadarConfig_CheckAdvancedActive();
