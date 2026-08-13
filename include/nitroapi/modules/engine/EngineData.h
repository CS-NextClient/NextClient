#pragma once

#include "../../chain/HandlersChainInterface.h"
#include "../../NitroFunction.h"
#include "IEngine.h"
#include "structs.h"
#include "voice.h"
#include "port.h"

#if NO_HLSDK_ARCHTYPES
#include <steam/steamtypes.h>
#endif

#include <server.h>
#include <extdll.h>
#include <vmodes.h>
#include <pm_defs.h>
#include <client.h>
#include <r_efx.h>
#include <ref_params.h>
#include <ref_int.h>
#include <APIProxy.h>
#include <r_studioint.h>
#include <cdll_int.h>
#include <cmd.h>

struct dma_t;
struct sfxcache_t;
struct channel_t;
struct wavstream_t;

namespace nitroapi
{
    typedef char* (__cdecl *EngCon_PrintfFunc)(const char* format, ...);
    typedef void (__cdecl *Sys_ErrorFunc)(const char *error, ...);
    typedef void (__cdecl *Sys_PrintfFunc)(const char *format, ...);

    struct EngineData
    {
        PEngineMsg EngineMsgBase = nullptr;

        int* msg_readcount = nullptr;
        int* host_framecount = nullptr;
        double* realtime = nullptr;
        double* host_frametime = nullptr;
        qboolean* gfExtendedError = nullptr;
        char* gszDisconnectReason = nullptr;
        uint16_t* SampleRate = nullptr;
        char* gDownloadFile = nullptr;

        server_static_t* server_static = nullptr; // svs
        server_t* server = nullptr; // sv
        client_static_t* client_static = nullptr;
        client_state_t* client_state = nullptr;
        cl_enginefunc_t* cl_enginefunc = nullptr;
        enginefuncs_t* enginefuncs = nullptr;
        sizebuf_t* net_message = nullptr;
        netadr_t* net_from = nullptr;
        cldll_func_t* cldll_func = nullptr;
        modfuncs_t* modfuncs = nullptr;
        IEngine* eng = nullptr;
        cvar_t* scr_downloading = nullptr;
        cmd_source_t* cmd_source = nullptr;
        qboolean* bMajorMapChange = nullptr;
        CareerStateType* careerState = nullptr;
        model_t** loadmodel = nullptr;
        int* cl_playerindex = nullptr;
        int* com_argc = nullptr;
        char** com_argv = nullptr;
        cache_user_t** loadcache = nullptr;
        unsigned char** loadbuf = nullptr;
        int* loadsize = nullptr;
        sfx_t** known_sfx = nullptr;
        int* num_sfx = nullptr;
        cl_entity_t** cl_entities = nullptr;
        bool* noclip_anglehack = nullptr;
        model_t** currentmodel = nullptr;
        mvertex_t ** r_pcurrentvertbase = nullptr;

        // void CL_QueueHTTPDownload(const char *file)
        NitroFunctionVoid<const char*>                  CL_QueueHTTPDownload;
        // void CL_HTTPUpdate()
        NitroFunctionVoid<>                             CL_HTTPUpdate;
        // bool CL_CanUseHTTPDownload()
        NitroFunction<bool>                             CL_CanUseHTTPDownload;
        // int CL_GetDownloadQueueSize()
        NitroFunction<int>                              CL_GetDownloadQueueSize;
        // void CL_Connect_f()
        NitroFunctionVoid<>                             CL_Connect_f;
        // void CL_HTTPStop_f()
        NitroFunctionVoid<>                             CL_HTTPStop_f;
        // void CL_HTTPCancel_f()
        NitroFunctionVoid<>                             CL_HTTPCancel_f;
        // void CL_Disconnect()
        NitroFunctionVoid<>                             CL_Disconnect;
        // void CL_SendConsistencyInfo(sizebuf_t *buf)
        NitroFunctionVoid<sizebuf_t*>                   CL_SendConsistencyInfo;
        // void CL_Init()
        NitroFunctionVoid<>                             CL_Init;
        // int CL_CheckFile(sizebuf_t *msg, char *filename)
        NitroFunction<int, sizebuf_t*, char*>           CL_CheckFile;
        // void CL_SendConnectPacket()
        NitroFunctionVoid<>                             CL_SendConnectPacket;
        // void CL_ParseResourceList()
        NitroFunctionVoid<>                             CL_ParseResourceList;
        // bool CL_PrecacheResources()
        NitroFunction<bool>                             CL_PrecacheResources;
        // void CL_ReadPackets()
        NitroFunctionVoid<>                             CL_ReadPackets;
        // void CL_StartResourceDownloading(const char *pszMessage, int bCustom)
        NitroFunctionVoid<const char*, int>             CL_StartResourceDownloading;
        // void CL_ParseServerMessage(qboolean normal_message)
        NitroFunctionVoid<qboolean>                     CL_ParseServerMessage;
        // void CL_RegisterResources(sizebuf_t *msg)
        NitroFunctionVoid<sizebuf_t*>                   CL_RegisterResources;
        // void CL_MarkMapAsUsingHTTPDownload()
        NitroFunctionVoid<>                             CL_MarkMapAsUsingHTTPDownload;
        // void CL_SetSolidEntities()
        NitroFunctionVoid<>                             CL_SetSolidEntities;
        // void CL_ConnectionlessPacket()
        NitroFunctionVoid<>                             CL_ConnectionlessPacket;
        // void CL_ProcessFile_0(qboolean successfully_received, const char* filename)
        NitroFunctionVoid<qboolean, const char*>        CL_ProcessFile_0;
        // qboolean CL_RequestMissingResources()
        NitroFunction<qboolean>                         CL_RequestMissingResources;
        // void CL_InitSpectator()
        NitroFunctionVoid<>                             CL_InitSpectator;
        // void CL_ReallocateDynamicData(int maxclients)
        NitroFunctionVoid<int>                          CL_ReallocateDynamicData;
        // void Cvar_Set(const char *name, const char *value)
        NitroFunctionVoid<const char*, const char*>     Cvar_Set;
        // void SCR_UpdateScreen()
        NitroFunctionVoid<>                             SCR_UpdateScreen;
        // void BuildSurfaceDisplayList(msurface_t* fa)
        NitroFunctionVoid<msurface_t*>                  BuildSurfaceDisplayList;
        // void COM_InitArgv(int argc, char *argv[])
        NitroFunctionVoid<int, char**>                  COM_InitArgv;
        // qboolean COM_CreateCustomization(customization_t* pListHead, resource_t* pResource, int playernumber, int flags, customization_t** pCustomization, int* nLumps)
        NitroFunction<qboolean, customization_t*, resource_t*, int, int, customization_t**, int*> COM_CreateCustomization;
        // void SetCareerAudioState(int paused)
        NitroFunctionVoid<int>                          SetCareerAudioState;
        // void Cmd_TokenizeString(const char *text)
        NitroFunctionVoid<const char*>                  Cmd_TokenizeString;
        // void Draw_DecalSetName(int decal, const char* name)
        NitroFunctionVoid<int, const char*>             Draw_DecalSetName;

