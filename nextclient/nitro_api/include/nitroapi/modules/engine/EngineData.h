#pragma once

#include "../../chain/HandlersChainInterface.h"
#include "../../NitroFunction.h"
#include "IEngine.h"
#include <goldsrc/client_api.h>
#include <goldsrc/cvar.h>
#include <goldsrc/engine/client_state.h>
#include <goldsrc/engine/cmd.h>
#include <goldsrc/engine/model.h>
#include <goldsrc/engine/net.h>
#include <goldsrc/engine/resource.h>
#include <goldsrc/engine/server_state.h>
#include <goldsrc/engine/sound.h>
#include <goldsrc/engine/user_msg.h>
#include <goldsrc/engine/video.h>
#include <goldsrc/entity.h>
#include <goldsrc/event_api.h>
#include <goldsrc/fwd.h>
#include <goldsrc/keys.h>
#include <goldsrc/server_api.h>
#include <goldsrc/studio_api.h>
#include <goldsrc/types.h>
#include <goldsrc/view.h>
#include <Windows.h>
#include <FileSystem.h>

#include <steam/steamtypes.h>

namespace nitroapi
{
    typedef char* (__cdecl *EngCon_PrintfFunc)(const char* format, ...);
    typedef void (__cdecl *Sys_ErrorFunc)(const char *error, ...);
    typedef void (__cdecl *Sys_PrintfFunc)(const char *format, ...);

    struct EngineData
    {
        goldsrc::PEngineMsg EngineMsgBase = nullptr;

        int* msg_readcount = nullptr;
        int* host_framecount = nullptr;
        double* realtime = nullptr;
        double* host_frametime = nullptr;
        goldsrc::qboolean* gfExtendedError = nullptr;
        char* gszDisconnectReason = nullptr;
        uint16_t* SampleRate = nullptr;
        char* gDownloadFile = nullptr;

        goldsrc::server_static_t* server_static = nullptr; // svs
        goldsrc::server_t* server = nullptr; // sv
        goldsrc::client_static_t* client_static = nullptr;
        goldsrc::client_state_t* client_state = nullptr;
        goldsrc::cl_enginefunc_t* cl_enginefunc = nullptr;
        goldsrc::enginefuncs_t* enginefuncs = nullptr;
        goldsrc::sizebuf_t* net_message = nullptr;
        goldsrc::netadr_t* net_from = nullptr;
        goldsrc::cldll_func_t* cldll_func = nullptr;
        goldsrc::modfuncs_t* modfuncs = nullptr;
        IEngine* eng = nullptr;
        goldsrc::cvar_t* scr_downloading = nullptr;
        goldsrc::cmd_source_t* cmd_source = nullptr;
        goldsrc::qboolean* bMajorMapChange = nullptr;
        goldsrc::CareerStateType* careerState = nullptr;
        goldsrc::model_t** loadmodel = nullptr;
        int* cl_playerindex = nullptr;
        int* com_argc = nullptr;
        char** com_argv = nullptr;
        goldsrc::cache_user_t** loadcache = nullptr;
        unsigned char** loadbuf = nullptr;
        int* loadsize = nullptr;
        goldsrc::sfx_t** known_sfx = nullptr;
        int* num_sfx = nullptr;
        goldsrc::cl_entity_t** cl_entities = nullptr;
        bool* noclip_anglehack = nullptr;
        goldsrc::model_t** currentmodel = nullptr;
        goldsrc::mvertex_t** r_pcurrentvertbase = nullptr;

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
        // void CL_SendConsistencyInfo(goldsrc::sizebuf_t *buf)
        NitroFunctionVoid<goldsrc::sizebuf_t*> CL_SendConsistencyInfo;
        // void CL_Init()
        NitroFunctionVoid<>                             CL_Init;
        // int CL_CheckFile(goldsrc::sizebuf_t *msg, char *filename)
        NitroFunction<int, goldsrc::sizebuf_t*, char*> CL_CheckFile;
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
        // void CL_ParseServerMessage(goldsrc::qboolean normal_message)
        NitroFunctionVoid<goldsrc::qboolean> CL_ParseServerMessage;
        // void CL_RegisterResources(goldsrc::sizebuf_t *msg)
        NitroFunctionVoid<goldsrc::sizebuf_t*> CL_RegisterResources;
        // void CL_MarkMapAsUsingHTTPDownload()
        NitroFunctionVoid<>                             CL_MarkMapAsUsingHTTPDownload;
        // void CL_SetSolidEntities()
        NitroFunctionVoid<>                             CL_SetSolidEntities;
        // void CL_ConnectionlessPacket()
        NitroFunctionVoid<>                             CL_ConnectionlessPacket;
        // void CL_ProcessFile_0(goldsrc::qboolean successfully_received, const char* filename)
        NitroFunctionVoid<goldsrc::qboolean, const char*> CL_ProcessFile_0;
        // goldsrc::qboolean CL_RequestMissingResources()
        NitroFunction<goldsrc::qboolean> CL_RequestMissingResources;
        // void CL_InitSpectator()
        NitroFunctionVoid<>                             CL_InitSpectator;
        // void CL_ReallocateDynamicData(int maxclients)
        NitroFunctionVoid<int>                          CL_ReallocateDynamicData;
        // void Cvar_Set(const char *name, const char *value)
        NitroFunctionVoid<const char*, const char*>     Cvar_Set;
        // void SCR_UpdateScreen()
        NitroFunctionVoid<>                             SCR_UpdateScreen;
        // void BuildSurfaceDisplayList(goldsrc::msurface_t* fa)
        NitroFunctionVoid<goldsrc::msurface_t*> BuildSurfaceDisplayList;
        // void COM_InitArgv(int argc, char *argv[])
        NitroFunctionVoid<int, char**>                  COM_InitArgv;
        // goldsrc::qboolean COM_CreateCustomization(goldsrc::customization_t* pListHead, goldsrc::resource_t* pResource, int playernumber, int flags, goldsrc::customization_t** pCustomization, int* nLumps)
        NitroFunction<goldsrc::qboolean, goldsrc::customization_t*, goldsrc::resource_t*, int, int, goldsrc::customization_t**, int*>
            COM_CreateCustomization;
        // void SetCareerAudioState(int paused)
        NitroFunctionVoid<int>                          SetCareerAudioState;
        // void Cmd_TokenizeString(const char *text)
        NitroFunctionVoid<const char*>                  Cmd_TokenizeString;
        // void Draw_DecalSetName(int decal, const char* name)
        NitroFunctionVoid<int, const char*>             Draw_DecalSetName;

