#pragma once

#include <nitroapi/NitroApiInterface.h>
#include <next_gameui/IGameConsoleNext.h>
#include "hud/GameHud.h"

using V_CalcRefdefNext = nitroapi::NextHandlerInterface<void, ref_params_s*>*;
using UserMsg_SetFOVNext = nitroapi::NextHandlerInterface<int, const char*, int, void*>*;
using HUD_ProcessPlayerStateNext = nitroapi::NextHandlerInterface<void, entity_state_s*, const entity_state_s*>*;
using HUD_ResetNext = nitroapi::NextHandlerInterface<void>*;
using HUD_VidInitNext = nitroapi::NextHandlerInterface<int>*;
using UserMsg_TextMsgNext = nitroapi::NextHandlerInterface<int, const char*, int, void*>*;

extern nitroapi::NitroApiInterface* g_NitroApi;
nitroapi::EngineData* eng();
nitroapi::ClientData* client();

extern IGameConsole* g_GameConsole;
extern IGameConsoleNext* g_GameConsoleNext;

extern cl_enginefunc_t gEngfuncs;
extern engine_studio_api_t IEngineStudio;
extern r_studio_interface_t g_OriginalStudio;
extern local_state_t g_LastPlayerState;
extern client_data_t g_LastClientData;
extern server_static_t* sv_static;

extern gamehud_t* gHUD;
extern int g_iUser1;
extern int g_iUser2;
extern int g_CurrentWeaponId;

extern playermove_t* pmove;

extern std::unique_ptr<GameHud> g_GameHud;

//inline int Initialize(cl_enginefunc_t *pEnginefuncs, int iVersion)              { return g_NitroApi->GetClientData()->CLDLL_Initialize(pEnginefuncs, iVersion); }
inline int HUD_VidInit()                                                        { return g_NitroApi->GetClientData()->HUD_VidInit(); }
inline void HUD_Init()                                                          { g_NitroApi->GetClientData()->HUD_Init(); }
inline int HUD_Redraw(float time, int intermission)                             { return g_NitroApi->GetClientData()->HUD_Redraw(time, intermission); }
inline int HUD_UpdateClientData(client_data_t *pcldata, float flTime)           { return g_NitroApi->GetClientData()->HUD_UpdateClientData(pcldata, flTime); }
inline void HUD_Reset()                                                         { g_NitroApi->GetClientData()->HUD_Reset(); }
inline void HUD_PlayerMove(playermove_t *ppmove, int server)                    { g_NitroApi->GetClientData()->HUD_PlayerMove(ppmove, server); }
inline void HUD_PlayerMoveInit(playermove_t *ppmove)                            { g_NitroApi->GetClientData()->HUD_PlayerMoveInit(ppmove); }
inline char HUD_PlayerMoveTexture(char *name)                                   { return g_NitroApi->GetClientData()->HUD_PlayerMoveTexture(name); }
inline void IN_ActivateMouse()                                                  { g_NitroApi->GetClientData()->IN_ActivateMouse(); }
inline void IN_DeactivateMouse()                                                { g_NitroApi->GetClientData()->IN_DeactivateMouse(); }
inline void IN_MouseEvent(int mstate)                                           { g_NitroApi->GetClientData()->IN_MouseEvent(mstate); }
inline void IN_ClearStates()                                                    { g_NitroApi->GetClientData()->IN_ClearStates(); }
inline void IN_Accumulate()                                                     { g_NitroApi->GetClientData()->IN_Accumulate(); }
inline void CL_CreateMove(float frametime, usercmd_t *cmd, int active)          { g_NitroApi->GetClientData()->CL_CreateMove(frametime, cmd, active); }
inline int CL_IsThirdPerson()                                                   { return g_NitroApi->GetClientData()->CL_IsThirdPerson(); }
inline void CL_CameraOffset(float *ofs)                                         { g_NitroApi->GetClientData()->CL_CameraOffset(ofs); }
inline void CAM_Think()                                                         { g_NitroApi->GetClientData()->CAM_Think(); }
inline kbutton_t *KB_Find(const char *name)                                     { return g_NitroApi->GetClientData()->KB_Find(name); }
inline void V_CalcRefdef(ref_params_t *pparams)                                 { g_NitroApi->GetClientData()->V_CalcRefdef(pparams); }
inline int HUD_AddEntity(int type, cl_entity_t *ent, const char *modelname)     { return g_NitroApi->GetClientData()->HUD_AddEntity(type, ent, modelname); }
inline void HUD_CreateEntities()                                                { g_NitroApi->GetClientData()->HUD_CreateEntities(); }
inline void HUD_DrawNormalTriangles()                                           { g_NitroApi->GetClientData()->HUD_DrawNormalTriangles(); }
inline void HUD_DrawTransparentTriangles()                                      { g_NitroApi->GetClientData()->HUD_DrawTransparentTriangles(); }
inline void HUD_StudioEvent(const mstudioevent_s *event, const cl_entity_t *entity) { g_NitroApi->GetClientData()->HUD_StudioEvent(event, entity); }
inline void HUD_Shutdown()                                                      { g_NitroApi->GetClientData()->HUD_Shutdown(); }
inline void HUD_TxferLocalOverrides(entity_state_s *state, const clientdata_t *client) { g_NitroApi->GetClientData()->HUD_TxferLocalOverrides(state, client); }
inline void HUD_ProcessPlayerState(entity_state_t *dst, entity_state_t *src)    { g_NitroApi->GetClientData()->HUD_ProcessPlayerState(dst, src); }
inline void HUD_TxferPredictionData(entity_state_t *ps, const entity_state_t *pps, clientdata_t *pcd, const clientdata_t *ppcd, weapon_data_t *wd, const weapon_data_t *pwd)
                                                                                { g_NitroApi->GetClientData()->HUD_TxferPredictionData(ps, pps, pcd, ppcd, wd, pwd); }