        // int Info_IsKeyImportant(const char* key)
        NitroFunction<int, const char*>                 Info_IsKeyImportant;

        // char* Cbuf_AddText(const char *text, sizebuf_t *buf)
        NitroFunction<char*, const char*, sizebuf_t*>   Cbuf_AddText;
        // char* Con_Printf(const char *format, ...);
        NitroFunctionTyped<EngCon_PrintfFunc, char*, const char*>  Con_Printf;
        // char* Con_DPrintf(const char *format, ...);
        NitroFunctionTyped<EngCon_PrintfFunc, char*, const char*>  Con_DPrintf;
        // void Mem_Free(void *data)
        NitroFunctionVoid<void*>                        Mem_Free;
        // void* Mem_Malloc(size_t size)
        NitroFunction<void*, size_t>                    Mem_Malloc;
        // void* Mem_ZeroMalloc(size_t size)
        NitroFunction<void*, size_t>                    Mem_ZeroMalloc;
        // char* Mem_Strdup(const char* strSource)
        NitroFunction<char*, const char*>               Mem_Strdup;
        // char* SZ_Write(sizebuf_t *buf, char *text, int length)
        NitroFunction<char*, sizebuf_t*, char*, int>    SZ_Write;
        // void SZ_Clear(sizebuf_t *buf)
        NitroFunctionVoid<sizebuf_t*>                   SZ_Clear;
        // signed int Netchan_CopyFileFragments(netchan_t *chan);
        NitroFunction<signed int, netchan_t*>           Netchan_CopyFileFragments;
        // void Netchan_CreateFragments(qboolean server, netchan_t *chan, sizebuf_t *msg)
        NitroFunctionVoid<qboolean, netchan_t*, sizebuf_t*> Netchan_CreateFragments;
        // void Netchan_FragSend(netchan_t *chan)
        NitroFunctionVoid<netchan_t*>                   Netchan_FragSend;
        // void Netchan_UpdateProgress(netchan_t *chan)
        NitroFunctionVoid<netchan_t*>                   Netchan_UpdateProgress;
        // void Netchan_FlushIncoming(netchan_t *chan, int stream)
        NitroFunctionVoid<netchan_t*, int>              Netchan_FlushIncoming;
        // qboolean Netchan_Process(netchan_t *chan)
        NitroFunction<qboolean, netchan_t*>             Netchan_Process;
        // qboolean Netchan_CopyNormalFragments(netchan_t *chan)
        NitroFunction<qboolean, netchan_t*>             Netchan_CopyNormalFragments;
        // VoiceRecordInterface* CreateVoiceRecord(int sample_rate)
        NitroFunction<VoiceRecordInterface*, int>       CreateVoiceRecord_DSound;
        // VoiceRecordInterface* CreateVoiceRecord(int sample_rate)
        NitroFunction<VoiceRecordInterface*, int>       CreateVoiceRecord_WaveIn;