        // int Info_IsKeyImportant(const char* key)
        NitroFunction<int, const char*>                 Info_IsKeyImportant;

        // char* Cbuf_AddText(const char *text, goldsrc::sizebuf_t *buf)
        NitroFunction<char*, const char*, goldsrc::sizebuf_t*> Cbuf_AddText;
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
        // char* SZ_Write(goldsrc::sizebuf_t *buf, char *text, int length)
        NitroFunction<char*, goldsrc::sizebuf_t*, char*, int> SZ_Write;
        // void SZ_Clear(goldsrc::sizebuf_t *buf)
        NitroFunctionVoid<goldsrc::sizebuf_t*> SZ_Clear;
        // signed int Netchan_CopyFileFragments(goldsrc::netchan_t *chan);
        NitroFunction<signed int, goldsrc::netchan_t*> Netchan_CopyFileFragments;
        // void Netchan_CreateFragments(goldsrc::qboolean server, goldsrc::netchan_t *chan, goldsrc::sizebuf_t *msg)
        NitroFunctionVoid<goldsrc::qboolean, goldsrc::netchan_t*, goldsrc::sizebuf_t*> Netchan_CreateFragments;
        // void Netchan_FragSend(goldsrc::netchan_t *chan)
        NitroFunctionVoid<goldsrc::netchan_t*> Netchan_FragSend;
        // void Netchan_UpdateProgress(goldsrc::netchan_t *chan)
        NitroFunctionVoid<goldsrc::netchan_t*> Netchan_UpdateProgress;
        // void Netchan_FlushIncoming(goldsrc::netchan_t *chan, int stream)
        NitroFunctionVoid<goldsrc::netchan_t*, int> Netchan_FlushIncoming;
        // goldsrc::qboolean Netchan_Process(goldsrc::netchan_t *chan)
        NitroFunction<goldsrc::qboolean, goldsrc::netchan_t*> Netchan_Process;
        // goldsrc::qboolean Netchan_CopyNormalFragments(goldsrc::netchan_t *chan)
        NitroFunction<goldsrc::qboolean, goldsrc::netchan_t*> Netchan_CopyNormalFragments;
        // goldsrc::VoiceRecordInterface* CreateVoiceRecord(int sample_rate)
        NitroFunction<goldsrc::VoiceRecordInterface*, int> CreateVoiceRecord_DSound;
        // goldsrc::VoiceRecordInterface* CreateVoiceRecord(int sample_rate)
        NitroFunction<goldsrc::VoiceRecordInterface*, int> CreateVoiceRecord_WaveIn;

