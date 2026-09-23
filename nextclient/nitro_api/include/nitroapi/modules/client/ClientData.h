#pragma once

#include "../../chain/HandlersChainInterface.h"
#include "../../NitroFunction.h"
#include "GameHud.h"
#include <goldsrc/client_api.h>
#include <goldsrc/cstrike/hud.h>
#include <goldsrc/cstrike/hud_elements.h>
#include <goldsrc/engine/net.h>
#include <goldsrc/entity.h>
#include <goldsrc/format/studio.h>
#include <goldsrc/fwd.h>
#include <goldsrc/pmove.h>
#include <goldsrc/studio_api.h>
#include <goldsrc/types.h>
#include <goldsrc/usercmd.h>
#include <goldsrc/view.h>

#include <steam/steamtypes.h>

namespace nitroapi
{
    struct ClientData
    {
        gamehud_t* gHUD = nullptr;
        int* g_iShotsFired = nullptr;

        // int UserMsg_MOTD(const char* name, int size, void* data)
        NitroFunction<int, const char*, int, void*>                 UserMsg_MOTD;
        // int UserMsg_AmmoX(const char* name, int size, void* data)
        NitroFunction<int, const char*, int, void*>                 UserMsg_AmmoX;
        // int UserMsg_SetFOV(const char* name, int size, void* data)
        NitroFunction<int, const char*, int, void*>                 UserMsg_SetFOV;

        // int CHudAmmo::DrawCrosshair(goldsrc::cstrike::CHudAmmo *const this, float flTime, int weaponid)
        NitroFunction<int, goldsrc::cstrike::CHudAmmo* const, float, int> CHudAmmo__DrawCrosshair;

