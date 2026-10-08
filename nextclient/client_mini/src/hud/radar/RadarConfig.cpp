#include "RadarConfig.h"

#include "main.h"

#include <next_client_mini/radar_cvars.h>
#include <goldsrc/client_api.h>
#include <goldsrc/cvar.h>

#include "RadarDraw.h"

RadarConfig g_RadarCfg;

namespace
{
    // A name missing from the shared table registers a null name and crashes loudly at
    // startup, instead of silently registering a wrong default.
    goldsrc::cvar_t* RegisterCvar(const char* name)
    {
        RadarCvarDef def = RadarCvarDef_Get(name);
        return gEngfuncs.pfnRegisterVariable(const_cast<char*>(def.name), const_cast<char*>(def.default_value), goldsrc::FCVAR_ARCHIVE);
    }
} // namespace

void RadarConfig_Init()
{
    g_RadarCfg.mode = RegisterCvar("hud_radar_mode");
    g_RadarCfg.scale = RegisterCvar("hud_radar_scale");
    g_RadarCfg.map_scale = RegisterCvar("hud_radar_map_scale");
    g_RadarCfg.opacity = RegisterCvar("hud_radar_opacity");
    g_RadarCfg.round = RegisterCvar("hud_radar_round");
    g_RadarCfg.fullmap = RegisterCvar("hud_radar_fullmap");
    g_RadarCfg.shift_scoreboard = RegisterCvar("hud_radar_shift_scoreboard");
    g_RadarCfg.rotate = RegisterCvar("hud_radar_rotate");
    g_RadarCfg.names = RegisterCvar("hud_radar_names");
    g_RadarCfg.health = RegisterCvar("hud_radar_health");
    g_RadarCfg.tracks = RegisterCvar("hud_radar_tracks");
    g_RadarCfg.map_mode = RegisterCvar("hud_radar_map_mode");
    g_RadarCfg.map_mask = RegisterCvar("hud_radar_map_mask");
    g_RadarCfg.bg = RegisterCvar("hud_radar_bg");

    g_RadarCfg.health_size = RegisterCvar("hud_radar_health_size");
    g_RadarCfg.name_size = RegisterCvar("hud_radar_name_size");
    g_RadarCfg.allies = RegisterCvar("hud_radar_allies");
}

bool RadarConfig_IsAdvancedSelected()
{
    return g_RadarCfg.mode != nullptr && g_RadarCfg.mode->value >= 1.0f;
}

bool RadarConfig_CheckAdvancedActive()
{
    return RadarConfig_IsAdvancedSelected() && RadarDraw_CheckPrimsAvailable();
}