        // void MSG_BeginReading()
        NitroFunctionVoid<>                             MSG_BeginReading;
        // void MSG_StartBitReading(sizebuf_t *buf)
        NitroFunctionVoid<sizebuf_t*>                   MSG_StartBitReading;
        // void MSG_EndBitReading(sizebuf_t *buf)
        NitroFunctionVoid<sizebuf_t*>                   MSG_EndBitReading;
        // int MSG_ReadBits(int numbits)
        NitroFunction<int, int>                         MSG_ReadBits;
        // char* MSG_ReadBitString()
        NitroFunction<char*>                            MSG_ReadBitString;
        // int MSG_ReadBitData(void *dest, int length)
        NitroFunction<char, void*, int>                 MSG_ReadBitData;
        // int MSG_ReadByte()
        NitroFunction<int>                              MSG_ReadByte;
        // const char* MSG_ReadString()
        NitroFunction<const char*>                      MSG_ReadString;
        // int MSG_ReadLong()
        NitroFunction<int>                              MSG_ReadLong;
        // char* MSG_ReadStringLine()
        NitroFunction<char*>                            MSG_ReadStringLine;
        // void MSG_WriteByte(sizebuf_t *sb, int c)
        NitroFunctionVoid<sizebuf_t*, int>              MSG_WriteByte;
        // void MSG_WriteString(sizebuf_t *sb, const char *s)
        NitroFunctionVoid<sizebuf_t*, const char*>      MSG_WriteString;

        // int NET_SendPacket(netsrc_t sock, int length, void *data, netadr_t to)
        NitroFunction<int, netsrc_t, int, void*, netadr_t> NET_SendPacket;
        // qboolean NET_GetPacket(netsrc_t sock)
        NitroFunction<qboolean, netsrc_t>               NET_GetPacket;
        // qboolean NET_GetPacket_0()
        NitroFunction<qboolean>                         NET_GetPacket_0;
        // void Net_APICheckTimeouts()
        NitroFunctionVoid<>                             Net_APICheckTimeouts;

        // bool Sys_InitGame(char *lpOrgCmdLine, char *pBaseDir, void *pwnd, int bIsDedicated)
        NitroFunction<bool, char*, char*, void*, int>   Sys_InitGame;
        // void Sys_Error(const char *error, ...)
        NitroFunctionVoidTyped<Sys_ErrorFunc, const char*> Sys_Error;
        // void Sys_Printf(const char* format, ...)
        NitroFunctionVoidTyped<Sys_PrintfFunc, const char*> Sys_Printf;
        // void Host_WriteConfiguration()
        NitroFunctionVoid<>                             Host_WriteConfiguration;
        // int Host_Frame(float delta, int iState, int *stateInfo)
        NitroFunction<int, float, int, int*>            Host_Frame;
        // void Host_FrameInternal(float delta)
        NitroFunctionVoid<float>                        Host_FrameInternal;
        // int Host_FilterTime(float delta)
        NitroFunction<int, float>                       Host_FilterTime;
        // void Host_Map_f()
        NitroFunctionVoid<>                             Host_Map_f;
        // void Host_Map(qboolean bIsDemo, const char* mapstring, const char* mapName, qboolean loadGame)
        NitroFunctionVoid<qboolean, const char*, const char*, qboolean> Host_Map;
        // void Con_MessageMode_f()
        NitroFunctionVoid<>                             Con_MessageMode_f;
        // void Con_MessageMode2_f()
        NitroFunctionVoid<>                             Con_MessageMode2_f;
        // void Key_Message(int key)
        NitroFunctionVoid<int>                          Key_Message;
        // void Host_InitializeGameDLL()
        NitroFunctionVoid<>                             Host_InitializeGameDLL;
        // void ForceReloadProfile()
        NitroFunctionVoid<>                             ForceReloadProfile;

        // void VGuiWrap2_ConPrintf(const char *msg)
        NitroFunctionVoid<const char *>                 VGuiWrap2_ConPrintf;
        // void SystemWrapper_RunFrame(double delta)
        NitroFunctionVoid<double>                       SystemWrapper_RunFrame;

