#include "ClientAddressProvider8684Linux.h"

namespace nitroapi
{
    const std::unordered_map<StaticHookId, SearchConfig> ClientAddressProvider8684Linux::addresses_hook_id_
    {
        { StaticHookId::UserMsg_MOTD,               SearchConfig("_Z14__MsgFunc_MOTDPKciPv", SearchType::ExportFunc) },
        { StaticHookId::UserMsg_AmmoX,              SearchConfig("_Z15__MsgFunc_AmmoXPKciPv", SearchType::ExportFunc) },
        { StaticHookId::UserMsg_SetFOV,             SearchConfig("_Z16__MsgFunc_SetFOVPKciPv", SearchType::ExportFunc) },
        { StaticHookId::UserMsg_CurWeapon,          SearchConfig("_Z19__MsgFunc_CurWeaponPKciPv", SearchType::ExportFunc) },
        { StaticHookId::UserMsg_InitHUD,            SearchConfig("_Z17__MsgFunc_InitHUDPKciPv", SearchType::ExportFunc) },
        { StaticHookId::UserMsg_TextMsg,            SearchConfig("_Z17__MsgFunc_TextMsgPKciPv", SearchType::ExportFunc) },

        { StaticHookId::CLDLL_Initialize,           SearchConfig("Initialize", SearchType::ExportFunc) },
        { StaticHookId::HUD_Init,                   SearchConfig("HUD_Init", SearchType::ExportFunc) },
        { StaticHookId::HUD_VidInit,                SearchConfig("HUD_VidInit", SearchType::ExportFunc) },
        { StaticHookId::HUD_Redraw,                 SearchConfig("HUD_Redraw", SearchType::ExportFunc) },
        { StaticHookId::HUD_UpdateClientData,       SearchConfig("HUD_UpdateClientData", SearchType::ExportFunc) },
        { StaticHookId::HUD_Reset,                  SearchConfig("HUD_Reset", SearchType::ExportFunc) },
        { StaticHookId::HUD_PlayerMove,             SearchConfig("HUD_PlayerMove", SearchType::ExportFunc) },
        { StaticHookId::HUD_PlayerMoveInit,         SearchConfig("HUD_PlayerMoveInit", SearchType::ExportFunc) },
        { StaticHookId::HUD_PlayerMoveTexture,      SearchConfig("HUD_PlayerMoveTexture", SearchType::ExportFunc) },
        { StaticHookId::IN_ActivateMouse,           SearchConfig("IN_ActivateMouse", SearchType::ExportFunc) },
        { StaticHookId::IN_DeactivateMouse,         SearchConfig("IN_DeactivateMouse", SearchType::ExportFunc) },
        { StaticHookId::IN_MouseEvent,              SearchConfig("IN_MouseEvent", SearchType::ExportFunc) },
        { StaticHookId::IN_ClearStates,             SearchConfig("IN_ClearStates", SearchType::ExportFunc) },
        { StaticHookId::IN_Accumulate,              SearchConfig("IN_Accumulate", SearchType::ExportFunc) },
        { StaticHookId::CL_CreateMove,              SearchConfig("CL_CreateMove", SearchType::ExportFunc) },
        { StaticHookId::CL_IsThirdPerson,           SearchConfig("CL_IsThirdPerson", SearchType::ExportFunc) },
        { StaticHookId::CL_CameraOffset,            SearchConfig("CL_CameraOffset", SearchType::ExportFunc) },
        { StaticHookId::KB_Find,                    SearchConfig("KB_Find", SearchType::ExportFunc) },
        { StaticHookId::CAM_Think,                  SearchConfig("CAM_Think", SearchType::ExportFunc) },
        { StaticHookId::V_CalcRefdef,               SearchConfig("V_CalcRefdef", SearchType::ExportFunc) },
        { StaticHookId::HUD_AddEntity,              SearchConfig("HUD_AddEntity", SearchType::ExportFunc) },
        { StaticHookId::HUD_CreateEntities,         SearchConfig("HUD_CreateEntities", SearchType::ExportFunc) },
        { StaticHookId::HUD_DrawNormalTriangles,    SearchConfig("HUD_DrawNormalTriangles", SearchType::ExportFunc) },
        { StaticHookId::HUD_DrawTransparentTriangles,SearchConfig("HUD_DrawTransparentTriangles", SearchType::ExportFunc) },
        { StaticHookId::HUD_StudioEvent,            SearchConfig("HUD_StudioEvent", SearchType::ExportFunc) },
        { StaticHookId::HUD_PostRunCmd,             SearchConfig("HUD_PostRunCmd", SearchType::ExportFunc) },
        { StaticHookId::HUD_Shutdown,               SearchConfig("HUD_Shutdown", SearchType::ExportFunc) },
        { StaticHookId::HUD_TxferLocalOverrides,    SearchConfig("HUD_TxferLocalOverrides", SearchType::ExportFunc) },
        { StaticHookId::HUD_ProcessPlayerState,     SearchConfig("HUD_ProcessPlayerState", SearchType::ExportFunc) },
        { StaticHookId::HUD_TxferPredictionData,    SearchConfig("HUD_TxferPredictionData", SearchType::ExportFunc) },
        { StaticHookId::Demo_ReadBuffer,            SearchConfig("Demo_ReadBuffer", SearchType::ExportFunc) },
        { StaticHookId::HUD_ConnectionlessPacket,   SearchConfig("HUD_ConnectionlessPacket", SearchType::ExportFunc) },
        { StaticHookId::HUD_GetHullBounds,          SearchConfig("HUD_GetHullBounds", SearchType::ExportFunc) },
        { StaticHookId::HUD_Frame,                  SearchConfig("HUD_Frame", SearchType::ExportFunc) },
        { StaticHookId::HUD_Key_Event,              SearchConfig("HUD_Key_Event", SearchType::ExportFunc) },
        { StaticHookId::HUD_TempEntUpdate,          SearchConfig("HUD_TempEntUpdate", SearchType::ExportFunc) },
        { StaticHookId::HUD_GetUserEntity,          SearchConfig("HUD_GetUserEntity", SearchType::ExportFunc) },
        { StaticHookId::HUD_VoiceStatus,            SearchConfig("HUD_VoiceStatus", SearchType::ExportFunc) },
        { StaticHookId::HUD_DirectorMessage,        SearchConfig("HUD_DirectorMessage", SearchType::ExportFunc) },
        { StaticHookId::HUD_GetStudioModelInterface,SearchConfig("HUD_GetStudioModelInterface", SearchType::ExportFunc) },

        { StaticHookId::CHudAmmo__DrawCrosshair,    SearchConfig("_ZN8CHudAmmo13DrawCrosshairEfi", SearchType::ExportFunc) },
        { StaticHookId::CHudHealth__Draw,           SearchConfig("_ZN10CHudHealth4DrawEf", SearchType::ExportFunc) },
        { StaticHookId::CHudHealth__DrawRadar,      SearchConfig("_ZN10CHudHealth9DrawRadarEf", SearchType::ExportFunc) }, 
        { StaticHookId::CHudHealth__GetAllPlayersInfo,SearchConfig("_ZN10CHudHealth17GetAllPlayersInfoEv", SearchType::ExportFunc) },
        { StaticHookId::CHudDeathNotice__Draw,      SearchConfig("_ZN15CHudDeathNotice4DrawEf", SearchType::ExportFunc) },
        { StaticHookId::UserMsg_DeathMsg,           SearchConfig("_Z18__MsgFunc_DeathMsgPKciPv", SearchType::ExportFunc) },
        { StaticHookId::UserMsg_BombDrop,           SearchConfig("_Z18__MsgFunc_BombDropPKciPv", SearchType::ExportFunc) },
        { StaticHookId::UserMsg_BombPickup,         SearchConfig("_Z20__MsgFunc_BombPickupPKciPv", SearchType::ExportFunc) },
        { StaticHookId::UserMsg_HostagePos,         SearchConfig("_Z20__MsgFunc_HostagePosPKciPv", SearchType::ExportFunc) },
        { StaticHookId::UserMsg_HostageK,           SearchConfig("_Z18__MsgFunc_HostageKPKciPv", SearchType::ExportFunc) },
        { StaticHookId::UserMsg_ResetHUD,           SearchConfig("_Z18__MsgFunc_ResetHUDPKciPv", SearchType::ExportFunc) }

    };

    const std::unordered_map<std::string, SearchConfig> ClientAddressProvider8684Linux::addresses_str_
    {
        { "gHUD",                                   SearchConfig("gHUD", SearchType::ExportFunc) },
        { "g_iShotsFired",                          SearchConfig("g_iShotsFired", SearchType::ExportFunc) },
        { "g_PlayerExtraInfo",                      SearchConfig("g_PlayerExtraInfo", SearchType::ExportFunc) },
        { "gEngfuncs",                              SearchConfig("gEngfuncs", SearchType::ExportFunc) }
    };
}