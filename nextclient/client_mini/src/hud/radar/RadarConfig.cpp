#include "RadarConfig.h"
#include "RadarDraw.h"
#include "../../main.h"

#include <next_client_mini/radar_cvars.h>

RadarConfig g_RadarCfg;

namespace
{
    // A name missing from the shared table registers a null name and crashes loudly at
    // startup, instead of silently registering a wrong default.
    cvar_t* Reg(const char* name)
    {
        RadarCvarDef def = RadarCvarDef_Get(name);
        return gEngfuncs.pfnRegisterVariable(const_cast<char*>(def.name), const_cast<char*>(def.default_value), FCVAR_ARCHIVE);
    }
} // namespace

void RadarConfig_Init()
{
    g_RadarCfg.mode = Reg("hud_radar_mode");
    g_RadarCfg.scale = Reg("hud_radar_scale");
    g_RadarCfg.map_scale = Reg("hud_radar_map_scale");
    g_RadarCfg.opacity = Reg("hud_radar_opacity");
    g_RadarCfg.round = Reg("hud_radar_round");
    g_RadarCfg.fullmap = Reg("hud_radar_fullmap");
    g_RadarCfg.rotate = Reg("hud_radar_rotate");
    g_RadarCfg.names = Reg("hud_radar_names");
    g_RadarCfg.health = Reg("hud_radar_health");
    g_RadarCfg.tracks = Reg("hud_radar_tracks");
    g_RadarCfg.objectives = Reg("hud_radar_objectives");
    g_RadarCfg.map_mode = Reg("hud_radar_map_mode");
    g_RadarCfg.map_mask = Reg("hud_radar_map_mask");
    g_RadarCfg.bg = Reg("hud_radar_bg");

    g_RadarCfg.health_size = Reg("hud_radar_health_size");
    g_RadarCfg.name_size = Reg("hud_radar_name_size");
    g_RadarCfg.allies = Reg("hud_radar_allies");
}

bool RadarConfig_AdvancedSelected()
{
    return g_RadarCfg.mode != nullptr && g_RadarCfg.mode->value >= 1.0f;
}

bool RadarConfig_AdvancedActive()
{
    return RadarConfig_AdvancedSelected() && RadarDraw_PrimsAvailable();
}