        // void ClientDLL_Frame(double delta)
        NitroFunctionVoid<double>                       ClientDLL_Frame;
        // void ClientDLL_Init()
        NitroFunctionVoid<>                             ClientDLL_Init;
        // void ClientDLL_Shutdown()
        //NitroFunctionVoid<>                           ClientDLL_Shutdown;
        // void AddStartupTiming(const char *name)
        NitroFunctionVoid<const char *>                 AddStartupTiming;
        // bool HPAK_GetDataPointer(const char *filename, resource_t *pResource, uint8_t **buffer, int *bufsize)
        NitroFunction<bool, const char*, resource_t*, uint8_t**, int*> HPAK_GetDataPointer;
        // void HPAK_AddLump(qboolean bUseQueue, const char* pakname, resource_t* pResource, uint8_t* pData, FileHandle_t fpSource)
        NitroFunctionVoid<qboolean, const char*, resource_t*, uint8_t*, FileHandle_t> HPAK_AddLump;
        // qboolean GL_SetMode(void* mainwindow, HDC* pmaindc, HGLRC* pbaseRC, const char* pszDriver, const char* pszCmdLine)
        NitroFunction<qboolean, void*, HDC*, HGLRC*, const char*, const char*> GL_SetMode;
        // void NotifyDedicatedServerUI(const char* message)
        NitroFunctionVoid<const char*>                  NotifyDedicatedServerUI;
        // void StartLoadingProgressBar(const char *loadingType, int numProgressPoints)
        NitroFunctionVoid<const char*, int>             StartLoadingProgressBar;
        // void SetLoadingProgressBarStatusText(const char *statusText)
        NitroFunctionVoid<const char*>                  SetLoadingProgressBarStatusText;
        // void Mod_ClearAll()
        NitroFunctionVoid<>                             Mod_ClearAll;
        // model_t* Mod_FindName(qboolean trackCRC, const char* name)
        NitroFunction<model_t*, qboolean, const char*>  Mod_FindName;
        // model_t* Mod_LoadModel(model_t* mod, qboolean crash, qboolean trackCRC)
        NitroFunction<model_t*, model_t*, qboolean, qboolean> Mod_LoadModel;
        // void Mod_Print()
        NitroFunctionVoid<>                             Mod_Print;
        // qboolean Mod_ValidateCRC(const char* name, CRC32_t crc)
        NitroFunction<qboolean, const char*, CRC32_t>   Mod_ValidateCRC;
        //void Mod_NeedCRC(const char* name, qboolean needCRC)
        NitroFunctionVoid<const char*, qboolean>        Mod_NeedCRC;
        // void Mod_LoadAliasModel(model_t* mod, void* buffer)
        NitroFunctionVoid<model_t*, void*>              Mod_LoadAliasModel;
        // void Mod_LoadSpriteModel(model_t* mod, void* buffer)
        NitroFunctionVoid<model_t*, void*>              Mod_LoadSpriteModel;
        // void Mod_LoadStudioModel(model_t* mod, void* buffer)
        NitroFunctionVoid<model_t*, void*>              Mod_LoadStudioModel;
        // void Mod_LoadBrushModel(model_t* mod, void* buffer)
        NitroFunctionVoid<model_t*, void*>              Mod_LoadBrushModel;
        // char* S_PrecacheSound(char* name)
        NitroFunction<sfx_t*, char*>                    S_PrecacheSound;
        // void SPR_Shutdown()
        NitroFunctionVoid<>                             SPR_Shutdown;
        // void SPR_Init()
        NitroFunctionVoid<>                             SPR_Init;
        // int* R_StudioReloadSkin(model_t* pModel, int index, skin_t* pskin)
        NitroFunction<int*, model_t*, int, skin_t*>     R_StudioReloadSkin;
        // void R_NewMap()
        NitroFunctionVoid<>                             R_NewMap;
        // void DT_LoadDetailMapFile(char* levelName)
        NitroFunctionVoid<char*>                        DT_LoadDetailMapFile;
        // void* Cache_Alloc(cache_user_t* c, int size, char* name)
        NitroFunction<void*, cache_user_t*, int, char*> Cache_Alloc;
        // void* Cache_Check(cache_user_t* c)
        NitroFunction<void*, cache_user_t*>             Cache_Check;
        // void Cache_Free(cache_user_t* c)
        NitroFunctionVoid<cache_user_t*>                Cache_Free;
        // void* Z_Malloc(int size)
        NitroFunction<void*, int>                       Z_Malloc;
        // void* Hunk_AllocName(int size, const char* name)
        NitroFunction<void*, int, const char*>          Hunk_AllocName;
        // void* Hunk_TempAlloc(int size)
        NitroFunction<void*, int>                       Hunk_TempAlloc;
        // void Hunk_Check()
        NitroFunctionVoid<>                             Hunk_Check;
        // void SVC_Nop()
        NitroFunctionVoid<>                             SVC_Nop;
        // void SVC_Disconnect()
        NitroFunctionVoid<>                             SVC_Disconnect;
        // void SVC_Event()
        NitroFunctionVoid<>                             SVC_Event;
        // void SVC_Version()
        NitroFunctionVoid<>                             SVC_Version;
        // void SVC_SetView()
        NitroFunctionVoid<>                             SVC_SetView;
        // void SVC_Sound()
        NitroFunctionVoid<>                             SVC_Sound;
        // void SVC_Time()
        NitroFunctionVoid<>                             SVC_Time;
        // void SVC_Print()
        NitroFunctionVoid<>                             SVC_Print;
        // void SVC_StuffText()
        NitroFunctionVoid<>                             SVC_StuffText;
        // void SVC_SetAngle()
        NitroFunctionVoid<>                             SVC_SetAngle;
        // void SVC_ResourceList()
        NitroFunctionVoid<>                             SVC_ResourceList;
        // void SVC_ResourceLocation()
        NitroFunctionVoid<>                             SVC_ResourceLocation;
        // void SVC_Director()
        NitroFunctionVoid<>                             SVC_Director;
        // void SVC_SignOnNum()
        NitroFunctionVoid<>                             SVC_SignOnNum;
        // void SVC_NewUserMsg()
        NitroFunctionVoid<>                             SVC_NewUserMsg;
        // void SVC_SendCvarValue()
        NitroFunctionVoid<>                             SVC_SendCvarValue;
        // void SVC_UpdateUserInfo()
        NitroFunctionVoid<>                             SVC_UpdateUserInfo;
        // void Netchan_Setup(netsrc_t socketnumber, netchan_t *chan, netadr_t adr, int player_slot, void *connection_status, qboolean(*pfnNetchan_Blocksize)(void *))
        NitroFunctionVoid<netsrc_t, netchan_t*, netadr_t, int, void*, qboolean(*)(void *)>  Netchan_Setup;
        // void SVC_TimeScale()
        NitroFunctionVoid<>                             SVC_TimeScale;