        // int Initialize(goldsrc::cl_enginefunc_t *pEnginefuncs, int iVersion)
        NitroFunction<int, goldsrc::cl_enginefunc_t*, int> CLDLL_Initialize;
        // void HUD_Init()
        NitroFunctionVoid<>                                         HUD_Init;
        // int HUD_VidInit()
        NitroFunction<int>                                          HUD_VidInit;
        // int HUD_Redraw(float flTime, int iIntermission)
        NitroFunction<int, float, int>                              HUD_Redraw;
        // int HUD_UpdateClientData(goldsrc::client_data_t *cdata, float flTime)
        NitroFunction<int, goldsrc::client_data_t*, float> HUD_UpdateClientData;
        // void HUD_Reset()
        NitroFunctionVoid<>                                         HUD_Reset;
        // void HUD_PlayerMove(struct goldsrc::playermove_t *ppmove, int server)
        NitroFunctionVoid<goldsrc::playermove_t*, int> HUD_PlayerMove;
        // void HUD_PlayerMoveInit(struct goldsrc::playermove_t *ppmove)
        NitroFunctionVoid<goldsrc::playermove_t*> HUD_PlayerMoveInit;
        // char HUD_PlayerMoveTexture(char *name)
        NitroFunction<char, char*>                                  HUD_PlayerMoveTexture;
        // void IN_ActivateMouse()
        NitroFunctionVoid<>                                         IN_ActivateMouse;
        // void IN_DeactivateMouse()
        NitroFunctionVoid<>                                         IN_DeactivateMouse;
        // void IN_MouseEvent(int mstate)
        NitroFunctionVoid<int>                                      IN_MouseEvent;
        // void IN_ClearStates()
        NitroFunctionVoid<>                                         IN_ClearStates;
        // void IN_Accumulate()
        NitroFunctionVoid<>                                         IN_Accumulate;
        // void CL_CreateMove(float frametime, struct goldsrc::usercmd_t *cmd, int active)
        NitroFunctionVoid<float, goldsrc::usercmd_t*, int> CL_CreateMove;
        // int CL_IsThirdPerson()
        NitroFunction<int>                                          CL_IsThirdPerson;
        // void CL_CameraOffset(float *ofs)
        NitroFunctionVoid<float*>                                   CL_CameraOffset;
        // struct goldsrc::kbutton_t *KB_Find(const char *name)
        NitroFunction<goldsrc::kbutton_t*, const char*> KB_Find;
        // void CAM_Think()
        NitroFunctionVoid<>                                         CAM_Think;
        // void V_CalcRefdef(struct goldsrc::ref_params_t* pparams)
        NitroFunctionVoid<goldsrc::ref_params_t*> V_CalcRefdef;
        // int HUD_AddEntity(int type, struct goldsrc::cl_entity_t *ent, const char *modelname)
        NitroFunction<int, int, goldsrc::cl_entity_t*, const char*> HUD_AddEntity;
        // void HUD_CreateEntities()
        NitroFunctionVoid<>                                         HUD_CreateEntities;
        // void HUD_DrawNormalTriangles()
        NitroFunctionVoid<>                                         HUD_DrawNormalTriangles;
        // void HUD_DrawTransparentTriangles()
        NitroFunctionVoid<>                                         HUD_DrawTransparentTriangles;
        // void HUD_StudioEvent(const struct goldsrc::mstudioevent_s *event, const struct goldsrc::cl_entity_t *entity)
        NitroFunctionVoid<const goldsrc::mstudioevent_s*, const goldsrc::cl_entity_t*> HUD_StudioEvent;
        // void HUD_PostRunCmd(struct goldsrc::local_state_t *from, struct goldsrc::local_state_t *to, struct goldsrc::usercmd_t *cmd, int runfuncs, double time, unsigned int random_seed)
        NitroFunctionVoid<goldsrc::local_state_t*, goldsrc::local_state_t*, goldsrc::usercmd_t*, int, double, unsigned int> HUD_PostRunCmd;
        // void HUD_Shutdown()
        NitroFunctionVoid<>                                         HUD_Shutdown;
        // void HUD_TxferLocalOverrides(struct goldsrc::entity_state_t *state, const struct goldsrc::clientdata_t *client)
        NitroFunctionVoid<goldsrc::entity_state_t*, const goldsrc::clientdata_t*> HUD_TxferLocalOverrides;
        // void HUD_ProcessPlayerState(struct goldsrc::entity_state_t *dst, const struct goldsrc::entity_state_t *src)
        NitroFunctionVoid<goldsrc::entity_state_t*, const goldsrc::entity_state_t*> HUD_ProcessPlayerState;
        // void HUD_TxferPredictionData(struct goldsrc::entity_state_t *ps, const struct goldsrc::entity_state_t *pps, struct goldsrc::clientdata_t *pcd, const struct goldsrc::clientdata_t *ppcd, struct goldsrc::weapon_data_t *wd, const struct goldsrc::weapon_data_t *pwd)
        NitroFunctionVoid<goldsrc::entity_state_t*,
                          const goldsrc::entity_state_t*,
                          goldsrc::clientdata_t*,
                          const goldsrc::clientdata_t*,
                          goldsrc::weapon_data_t*,
                          const goldsrc::weapon_data_t*>
            HUD_TxferPredictionData;
        // void Demo_ReadBuffer(int size, unsigned char *buffer)
        NitroFunctionVoid<int, unsigned char*>                      Demo_ReadBuffer;
        // int HUD_ConnectionlessPacket(const goldsrc::netadr_t *net_from, const char *args, char *response_buffer,
        //                              int *response_buffer_size)
        NitroFunction<int, const goldsrc::netadr_t*, const char*, char*, int*> HUD_ConnectionlessPacket;
        // int HUD_GetHullBounds(int hullnumber, float *mins, float *maxs)
        NitroFunction<int, int, float*, float*>                     HUD_GetHullBounds;
        // void HUD_Frame(double time)
        NitroFunctionVoid<double>                                   HUD_Frame;
        // int HUD_Key_Event(int eventcode, int keynum, const char *pszCurrentBinding)
        NitroFunction<int, int, int, const char*>                   HUD_Key_Event;
        // void HUD_TempEntUpdate(double frametime,double client_time, double cl_gravity, goldsrc::TEMPENTITY **ppTempEntFree, goldsrc::TEMPENTITY **ppTempEntActive, int ( *Callback_AddVisibleEntity )( goldsrc::cl_entity_t *pEntity ), void ( *Callback_TempEntPlaySound )( goldsrc::TEMPENTITY *pTemp, float damp ))
        NitroFunctionVoid<double,
                          double,
                          double,
                          goldsrc::TEMPENTITY**,
                          goldsrc::TEMPENTITY**,
                          int (*)(goldsrc::cl_entity_t* pEntity),
                          void (*)(goldsrc::TEMPENTITY* pTemp, float damp)>
            HUD_TempEntUpdate;
        // struct goldsrc::cl_entity_t *HUD_GetUserEntity(int index)
        NitroFunction<goldsrc::cl_entity_t*, int> HUD_GetUserEntity;
        // int HUD_VoiceStatus(int entindex, goldsrc::qboolean bTalking)
        NitroFunction<int, int, goldsrc::qboolean> HUD_VoiceStatus;
        // void HUD_DirectorMessage(int iSize, void *pbuf)
        NitroFunctionVoid<int, void*>                               HUD_DirectorMessage;
        // int HUD_GetStudioModelInterface(int version, struct goldsrc::r_studio_interface_t **ppinterface, struct goldsrc::engine_studio_api_t *pstudio)
        NitroFunction<int, int, goldsrc::r_studio_interface_t**, goldsrc::engine_studio_api_t*> HUD_GetStudioModelInterface;