        // void MSG_BeginReading()
        NitroFunctionVoid<>                             MSG_BeginReading;
        // void MSG_StartBitReading(goldsrc::sizebuf_t *buf)
        NitroFunctionVoid<goldsrc::sizebuf_t*> MSG_StartBitReading;
        // void MSG_EndBitReading(goldsrc::sizebuf_t *buf)
        NitroFunctionVoid<goldsrc::sizebuf_t*> MSG_EndBitReading;
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
        // void MSG_WriteByte(goldsrc::sizebuf_t *sb, int c)
        NitroFunctionVoid<goldsrc::sizebuf_t*, int> MSG_WriteByte;
        // void MSG_WriteString(goldsrc::sizebuf_t *sb, const char *s)
        NitroFunctionVoid<goldsrc::sizebuf_t*, const char*> MSG_WriteString;

        // int NET_SendPacket(goldsrc::netsrc_t sock, int length, void *data, goldsrc::netadr_t to)
        NitroFunction<int, goldsrc::netsrc_t, int, void*, goldsrc::netadr_t> NET_SendPacket;
        // goldsrc::qboolean NET_GetPacket(goldsrc::netsrc_t sock)
        NitroFunction<goldsrc::qboolean, goldsrc::netsrc_t> NET_GetPacket;
        // goldsrc::qboolean NET_GetPacket_0()
        NitroFunction<goldsrc::qboolean> NET_GetPacket_0;
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
        // void Host_Map(goldsrc::qboolean bIsDemo, const char* mapstring, const char* mapName, goldsrc::qboolean loadGame)
        NitroFunctionVoid<goldsrc::qboolean, const char*, const char*, goldsrc::qboolean> Host_Map;
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
        // bool HPAK_GetDataPointer(const char *filename, goldsrc::resource_t *pResource, uint8_t **buffer, int *bufsize)
        NitroFunction<bool, const char*, goldsrc::resource_t*, uint8_t**, int*> HPAK_GetDataPointer;
        // void HPAK_AddLump(goldsrc::qboolean bUseQueue, const char* pakname, goldsrc::resource_t* pResource, uint8_t* pData, FileHandle_t fpSource)
        NitroFunctionVoid<goldsrc::qboolean, const char*, goldsrc::resource_t*, uint8_t*, FileHandle_t> HPAK_AddLump;
        // goldsrc::qboolean GL_SetMode(void* mainwindow, HDC* pmaindc, HGLRC* pbaseRC, const char* pszDriver, const char* pszCmdLine)
        NitroFunction<goldsrc::qboolean, void*, HDC*, HGLRC*, const char*, const char*> GL_SetMode;
        // void NotifyDedicatedServerUI(const char* message)
        NitroFunctionVoid<const char*>                  NotifyDedicatedServerUI;
        // void StartLoadingProgressBar(const char *loadingType, int numProgressPoints)
        NitroFunctionVoid<const char*, int>             StartLoadingProgressBar;
        // void SetLoadingProgressBarStatusText(const char *statusText)
        NitroFunctionVoid<const char*>                  SetLoadingProgressBarStatusText;
        // void Mod_ClearAll()
        NitroFunctionVoid<>                             Mod_ClearAll;
        // goldsrc::model_t* Mod_FindName(goldsrc::qboolean trackCRC, const char* name)
        NitroFunction<goldsrc::model_t*, goldsrc::qboolean, const char*> Mod_FindName;
        // goldsrc::model_t* Mod_LoadModel(goldsrc::model_t* mod, goldsrc::qboolean crash, goldsrc::qboolean trackCRC)
        NitroFunction<goldsrc::model_t*, goldsrc::model_t*, goldsrc::qboolean, goldsrc::qboolean> Mod_LoadModel;
        // void Mod_Print()
        NitroFunctionVoid<>                             Mod_Print;
        // goldsrc::qboolean Mod_ValidateCRC(const char* name, goldsrc::CRC32_t crc)
        NitroFunction<goldsrc::qboolean, const char*, goldsrc::CRC32_t> Mod_ValidateCRC;
        //void Mod_NeedCRC(const char* name, goldsrc::qboolean needCRC)
        NitroFunctionVoid<const char*, goldsrc::qboolean> Mod_NeedCRC;
        // void Mod_LoadAliasModel(goldsrc::model_t* mod, void* buffer)
        NitroFunctionVoid<goldsrc::model_t*, void*> Mod_LoadAliasModel;
        // void Mod_LoadSpriteModel(goldsrc::model_t* mod, void* buffer)
        NitroFunctionVoid<goldsrc::model_t*, void*> Mod_LoadSpriteModel;
        // void Mod_LoadStudioModel(goldsrc::model_t* mod, void* buffer)
        NitroFunctionVoid<goldsrc::model_t*, void*> Mod_LoadStudioModel;
        // void Mod_LoadBrushModel(goldsrc::model_t* mod, void* buffer)
        NitroFunctionVoid<goldsrc::model_t*, void*> Mod_LoadBrushModel;
        // char* S_PrecacheSound(char* name)
        NitroFunction<goldsrc::sfx_t*, char*> S_PrecacheSound;
        // void SPR_Shutdown()
        NitroFunctionVoid<>                             SPR_Shutdown;
        // void SPR_Init()
        NitroFunctionVoid<>                             SPR_Init;
        // int* R_StudioReloadSkin(goldsrc::model_t* pModel, int index, goldsrc::skin_t* pskin)
        NitroFunction<int*, goldsrc::model_t*, int, goldsrc::skin_t*> R_StudioReloadSkin;
        // void R_NewMap()
        NitroFunctionVoid<>                             R_NewMap;
        // void DT_LoadDetailMapFile(char* levelName)
        NitroFunctionVoid<char*>                        DT_LoadDetailMapFile;
        // void* Cache_Alloc(goldsrc::cache_user_t* c, int size, char* name)
        NitroFunction<void*, goldsrc::cache_user_t*, int, char*> Cache_Alloc;
        // void* Cache_Check(goldsrc::cache_user_t* c)
        NitroFunction<void*, goldsrc::cache_user_t*> Cache_Check;
        // void Cache_Free(goldsrc::cache_user_t* c)
        NitroFunctionVoid<goldsrc::cache_user_t*> Cache_Free;
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
        // void Netchan_Setup(goldsrc::netsrc_t socketnumber, goldsrc::netchan_t *chan, goldsrc::netadr_t adr, int player_slot, void *connection_status, goldsrc::qboolean(*pfnNetchan_Blocksize)(void *))
        NitroFunctionVoid<goldsrc::netsrc_t, goldsrc::netchan_t*, goldsrc::netadr_t, int, void*, goldsrc::qboolean (*)(void*)>
            Netchan_Setup;
        // void SVC_TimeScale()
        NitroFunctionVoid<>                             SVC_TimeScale;