        cvar_s* sys_timescale                           = nullptr;

        // qboolean Cvar_Command()
        NitroFunction<qboolean>                         Cvar_Command;
        // int MSG_ReadFloat()
        NitroFunction<float>                            MSG_ReadFloat;
        // void CL_WriteDemoStartup(int start, sizebuf_t* msg)
        NitroFunctionVoid<int, sizebuf_t*>              CL_WriteDemoStartup;
        // void CL_WriteDemoMessage(int start, sizebuf_t* msg)
        NitroFunctionVoid<int, sizebuf_t*>              CL_WriteDemoMessage;
        // void CL_Record_f()
        NitroFunctionVoid<>                             CL_Record_f;
        // sfxcache_t* S_LoadSound(sfx_t* sound, channel_t* channel)
        NitroFunction<sfxcache_t*, sfx_t*, channel_t*>  S_LoadSound;
        // sfxcache_t* VoiceSE_GetSFXCache(sfx_t* sound)
        NitroFunction<sfxcache_t*, sfx_t*>              VoiceSE_GetSFXCache;
        // double Sys_FloatTime()
        NitroFunction<double>                           Sys_FloatTime;

        dma_t** shm                                     = nullptr;
        wavstream_t** wavstreams                        = nullptr;
        channel_t** channels                            = nullptr;

        // HSPRITE_t SPR_Load(const char *pTextureName);
        NitroFunction<HSPRITE_t, const char*>           SPR_Load;

        // void GL_UnloadTexture(const char *identifier)
        NitroFunctionVoid<const char*>                  GL_UnloadTexture;

        qboolean* host_initialized                      = nullptr;
        cl_enginefunc_dst_t* g_engdstAddrs              = nullptr;

        // void SPR_Set(HSPRITE_t hsprite, int r, int g, int b)
        NitroFunctionVoid<HSPRITE_t, int, int, int>     SPR_Set;
        // int SPR_Width(HSPRITE_t hsprite, int frame)
        NitroFunction<int, HSPRITE_t, int>              SPR_Width;
        // int SPR_Height(HSPRITE_t hsprite, int frame)
        NitroFunction<int, HSPRITE_t, int>              SPR_Height;
        // int SPR_Frames(HSPRITE_t hsprite)
        NitroFunction<int, HSPRITE_t>                   SPR_Frames;
        // const model_t* SPR_GetModelPointer(HSPRITE_t hsprite)
        NitroFunction<const model_t*, HSPRITE_t>        SPR_GetModelPointer;
        // void SPR_Shutdown_NoModelFree()
        NitroFunctionVoid<>                             SPR_Shutdown_NoModelFree;

        viddef_t* vid                                   = nullptr;
        int* gl_mtexable                                = nullptr;
        int* r_framecount                               = nullptr;
        float* gVShake                                  = nullptr;
        int* gHostSpawnCount                            = nullptr;
        void* gGLPalette                                = nullptr;
        refdef_t* r_refdef                              = nullptr;
        int* currenttexture                             = nullptr;