        // int UserMsg_CurWeapon(const char* name, int size, void* data)
        NitroFunction<int, const char*, int, void*>                 UserMsg_CurWeapon;
        // int UserMsg_InitHUD(const char* name, int size, void* data)
        NitroFunction<int, const char*, int, void*>                 UserMsg_InitHUD;
        // int UserMsg_TextMsg(const char* name, int size, void* data)
        NitroFunction<int, const char*, int, void*>                 UserMsg_TextMsg;

        // int CHudHealth::Draw(goldsrc::cstrike::CHudHealth *const this, float flTime)
        NitroFunction<int, goldsrc::cstrike::CHudHealth* const, float> CHudHealth__Draw;
        // void CHudHealth::DrawRadar(goldsrc::cstrike::CHudHealth *const this, float flTime)
        NitroFunctionVoid<goldsrc::cstrike::CHudHealth* const, float> CHudHealth__DrawRadar;
        // void CHudHealth::GetAllPlayersInfo(goldsrc::cstrike::CHudHealth *const this)
        NitroFunctionVoid<goldsrc::cstrike::CHudHealth* const> CHudHealth__GetAllPlayersInfo;
        // int CHudDeathNotice::Draw(goldsrc::cstrike::CHudDeathNotice *const this, float flTime)
        NitroFunction<int, goldsrc::cstrike::CHudDeathNotice* const, float> CHudDeathNotice__Draw;

        // void UserMsg_DeathMsg(const char* name, int size, void* data)
        NitroFunctionVoid<const char*, int, void*>                 UserMsg_DeathMsg;

        goldsrc::cstrike::extra_player_info_t* g_PlayerExtraInfo;

        // int UserMsg_BombDrop(const char* name, int size, void* data)
        NitroFunction<int, const char*, int, void*>                UserMsg_BombDrop;
        // int UserMsg_BombPickup(const char* name, int size, void* data)
        NitroFunction<int, const char*, int, void*>                UserMsg_BombPickup;
        // int UserMsg_HostagePos(const char* name, int size, void* data)
        NitroFunction<int, const char*, int, void*>                UserMsg_HostagePos;
        // int UserMsg_HostageK(const char* name, int size, void* data)
        NitroFunction<int, const char*, int, void*>                UserMsg_HostageK;
        // int MsgFunc_ResetHUD(const char* name, int size, void* data)
        NitroFunction<int, const char*, int, void*>                UserMsg_ResetHUD;

        goldsrc::cl_enginefunc_t* gEngfuncs = nullptr;
    };
}