inline void Demo_ReadBuffer(int size, unsigned char *buffer)                    { g_NitroApi->GetClientData()->Demo_ReadBuffer(size, buffer); }
inline int HUD_ConnectionlessPacket(const netadr_t *net_from, const char *args, char *response_buffer, int *response_buffer_size)
                                                                                { return g_NitroApi->GetClientData()->HUD_ConnectionlessPacket(net_from, args, response_buffer, response_buffer_size); }
inline int HUD_GetHullBounds(int hullnumber, float *mins, float *maxs)          { return g_NitroApi->GetClientData()->HUD_GetHullBounds(hullnumber, mins, maxs); }
inline void HUD_Frame(double time)                                              { g_NitroApi->GetClientData()->HUD_Frame(time); }
inline int HUD_Key_Event(int down, int keynum, const char *pszCurrentBinding)   { return g_NitroApi->GetClientData()->HUD_Key_Event(down, keynum, pszCurrentBinding); }
inline void HUD_PostRunCmd(local_state_t *from, local_state_t *to, usercmd_t *cmd, int runfuncs, double time, unsigned int random_seed)
                                                                                { g_NitroApi->GetClientData()->HUD_PostRunCmd(from, to, cmd, runfuncs, time, random_seed); }
inline void HUD_TempEntUpdate(double frametime, double client_time, double cl_gravity, TEMPENTITY **ppTempEntFree, TEMPENTITY **ppTempEntActive, int (*Callback_AddVisibleEntity)(cl_entity_t *), void (*Callback_TempEntPlaySound)(TEMPENTITY *, float))
                                                                                { g_NitroApi->GetClientData()->HUD_TempEntUpdate(frametime, client_time, cl_gravity, ppTempEntFree, ppTempEntActive, Callback_AddVisibleEntity, Callback_TempEntPlaySound); }
inline cl_entity_t *HUD_GetUserEntity(int index)                                { return g_NitroApi->GetClientData()->HUD_GetUserEntity(index); }
inline void HUD_VoiceStatus(int entindex, qboolean bTalking)                    { g_NitroApi->GetClientData()->HUD_VoiceStatus(entindex, bTalking); }
inline void HUD_DirectorMessage(int iSize, void *pbuf)                          { g_NitroApi->GetClientData()->HUD_DirectorMessage(iSize, pbuf); }
//inline void HUD_WeaponsPostThink(local_state_t *from, local_state_t *to, usercmd_t *cmd, double time, unsigned int random_seed);
inline int HUD_GetStudioModelInterface(int version, r_studio_interface_t **ppinterface, engine_studio_api_t *pstudio)
                                                                                { return g_NitroApi->GetClientData()->HUD_GetStudioModelInterface(version, ppinterface, pstudio); }
//inline void HUD_ChatInputPosition(int *x, int *y);
//inline int HUD_GetPlayerTeam(int iplayer);