        // void SPR_Draw(int frame, int x, int y, const wrect_t *prc)
        NitroFunctionVoid<int, int, int, const wrect_t*> SPR_Draw;
        // void SPR_DrawAdditive(int frame, int x, int y, const wrect_t* prc)
        NitroFunctionVoid<int, int, int, const wrect_t*> SPR_DrawAdditive;
        // void SPR_DrawHoles(int frame, int x, int y, const wrect_t* prc)
        NitroFunctionVoid<int, int, int, const wrect_t*> SPR_DrawHoles;
        // void SPR_DrawGeneric(int frame, int x, int y, const wrect_t* prc, int src, int dest, int width, int height)
        NitroFunctionVoid<int, int, int, const wrect_t*, int, int, int, int> SPR_DrawGeneric;
        // void SPR_EnableScissor(int x, int y, int width, int height)
        NitroFunctionVoid<int, int, int, int>           SPR_EnableScissor;
        // void SPR_DisableScissor()
        NitroFunctionVoid<>                             SPR_DisableScissor;
        // model_t* CL_GetModelByIndex(int index)
        NitroFunction<model_t*, int>                    CL_GetModelByIndex;
        // void GL_Init()
        NitroFunctionVoid<>                             GL_Init;
        // void DT_Initialize()
        NitroFunctionVoid<>                             DT_Initialize;
        // void GL_BuildLightmaps()
        NitroFunctionVoid<>                             GL_BuildLightmaps;
        // void Cvar_DirectSet(cvar_t* var, const char* value)
        NitroFunctionVoid<cvar_t*, const char*>         Cvar_DirectSet;
        // void V_Init()
        NitroFunctionVoid<>                             V_Init;
        // void R_Init()
        NitroFunctionVoid<>                             R_Init;
        // void R_RenderView()
        NitroFunctionVoid<>                             R_RenderView;
        // void GL_SelectTexture(GLenum target)
        NitroFunctionVoid<unsigned int>                 GL_SelectTexture;
        // int GL_LoadTexture(const char* identifier, GL_TEXTURETYPE textureType, int width, int height, uint8_t* data, int mipmap, int iType, uint8_t* pPal)
        NitroFunction<int, const char*, int, int, int, uint8_t*, int, int, uint8_t*> GL_LoadTexture;
        // void GL_Bind(int texnum)
        NitroFunctionVoid<int>                          GL_Bind;
        // void VGUI2_ResetCurrentTexture()
        NitroFunctionVoid<>                             VGUI2_ResetCurrentTexture;
        // void SetCrosshair(HSPRITE_t hspr, wrect_t rc, int r, int g, int b)
        NitroFunctionVoid<HSPRITE_t, wrect_t, int, int, int> SetCrosshair;
        // void DrawCrosshair(int x, int y)
        NitroFunctionVoid<int, int>                     DrawCrosshair;
        // void R_DrawTEntitiesOnList(qboolean clientOnly)
        NitroFunctionVoid<qboolean>                     R_DrawTEntitiesOnList;
        // int CL_FxBlend(cl_entity_t* ent)
        NitroFunction<int, cl_entity_t*>                CL_FxBlend;
        // float* R_GetAttachmentPoint(int entity, int attachment)
        NitroFunction<float*, int, int>                 R_GetAttachmentPoint;
        // float GlowBlend(cl_entity_t* pEntity)
        NitroFunction<float, cl_entity_t*>              GlowBlend;
        // void R_DrawSpriteModel(cl_entity_t* e)
        NitroFunctionVoid<cl_entity_t*>                 R_DrawSpriteModel;
        // void R_DrawBrushModel(cl_entity_t* e)
        NitroFunctionVoid<cl_entity_t*>                 R_DrawBrushModel;
        // void R_DrawAliasModel(cl_entity_t* e)
        NitroFunctionVoid<cl_entity_t*>                 R_DrawAliasModel;
        // void ClientDLL_DrawTransparentTriangles()
        NitroFunctionVoid<>                             ClientDLL_DrawTransparentTriangles;
        // qboolean CL_IsDevOverviewMode()
        NitroFunction<qboolean>                         CL_IsDevOverviewMode;
        // void CL_SetDevOverView(refdef_t* refdef)
        NitroFunctionVoid<refdef_t*>                    CL_SetDevOverView;
        // void R_SetupFrame()
        NitroFunctionVoid<>                             R_SetupFrame;
        // void R_SetFrustum()
        NitroFunctionVoid<>                             R_SetFrustum;
        // void R_SetupGL()
        NitroFunctionVoid<>                             R_SetupGL;
        // void R_MarkLeaves()
        NitroFunctionVoid<>                             R_MarkLeaves;
        // void R_DrawWorld()
        NitroFunctionVoid<>                             R_DrawWorld;
        // void S_ExtraUpdate()
        NitroFunctionVoid<>                             S_ExtraUpdate;
        // void R_RenderFinalFog()
		NitroFunctionVoid<>                             R_RenderFinalFog;
        // void ClientDLL_DrawNormalTriangles()
        NitroFunctionVoid<>                             ClientDLL_DrawNormalTriangles;
        // void R_RenderDlights()
        NitroFunctionVoid<>                             R_RenderDlights;
        // void R_DrawParticles()
        NitroFunctionVoid<>                             R_DrawParticles;
        // void R_DrawViewModel()
        NitroFunctionVoid<>                             R_DrawViewModel;
        // void R_PolyBlend()
        NitroFunctionVoid<>                             R_PolyBlend;
        // void AddTEntity(cl_entity_t* ent)
        NitroFunctionVoid<cl_entity_t*>                 AddTEntity;
        // void AppendTEntity(cl_entity_t* ent)
        NitroFunctionVoid<cl_entity_t*>                 AppendTEntity;
        // void R_PreDrawViewModel()
        NitroFunctionVoid<>                             R_PreDrawViewModel;
        // void R_Clear()
        NitroFunctionVoid<>                             R_Clear;

