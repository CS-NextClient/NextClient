#include "ClientAddressProvider8684Windows.h"

namespace nitroapi
{
    const std::unordered_map<StaticHookId, SearchConfig> ClientAddressProvider8684Windows::addresses_hook_id_
    {
        { StaticHookId::UserMsg_MOTD,               SearchConfig(0x4e560) },
        { StaticHookId::UserMsg_AmmoX,              SearchConfig(0x402A0) },
        { StaticHookId::UserMsg_SetFOV,             SearchConfig(0x4E150) },
        { StaticHookId::UserMsg_CurWeapon,          SearchConfig(0x40260) },
        { StaticHookId::UserMsg_InitHUD,            SearchConfig(0x4E0F0) },
        { StaticHookId::UserMsg_TextMsg,            SearchConfig(0x69930) },

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

        { StaticHookId::CHudAmmo__DrawCrosshair,    SearchConfig(0x426C0) },
        { StaticHookId::CHudHealth__Draw,           SearchConfig(0x4B410) },
        { StaticHookId::CHudHealth__DrawRadar,      SearchConfig(0x4CAF0) },
        { StaticHookId::CHudHealth__GetAllPlayersInfo,SearchConfig(0x4D470) },
        { StaticHookId::CHudDeathNotice__Draw,      SearchConfig(0x45640) },
        { StaticHookId::UserMsg_DeathMsg,           SearchConfig(0x454D0) },
        { StaticHookId::UserMsg_BombDrop,           SearchConfig(0x4DFD0) },
        { StaticHookId::UserMsg_BombPickup,         SearchConfig(0x4E000) },
        { StaticHookId::UserMsg_HostagePos,         SearchConfig(0x4E090) },
        { StaticHookId::UserMsg_HostageK,           SearchConfig(0x4E0C0) },
        { StaticHookId::UserMsg_ResetHUD,           SearchConfig(0x4DF80) }
    };

    const std::unordered_map<std::string, SearchConfig> ClientAddressProvider8684Windows::addresses_str_
    {
        { "gHUD",                                   SearchConfig(0x121530) },
        { "g_iShotsFired",                          SearchConfig(0x1213A4) },
        { "g_PlayerExtraInfo",                      SearchConfig(0x12F420) },
        { "gEngfuncs",                              SearchConfig(0x125CC0) }
    };
}