        goldsrc::cvar_t* sys_timescale = nullptr;

        // goldsrc::qboolean Cvar_Command()
        NitroFunction<goldsrc::qboolean> Cvar_Command;
        // int MSG_ReadFloat()
        NitroFunction<float>                            MSG_ReadFloat;
        // void CL_WriteDemoStartup(int start, goldsrc::sizebuf_t* msg)
        NitroFunctionVoid<int, goldsrc::sizebuf_t*> CL_WriteDemoStartup;
        // void CL_WriteDemoMessage(int start, goldsrc::sizebuf_t* msg)
        NitroFunctionVoid<int, goldsrc::sizebuf_t*> CL_WriteDemoMessage;
        // void CL_Record_f()
        NitroFunctionVoid<>                             CL_Record_f;
        // goldsrc::sfxcache_t* S_LoadSound(goldsrc::sfx_t* sound, goldsrc::channel_t* channel)
        NitroFunction<goldsrc::sfxcache_t*, goldsrc::sfx_t*, goldsrc::channel_t*> S_LoadSound;
        // goldsrc::sfxcache_t* VoiceSE_GetSFXCache(goldsrc::sfx_t* sound)
        NitroFunction<goldsrc::sfxcache_t*, goldsrc::sfx_t*> VoiceSE_GetSFXCache;
        // double Sys_FloatTime()
        NitroFunction<double>                           Sys_FloatTime;