        cl_entity_t** currententity                     = nullptr;
        float* r_blend                                  = nullptr;
        vec3_t* r_entorigin                             = nullptr;
        qboolean* g_bUserFogOn                          = nullptr;
        int* numTransObjs                               = nullptr;
        transObjRef** transObjects                      = nullptr;
        qboolean* mtexenabled                           = nullptr;
        cl_entity_t* r_worldentity                      = nullptr;
        qboolean* mirror                                = nullptr;
        int* c_brush_polys                              = nullptr;
        int* c_alias_polys                              = nullptr;
        int* cl_numvisedicts                            = nullptr;
        cl_entity_t** cl_visedicts                      = nullptr;
        qboolean* isFogEnabled                          = nullptr;
        float* scr_fov_value                            = nullptr;
        float* g_SND_VoiceOverdrive                     = nullptr;
        int* cszrawsentences                            = nullptr;
        char* (*rgpszrawsentence)[CVOXFILESENTENCEMAX]  = nullptr;

        // void MSG_WriteShort(sizebuf_t* sb, uint16_t c)
        NitroFunctionVoid<sizebuf_t*, uint16_t>         MSG_WriteShort;
        // qboolean SNDDMA_Init()
        NitroFunction<qboolean>                         SNDDMA_Init;
        // void S_Init()
        NitroFunctionVoid<>                             S_Init;
        // void S_Shutdown()
        NitroFunctionVoid<>                             S_Shutdown;
        // sfx_t* S_FindName(const char* name, int* pfInCache)
        NitroFunction<sfx_t*, const char*, int*>        S_FindName;
        // void S_StartDynamicSound(int entnum, int entchannel, sfx_t* sfx, float* origin, float fvol, float attenuation, int flags, int pitch)
        NitroFunctionVoid<int, int, sfx_t*, float*, float, float, int, int> S_StartDynamicSound;
        // void S_StartStaticSound(int entnum, int entchannel, sfx_t* sfx, float* origin, float fvol, float attenuation, int flags, int pitch)
        NitroFunctionVoid<int, int, sfx_t*, float*, float, float, int, int> S_StartStaticSound;
        // void S_StopSound(int entnum, int entchannel)
        NitroFunctionVoid<int, int>                     S_StopSound;
        // void S_StopAllSounds(qboolean clear)
        NitroFunctionVoid<qboolean>                     S_StopAllSounds;
        // void S_Update(float* origin, float* forward, float* right, float* up)
        NitroFunctionVoid<float*, float*, float*, float*> S_Update;
        // void VoiceSE_NotifyFreeChannel(int iChannel)
        NitroFunctionVoid<int>                          VoiceSE_NotifyFreeChannel;
        // sentenceEntry_s* SequenceGetSentenceByIndex(unsigned int index)
        NitroFunction<sentenceEntry_s*, unsigned int>   SequenceGetSentenceByIndex;

        float* scr_con_current                          = nullptr;
        cvar_t* developer                               = nullptr;
        int* cmd_argc                                   = nullptr;
        char* (*cmd_argv)[80]                           = nullptr;
        cvar_t** cvar_vars                              = nullptr;
        keydest_t* key_dest                             = nullptr;

        // void CL_ConnectClient()
        NitroFunctionVoid<>                             CL_ConnectClient;
        // int CL_GetFragmentSize(void* state)
        NitroFunction<int, void*>                       CL_GetFragmentSize;
        // void NET_ClearLagData(qboolean bClient, qboolean bServer)
        NitroFunctionVoid<qboolean, qboolean>           NET_ClearLagData;
        // void* SZ_GetSpace(sizebuf_t* buf, int length)
        NitroFunction<void*, sizebuf_t*, int>           SZ_GetSpace;
        // void Host_Shutdown()
        NitroFunctionVoid<>                             Host_Shutdown;
        // void Host_Quit_f()
        NitroFunctionVoid<>                             Host_Quit_f;
        // void CL_ClearClientState()
        NitroFunctionVoid<>                             CL_ClearClientState;
        // qboolean MD5_Hash_File(unsigned char digest[16], char* pszFileName, int bUsefopen, int bSeed, unsigned int seed[4])
        NitroFunction<qboolean, unsigned char*, char*, int, int, unsigned int*> MD5_Hash_File;
        // void COM_ClearCustomizationList(customization_t* pHead, qboolean bCleanDecals)
        NitroFunctionVoid<customization_t*, qboolean>   COM_ClearCustomizationList;
        // void HPAK_FlushHostQueue()
        NitroFunctionVoid<>                             HPAK_FlushHostQueue;

        engine_studio_api_t* engine_studio_api          = nullptr;

        // void ClientDLL_HudInit()
        NitroFunctionVoid<>                             ClientDLL_HudInit;

        // "GL_ARB_multitexture "
        char (*data_arb_multitexture)[21]               = nullptr;
      
