#pragma once

#include <concurrencpp/concurrencpp.h>
#include <nitroapi/NitroApiInterface.h>
#include <next_engine_mini/AnalyticsInterface.h>
#include <next_engine_mini/NextClientVersion.h>
#include <next_filesystem/IFileSystemNext.h>
#include <next_gameui/IGameConsoleNext.h>
#include <nitro_utils/config/ConfigProviderInterface.h>
#include "client/http_download/HttpDownloadManager.h"
#include "client/client_stateex.h"

extern NextClientVersion g_NextClientVersion;

extern nitroapi::NitroApiInterface* g_NitroApi;
extern std::shared_ptr<nitro_utils::ConfigProviderInterface> g_SettingGuard;
extern IFileSystem* g_pFileSystem;
extern IFileSystemNext* g_pFileSystemNext;
extern AnalyticsInterface* g_Analytics;
extern IGameUI* g_pGameUi;
extern IGameConsole* g_GameConsole;
extern IGameConsoleNext* g_GameConsoleNext;
extern vgui2::ILocalize* g_pLocalize;

extern cl_enginefunc_t gEngfuncs;
extern enginefuncs_t g_engfuncs;
extern sizebuf_t* net_message;
extern netadr_t* net_from;
extern int* pMsg_readcount;
extern double* realtime;
extern double* host_frametime;
extern qboolean* gfExtendedError;
const int kDownloadFileSize = 128;
extern char* gDownloadFile;
const int kDisconnectReasonSize = 256;
extern char* gszDisconnectReason;
extern client_stateex_t client_stateex;
extern cmd_source_t* cmd_source;
extern client_static_t* cls;
extern server_static_t* g_psvs;
extern server_t* g_psv;
extern qboolean* g_bMajorMapChange;
extern CareerStateType* g_careerState;
extern model_t** loadmodel;
extern int* cl_playerindex;
extern cl_entity_t** p_cl_entities;
extern bool* p_noclip_anglehack;
extern cache_user_t** p_loadcache;
extern unsigned char** p_loadbuf;
extern int* p_loadsize;
extern qboolean* p_host_initialized;
extern cl_enginefunc_dst_t* p_g_engdstAddrs;
extern nitroapi::viddef_t* p_vid;
extern int* p_gl_mtexable;
extern int* p_currenttexture;
extern cl_entity_t** p_currententity;
extern GLfloat* p_r_blend;
extern vec3_t* p_r_entorigin;
extern qboolean* p_g_bUserFogOn;
extern int* p_numTransObjs;
extern nitroapi::transObjRef** p_transObjects;
extern qboolean* p_mtexenabled;
extern cl_entity_t* p_r_worldentity;
extern qboolean* p_mirror;
extern int* p_c_brush_polys;
extern int* p_c_alias_polys;
extern int* p_cl_numvisedicts;
extern cl_entity_t** p_cl_visedicts;
extern qboolean* p_isFogEnabled;
extern refdef_t* r_refdef;
extern float* p_scr_fov_value;
extern float* p_g_SND_VoiceOverdrive;
extern int* p_cszrawsentences;
extern char *(*p_rgpszrawsentence)[CVOXFILESENTENCEMAX];
extern float* p_scr_con_current;

extern cvar_t* fs_startup_timings;
extern cvar_t* fs_lazy_precache;
extern cvar_t* fs_precache_timings;
extern cvar_t* cl_download_ingame;
extern cvar_t* cl_shownet;
extern cvar_t* cl_timeout;
extern cvar_t* scr_downloading;
extern cvar_t* host_framerate;
extern cvar_t* sys_ticrate;
extern cvar_t* gl_vsync;
extern cvar_t* fps_override;
extern cvar_t* fps_max;
extern cvar_t* sys_timescale;
extern cvar_t* developer;
extern cvar_t* sv_cheats;
extern cvar_t* gl_spriteblend;
extern cvar_t* r_drawentities;
extern cvar_t* r_norefresh;
extern cvar_t* r_speeds;
extern cvar_t* viewmodel_fov;

extern bool g_bIsDedicatedServer;

nitroapi::NitroApiInterface* napi();
nitroapi::EngineData* eng();
nitroapi::ClientData* client();
nitroapi::SDL2Data* sdl2();

int COM_SizeofResourceList(resource_t *pList, resourceinfo_t *ri);

// client/cl_custom.cpp
bool IsSafeFileToDownload(const std::string &filename);
void CL_AddToResourceList(resource_t *pResource, resource_t *pList);
void CL_RemoveFromResourceList(resource_t *pResource);
void CL_MoveToOnHandList(resource_t *pResource);

// client/cl_parse.cpp
void CL_StartResourceDownloading(const char *pszMessage, bool bCustom);

// common/hpak.cpp
bool HPAK_GetDataPointer(const char *filename, resource_t *pResource, uint8_t **buffer, int *bufsize);
void HPAK_AddLump(qboolean bUseQueue, const char* pakname, resource_t* pResource, uint8_t* pData, FileHandle_t fpSource);

// cl_pmove.cpp
void CL_SetSolidEntities();

// host_cmd.cpp
void Host_Map_f();