        goldsrc::dma_t** shm = nullptr;
        goldsrc::wavstream_t** wavstreams = nullptr;
        goldsrc::channel_t** channels = nullptr;

        // goldsrc::HSPRITE_t SPR_Load(const char *pTextureName);
        NitroFunction<goldsrc::HSPRITE_t, const char*> SPR_Load;

        // void GL_UnloadTexture(const char *identifier)
        NitroFunctionVoid<const char*>                  GL_UnloadTexture;

        goldsrc::qboolean* host_initialized = nullptr;
        goldsrc::cl_enginefunc_dst_t* g_engdstAddrs = nullptr;

        // void SPR_Set(goldsrc::HSPRITE_t hsprite, int r, int g, int b)
        NitroFunctionVoid<goldsrc::HSPRITE_t, int, int, int> SPR_Set;
        // int SPR_Width(goldsrc::HSPRITE_t hsprite, int frame)
        NitroFunction<int, goldsrc::HSPRITE_t, int> SPR_Width;
        // int SPR_Height(goldsrc::HSPRITE_t hsprite, int frame)
        NitroFunction<int, goldsrc::HSPRITE_t, int> SPR_Height;
        // int SPR_Frames(goldsrc::HSPRITE_t hsprite)
        NitroFunction<int, goldsrc::HSPRITE_t> SPR_Frames;
        // const goldsrc::model_t* SPR_GetModelPointer(goldsrc::HSPRITE_t hsprite)
        NitroFunction<const goldsrc::model_t*, goldsrc::HSPRITE_t> SPR_GetModelPointer;
        // void SPR_Shutdown_NoModelFree()
        NitroFunctionVoid<>                             SPR_Shutdown_NoModelFree;

        goldsrc::viddef_t* vid = nullptr;
        int* gl_mtexable                                = nullptr;
        int* r_framecount                               = nullptr;
        float* gVShake                                  = nullptr;
        int* gHostSpawnCount                            = nullptr;
        void* gGLPalette                                = nullptr;
        goldsrc::refdef_t* r_refdef = nullptr;
        int* currenttexture                             = nullptr;