        // void GL_UnloadTextures()
        NitroFunctionVoid<>                             GL_UnloadTextures;
        // void Cbuf_InsertText(const char* text)
        NitroFunctionVoid<const char*>                  Cbuf_InsertText;
        // void* W_GetLumpName(int wad, const char *name)
        NitroFunction<void*, int, const char*>          W_GetLumpName;
        // void Draw_Init()
        NitroFunctionVoid<>                             Draw_Init;
        // void Draw_Shutdown()
        NitroFunctionVoid<>                             Draw_Shutdown;
        // qpic_t* Draw_PicFromWad(const char *name)
        NitroFunction<void*, const char*>               Draw_PicFromWad;
        // void Draw_CacheWadHandler(cachewad_t* wad, PFNCACHE fn, int extraDataSize)
        NitroFunctionVoid<cachewad_t*, PFNCACHE, int>   Draw_CacheWadHandler;
        // void Draw_FreeWad(cachewad_t* pWad)
        NitroFunctionVoid<cachewad_t*>                  Draw_FreeWad;
        // void Draw_CacheWadInit(const char* name, int cacheMax, cachewad_t* wad)
        NitroFunctionVoid<const char*, int, cachewad_t*>Draw_CacheWadInit;
        // void Draw_CacheWadInitFromFile(FileHandle_t hFile, int len, const char *name, int cacheMax, cachewad_t *wad)
        NitroFunctionVoid<FileHandle_t, int, const char*, int, cachewad_t*> Draw_CacheWadInitFromFile;
        // void Draw_MiptexTexture(cachewad_t* wad, unsigned char* data)
        NitroFunctionVoid<cachewad_t*, unsigned char*>  Draw_MiptexTexture;
        // void V_UpdatePalette()
        NitroFunctionVoid<>                             V_UpdatePalette;
        // qboolean V_CheckGamma()
        NitroFunction<qboolean>                         V_CheckGamma;
        // void BuildGammaTable(float g)
        NitroFunctionVoid<float>                        BuildGammaTable;
        // void GL_Config()
        NitroFunctionVoid<>                             GL_Config;
        // qboolean Cvar_HookVariable(const char* var_name, cvarhook_t* hook)
        NitroFunction<qboolean, const char*, void*>     Cvar_HookVariable;
        // void VGUI2_Draw_Init()
        NitroFunctionVoid<>                             VGUI2_Draw_Init;
        // void Cmd_AddCommand(char* cmd_name, xcommand_t function)
        NitroFunctionVoid<const char*, xcommand_t>      Cmd_AddCommand;
        // void VGUI2_Draw_SetTextColor(int r, int g, int b)
        NitroFunctionVoid<int, int, int>                VGUI2_Draw_SetTextColor;
        // qboolean DT_SetRenderState(int diffuseId)
        NitroFunction<qboolean, int>                    DT_SetRenderState;
        // void R_ForceCVars(qboolean mp)
        NitroFunctionVoid<qboolean>                     R_ForceCVars;
        // int GL_LoadTexture2(const char* identifier, int textureType, int width, int height, uint8_t* data, int mipmap, int iType, uint8_t* pPal, int filter)
        NitroFunction<int, const char*, int, int, int, uint8_t*, int, int, uint8_t*, int> GL_LoadTexture2;
        // void DT_LoadDetailTexture(const char *diffuseName, int diffuseId)
        NitroFunctionVoid<const char*, int>             DT_LoadDetailTexture;
        
        netadr_t* net_local_adr                         = nullptr;
        netadr_t* g_GameServerAddress                   = nullptr;
        float* g_LastScreenUpdateTime                   = nullptr;
        int* maxTransObjs                               = nullptr;
        int* gLoadSky                                   = nullptr;

        // void CL_ClearState(qboolean quiet)
        NitroFunctionVoid<qboolean>                     CL_ClearState;
        // void CL_StopPlayback()
        NitroFunctionVoid<>                             CL_StopPlayback;
        // void Host_ShutdownServer(qboolean crash)
        NitroFunctionVoid<qboolean>                     Host_ShutdownServer;
        // void Steam_GSTerminateGameConnection(uint32_t ip, uint16_t port)
        NitroFunctionVoid<uint32_t, uint16_t>           Steam_GSTerminateGameConnection;
        // void CL_Stop_f()
        NitroFunctionVoid<>                             CL_Stop_f;
        // qboolean NET_LeaveGroup(netsrc_t sock, netadr_t addr)
        NitroFunction<qboolean, netsrc_t, netadr_t>     NET_LeaveGroup;
        // void Netchan_Transmit(netchan_t* chan, int length, uint8_t* data)
        NitroFunctionVoid<netchan_t*, int, uint8_t*>    Netchan_Transmit;
        // void Netchan_Clear(netchan_t* chan)
        NitroFunctionVoid<netchan_t*>                   Netchan_Clear;
        // void StopLoadingProgressBar()
        NitroFunctionVoid<>                             StopLoadingProgressBar;
        // void CL_HookEvent(char* name, void (*pfnEvent)(event_args_s*))
        NitroFunctionVoid<char*, void(*)(struct event_args_s*)> CL_HookEvent;

        // int lightmap_textures[MAX_LIGHTMAPS]
        int (*lightmap_textures)[64]                    = nullptr;

        // vgui2::ISurface* g_pSurface, the surface the engine's own vgui2 code draws through
        void** vgui2_surface                            = nullptr;

        // bool VGui_InitInterfacesList(const char* module, CreateInterfaceFn* factories, int count)
        NitroFunction<bool, const char*, void*, int>    VGui_InitInterfacesList;
    };
}
