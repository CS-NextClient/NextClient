#include <amxmodx>
#include <amxmisc>
#include <cstrike>
#include <fakemeta>
#include <hamsandwich>
#include <json>
#include <reapi>
#pragma dynamic 32768

new const kPluginVersion[] = "0.1.0";
new g_SessionId[33];
new g_MapEpoch;

#include "maps.inc"
#include "core.inc"
#include "diagnostics.inc"
#include "actors.inc"
#include "radar.inc"
#include "radar_hostage.inc"
#include "radar_bomb.inc"
#include "radar_players.inc"
#include "geometry.inc"
#include "selftests.inc"

public plugin_init()
{
    register_plugin("NextClient TestLab", kPluginVersion, "NextClient");
    register_srvcmd("nct_session", "Diagnostics_Session");
    register_srvcmd("nct_probe", "Radar_ProbeCommand");
    register_srvcmd("nct_selftest", "Selftests_Command");
    register_srvcmd("nct_geometry", "Geometry_Command");
    new const commands[][] = {"nct_menu", "nct_list", "nct_run", "nct_step", "nct_repeat", "nct_reset", "nct_resume",
        "nct_status", "nct_operator", "nct_outcome", "nct_team", "nct_respawn", "nct_die", "nct_observe", "nct_goto", "nct_god", "nct_noclip", "nct_trace", "nct_report", "nct_result", "nct_view", "nct_map"};
    for (new index = 0; index < sizeof commands; index++)
    {
        register_concmd(commands[index], "Core_Command", 0, "", 0);
    }
    register_concmd("nct_anchor", "Radar_Anchor", 0, "", 0);
    register_forward(FM_StartFrame, "Radar_StartFrame");
    register_forward(FM_AddToFullPack, "Diagnostics_FullPack", true);
    RegisterHookChain(RG_CSGameRules_RestartRound, "Radar_RoundRestarted", true);
    RegisterHookChain(RG_CBasePlayer_ImpulseCommands, "Radar_InputObserved");
    RegisterHam(Ham_Use, "hostage_entity", "Radar_HostageUsed", true);
    new const messages[][] = {"Radar", "BombDrop", "BombPickup", "HostagePos", "HostageK"};
    for (new index = 0; index < sizeof messages; index++)
    {
        register_message(get_user_msgid(messages[index]), "Radar_Message");
    }
}

public plugin_cfg()
{
    if (!is_rehlds() || !is_regamedll())
    {
        set_fail_state("ReHLDS and ReGameDLL are required");
        return;
    }
    Diagnostics_Initialize();
    Core_Initialize();
}

public plugin_end()
{
    Core_Cancel("plugin_end");
}