        // void SPR_Draw(int frame, int x, int y, const goldsrc::wrect_t *prc)
        NitroFunctionVoid<int, int, int, const goldsrc::wrect_t*> SPR_Draw;
        // void SPR_DrawAdditive(int frame, int x, int y, const goldsrc::wrect_t* prc)
        NitroFunctionVoid<int, int, int, const goldsrc::wrect_t*> SPR_DrawAdditive;
        // void SPR_DrawHoles(int frame, int x, int y, const goldsrc::wrect_t* prc)
        NitroFunctionVoid<int, int, int, const goldsrc::wrect_t*> SPR_DrawHoles;
        // void SPR_DrawGeneric(int frame, int x, int y, const goldsrc::wrect_t* prc, int src, int dest, int width, int height)
        NitroFunctionVoid<int, int, int, const goldsrc::wrect_t*, int, int, int, int> SPR_DrawGeneric;
        // void SPR_EnableScissor(int x, int y, int width, int height)
        NitroFunctionVoid<int, int, int, int>           SPR_EnableScissor;
        // void SPR_DisableScissor()
        NitroFunctionVoid<>                             SPR_DisableScissor;
        // goldsrc::model_t* CL_GetModelByIndex(int index)
        NitroFunction<goldsrc::model_t*, int> CL_GetModelByIndex;
        // void GL_Init()
        NitroFunctionVoid<>                             GL_Init;
        // void DT_Initialize()
        NitroFunctionVoid<>                             DT_Initialize;
        // void GL_BuildLightmaps()
        NitroFunctionVoid<>                             GL_BuildLightmaps;
        // void Cvar_DirectSet(goldsrc::cvar_t* var, const char* value)
        NitroFunctionVoid<goldsrc::cvar_t*, const char*> Cvar_DirectSet;
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
        // void SetCrosshair(goldsrc::HSPRITE_t hspr, goldsrc::wrect_t rc, int r, int g, int b)
        NitroFunctionVoid<goldsrc::HSPRITE_t, goldsrc::wrect_t, int, int, int> SetCrosshair;
        // void DrawCrosshair(int x, int y)
        NitroFunctionVoid<int, int>                     DrawCrosshair;
        // void R_DrawTEntitiesOnList(goldsrc::qboolean clientOnly)
        NitroFunctionVoid<goldsrc::qboolean> R_DrawTEntitiesOnList;
        // int CL_FxBlend(goldsrc::cl_entity_t* ent)
        NitroFunction<int, goldsrc::cl_entity_t*> CL_FxBlend;
        // float* R_GetAttachmentPoint(int entity, int attachment)
        NitroFunction<float*, int, int>                 R_GetAttachmentPoint;
        // float GlowBlend(goldsrc::cl_entity_t* pEntity)
        NitroFunction<float, goldsrc::cl_entity_t*> GlowBlend;
        // void R_DrawSpriteModel(goldsrc::cl_entity_t* e)
        NitroFunctionVoid<goldsrc::cl_entity_t*> R_DrawSpriteModel;
        // void R_DrawBrushModel(goldsrc::cl_entity_t* e)
        NitroFunctionVoid<goldsrc::cl_entity_t*> R_DrawBrushModel;
        // void R_DrawAliasModel(goldsrc::cl_entity_t* e)
        NitroFunctionVoid<goldsrc::cl_entity_t*> R_DrawAliasModel;
        // void ClientDLL_DrawTransparentTriangles()
        NitroFunctionVoid<>                             ClientDLL_DrawTransparentTriangles;
        // goldsrc::qboolean CL_IsDevOverviewMode()
        NitroFunction<goldsrc::qboolean> CL_IsDevOverviewMode;
        // void CL_SetDevOverView(goldsrc::refdef_t* refdef)
        NitroFunctionVoid<goldsrc::refdef_t*> CL_SetDevOverView;
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
        // void AddTEntity(goldsrc::cl_entity_t* ent)
        NitroFunctionVoid<goldsrc::cl_entity_t*> AddTEntity;
        // void AppendTEntity(goldsrc::cl_entity_t* ent)
        NitroFunctionVoid<goldsrc::cl_entity_t*> AppendTEntity;
        // void R_PreDrawViewModel()
        NitroFunctionVoid<>                             R_PreDrawViewModel;
        // void R_Clear()
        NitroFunctionVoid<>                             R_Clear;

        goldsrc::cl_entity_t** currententity = nullptr;
        float* r_blend                                  = nullptr;
        vec3_t* r_entorigin                             = nullptr;
        goldsrc::qboolean* g_bUserFogOn = nullptr;
        int* numTransObjs                               = nullptr;
        goldsrc::transObjRef** transObjects = nullptr;
        goldsrc::qboolean* mtexenabled = nullptr;
        goldsrc::cl_entity_t* r_worldentity = nullptr;
        goldsrc::qboolean* mirror = nullptr;
        int* c_brush_polys                              = nullptr;
        int* c_alias_polys                              = nullptr;
        int* cl_numvisedicts                            = nullptr;
        goldsrc::cl_entity_t** cl_visedicts = nullptr;
        goldsrc::qboolean* isFogEnabled = nullptr;
        float* scr_fov_value                            = nullptr;
        float* g_SND_VoiceOverdrive                     = nullptr;
        int* cszrawsentences                            = nullptr;
        char* (*rgpszrawsentence)[goldsrc::CVOXFILESENTENCEMAX] = nullptr;

        // void MSG_WriteShort(goldsrc::sizebuf_t* sb, uint16_t c)
        NitroFunctionVoid<goldsrc::sizebuf_t*, uint16_t> MSG_WriteShort;
        // goldsrc::qboolean SNDDMA_Init()
        NitroFunction<goldsrc::qboolean> SNDDMA_Init;
        // void S_Init()
        NitroFunctionVoid<>                             S_Init;
        // void S_Shutdown()
        NitroFunctionVoid<>                             S_Shutdown;
        // goldsrc::sfx_t* S_FindName(const char* name, int* pfInCache)
        NitroFunction<goldsrc::sfx_t*, const char*, int*> S_FindName;
        // void S_StartDynamicSound(int entnum, int entchannel, goldsrc::sfx_t* sfx, float* origin, float fvol, float attenuation, int flags, int pitch)
        NitroFunctionVoid<int, int, goldsrc::sfx_t*, float*, float, float, int, int> S_StartDynamicSound;
        // void S_StartStaticSound(int entnum, int entchannel, goldsrc::sfx_t* sfx, float* origin, float fvol, float attenuation, int flags, int pitch)
        NitroFunctionVoid<int, int, goldsrc::sfx_t*, float*, float, float, int, int> S_StartStaticSound;
        // void S_StopSound(int entnum, int entchannel)
        NitroFunctionVoid<int, int>                     S_StopSound;
        // void S_StopAllSounds(goldsrc::qboolean clear)
        NitroFunctionVoid<goldsrc::qboolean> S_StopAllSounds;
        // void S_Update(float* origin, float* forward, float* right, float* up)
        NitroFunctionVoid<float*, float*, float*, float*> S_Update;
        // void VoiceSE_NotifyFreeChannel(int iChannel)
        NitroFunctionVoid<int>                          VoiceSE_NotifyFreeChannel;
        // goldsrc::sentenceEntry_s* SequenceGetSentenceByIndex(unsigned int index)
        NitroFunction<goldsrc::sentenceEntry_s*, unsigned int> SequenceGetSentenceByIndex;

        float* scr_con_current                          = nullptr;
        goldsrc::cvar_t* developer = nullptr;
        int* cmd_argc                                   = nullptr;
        char* (*cmd_argv)[80]                           = nullptr;
        goldsrc::cvar_t** cvar_vars = nullptr;
        goldsrc::keydest_t* key_dest = nullptr;

        // void CL_ConnectClient()
        NitroFunctionVoid<>                             CL_ConnectClient;
        // int CL_GetFragmentSize(void* state)
        NitroFunction<int, void*>                       CL_GetFragmentSize;
        // void NET_ClearLagData(goldsrc::qboolean bClient, goldsrc::qboolean bServer)
        NitroFunctionVoid<goldsrc::qboolean, goldsrc::qboolean> NET_ClearLagData;
        // void* SZ_GetSpace(goldsrc::sizebuf_t* buf, int length)
        NitroFunction<void*, goldsrc::sizebuf_t*, int> SZ_GetSpace;
        // void Host_Shutdown()
        NitroFunctionVoid<>                             Host_Shutdown;
        // void Host_Quit_f()
        NitroFunctionVoid<>                             Host_Quit_f;
        // void CL_ClearClientState()
        NitroFunctionVoid<>                             CL_ClearClientState;
        // goldsrc::qboolean MD5_Hash_File(unsigned char digest[16], char* pszFileName, int bUsefopen, int bSeed, unsigned int seed[4])
        NitroFunction<goldsrc::qboolean, unsigned char*, char*, int, int, unsigned int*> MD5_Hash_File;
        // void COM_ClearCustomizationList(goldsrc::customization_t* pHead, goldsrc::qboolean bCleanDecals)
        NitroFunctionVoid<goldsrc::customization_t*, goldsrc::qboolean> COM_ClearCustomizationList;
        // void HPAK_FlushHostQueue()
        NitroFunctionVoid<>                             HPAK_FlushHostQueue;

        goldsrc::engine_studio_api_t* engine_studio_api = nullptr;

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
        // goldsrc::qpic_t* Draw_PicFromWad(const char *name)
        NitroFunction<void*, const char*>               Draw_PicFromWad;
        // void Draw_CacheWadHandler(goldsrc::cachewad_t* wad, goldsrc::PFNCACHE fn, int extraDataSize)
        NitroFunctionVoid<goldsrc::cachewad_t*, goldsrc::PFNCACHE, int> Draw_CacheWadHandler;
        // void Draw_FreeWad(goldsrc::cachewad_t* pWad)
        NitroFunctionVoid<goldsrc::cachewad_t*> Draw_FreeWad;
        // void Draw_CacheWadInit(const char* name, int cacheMax, goldsrc::cachewad_t* wad)
        NitroFunctionVoid<const char*, int, goldsrc::cachewad_t*> Draw_CacheWadInit;
        // void Draw_CacheWadInitFromFile(FileHandle_t hFile, int len, const char *name, int cacheMax, goldsrc::cachewad_t *wad)
        NitroFunctionVoid<FileHandle_t, int, const char*, int, goldsrc::cachewad_t*> Draw_CacheWadInitFromFile;
        // void Draw_MiptexTexture(goldsrc::cachewad_t* wad, unsigned char* data)
        NitroFunctionVoid<goldsrc::cachewad_t*, unsigned char*> Draw_MiptexTexture;
        // void V_UpdatePalette()
        NitroFunctionVoid<>                             V_UpdatePalette;
        // goldsrc::qboolean V_CheckGamma()
        NitroFunction<goldsrc::qboolean> V_CheckGamma;
        // void BuildGammaTable(float g)
        NitroFunctionVoid<float>                        BuildGammaTable;
        // void GL_Config()
        NitroFunctionVoid<>                             GL_Config;
        // goldsrc::qboolean Cvar_HookVariable(const char* var_name, cvarhook_t* hook)
        NitroFunction<goldsrc::qboolean, const char*, void*> Cvar_HookVariable;
        // void VGUI2_Draw_Init()
        NitroFunctionVoid<>                             VGUI2_Draw_Init;
        // void Cmd_AddCommand(char* cmd_name, goldsrc::xcommand_t function)
        NitroFunctionVoid<const char*, goldsrc::xcommand_t> Cmd_AddCommand;
        // void VGUI2_Draw_SetTextColor(int r, int g, int b)
        NitroFunctionVoid<int, int, int>                VGUI2_Draw_SetTextColor;
        // goldsrc::qboolean DT_SetRenderState(int diffuseId)
        NitroFunction<goldsrc::qboolean, int> DT_SetRenderState;
        // void R_ForceCVars(goldsrc::qboolean mp)
        NitroFunctionVoid<goldsrc::qboolean> R_ForceCVars;
        // int GL_LoadTexture2(const char* identifier, int textureType, int width, int height, uint8_t* data, int mipmap, int iType, uint8_t* pPal, int filter)
        NitroFunction<int, const char*, int, int, int, uint8_t*, int, int, uint8_t*, int> GL_LoadTexture2;
        // void DT_LoadDetailTexture(const char *diffuseName, int diffuseId)
        NitroFunctionVoid<const char*, int>             DT_LoadDetailTexture;
        
        goldsrc::netadr_t* net_local_adr = nullptr;
        goldsrc::netadr_t* g_GameServerAddress = nullptr;
        float* g_LastScreenUpdateTime                   = nullptr;
        int* maxTransObjs                               = nullptr;
        int* gLoadSky                                   = nullptr;

        // void CL_ClearState(goldsrc::qboolean quiet)
        NitroFunctionVoid<goldsrc::qboolean> CL_ClearState;
        // void CL_StopPlayback()
        NitroFunctionVoid<>                             CL_StopPlayback;
        // void Host_ShutdownServer(goldsrc::qboolean crash)
        NitroFunctionVoid<goldsrc::qboolean> Host_ShutdownServer;
        // void Steam_GSTerminateGameConnection(uint32_t ip, uint16_t port)
        NitroFunctionVoid<uint32_t, uint16_t>           Steam_GSTerminateGameConnection;
        // void CL_Stop_f()
        NitroFunctionVoid<>                             CL_Stop_f;
        // goldsrc::qboolean NET_LeaveGroup(goldsrc::netsrc_t sock, goldsrc::netadr_t addr)
        NitroFunction<goldsrc::qboolean, goldsrc::netsrc_t, goldsrc::netadr_t> NET_LeaveGroup;
        // void Netchan_Transmit(goldsrc::netchan_t* chan, int length, uint8_t* data)
        NitroFunctionVoid<goldsrc::netchan_t*, int, uint8_t*> Netchan_Transmit;
        // void Netchan_Clear(goldsrc::netchan_t* chan)
        NitroFunctionVoid<goldsrc::netchan_t*> Netchan_Clear;
        // void StopLoadingProgressBar()
        NitroFunctionVoid<>                             StopLoadingProgressBar;
        // void CL_HookEvent(char* name, void (*pfnEvent)(goldsrc::event_args_t*))
        NitroFunctionVoid<char*, void (*)(goldsrc::event_args_t*)> CL_HookEvent;

        // int lightmap_textures[MAX_LIGHTMAPS]
        int (*lightmap_textures)[64]                    = nullptr;

        // vgui2::ISurface* g_pSurface, the surface the engine's own vgui2 code draws through
        void** vgui2_surface                            = nullptr;

        // bool VGui_InitInterfacesList(const char* module, CreateInterfaceFn* factories, int count)
        NitroFunction<bool, const char*, void*, int>    VGui_InitInterfacesList;
    };
}
