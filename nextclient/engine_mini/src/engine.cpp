#include "engine.h"

#include <IGameConsole.h>
#include <nitro_utils/poor_reflection_utils.h>
#include <nitro_utils/string_utils.h>
#include <nitro_utils/PtrValidator.h>
#include <next_engine_mini/engine_mini.h>
#include <tier2/tier2.h>
#include <utils/TaskRun.h>

#include "common/common.h"
#include "common/net_chan.h"
#include "common/model.h"
#include "common/zone.h"
#include "common/host.h"
#include "common/sys_dll.h"
#include "graphics/gl_local.h"
#include "client/client.h"
#include "client/cl_main.h"
#include "client/download.h"
#include "client/spriteapi.h"
#include "client/cl_private_resources.h"
#include "client/cl_parsefn.h"
#include "client/cl_game.h"
#include "browser_extensions/entry.h"
#include "audio/include/metaaudio.h"
#include "common/cvar.h"
#include "console/console.h"
#include "console/protector.h"

NextClientVersion g_NextClientVersion;

nitroapi::NitroApiInterface* g_NitroApi;
std::shared_ptr<nitro_utils::ConfigProviderInterface> g_SettingGuard;
IFileSystem* g_pFileSystem;
IFileSystemNext* g_pFileSystemNext;
AnalyticsInterface* g_Analytics;
IGameUI* g_pGameUi;
IGameConsole* g_GameConsole;
IGameConsoleNext* g_GameConsoleNext;
vgui2::ILocalize* g_pLocalize;

cl_enginefunc_t gEngfuncs;
enginefuncs_t g_engfuncs;
modfuncs_t g_modfuncs;
sizebuf_t* net_message;
netadr_t* net_from;
int* pMsg_readcount;
double* realtime;
double* host_frametime;
qboolean* gfExtendedError;
char* gszDisconnectReason;
char* gDownloadFile;
client_state_t* cl;
client_stateex_t client_stateex;
cmd_source_t* cmd_source;
cldll_func_t cl_funcs;
client_static_t* cls;
server_static_t* g_psvs;
server_t* g_psv;
qboolean* g_bMajorMapChange;
CareerStateType* g_careerState;
model_t** loadmodel;
int* cl_playerindex;
cl_entity_t** p_cl_entities;
bool* p_noclip_anglehack;
cache_user_t** p_loadcache;
unsigned char** p_loadbuf;
int* p_loadsize;
qboolean* p_host_initialized;
cl_enginefunc_dst_t* p_g_engdstAddrs;
nitroapi::viddef_t* p_vid;
int* p_gl_mtexable;
int* p_currenttexture;
cl_entity_t** p_currententity;
GLfloat* p_r_blend;
vec3_t* p_r_entorigin;
qboolean* p_g_bUserFogOn;
int* p_numTransObjs;
nitroapi::transObjRef** p_transObjects;
qboolean* p_mtexenabled;
cl_entity_t* p_r_worldentity;
qboolean* p_mirror;
int* p_c_brush_polys;
int* p_c_alias_polys;
int* p_cl_numvisedicts;
cl_entity_t** p_cl_visedicts;
qboolean* p_isFogEnabled;
refdef_t* r_refdef;
float* p_scr_fov_value;
float* p_g_SND_VoiceOverdrive;
int* p_cszrawsentences;
char *(*p_rgpszrawsentence)[CVOXFILESENTENCEMAX];
float* p_scr_con_current;
keydest_t* p_key_dest;

cvar_t* fs_startup_timings;
cvar_t* fs_lazy_precache;
cvar_t* fs_precache_timings;
cvar_t* cl_download_ingame;
cvar_t* cl_shownet;
cvar_t* cl_timeout;
cvar_t* scr_downloading;
cvar_t* host_framerate;
cvar_t* sys_ticrate;
cvar_t* gl_vsync;
cvar_t* fps_override;
cvar_t* fps_max;
cvar_t* sys_timescale;
cvar_t* developer;
cvar_t* sv_cheats;
cvar_t* gl_spriteblend;
cvar_t* r_drawentities;
cvar_t* r_norefresh;
cvar_t* r_speeds;
cvar_t* viewmodel_fov;

bool g_bIsDedicatedServer;
r_studio_interface_t* pStudioAPI;

static std::vector<std::shared_ptr<nitroapi::Unsubscriber>> g_Unsubs;

nitroapi::NitroApiInterface* napi() { return g_NitroApi; }
nitroapi::EngineData* eng() { return g_NitroApi->GetEngineData(); }
nitroapi::ClientData* client() { return g_NitroApi->GetClientData(); }
nitroapi::SDL2Data* sdl2() { return g_NitroApi->GetSDL2Data(); }

static void UninitializeInternal()
{
    TaskRun::UnInitialize();

    CL_DeleteHttpDownloadManager();
    UnInstallBrowserExtensions();
    AUDIO_Shutdown();
    CL_CvarsSandboxShutdown();
    PROTECTOR_Shutdown();

    KV_UninitializeKeyValuesSystem();

    for (auto &unsubscriber : g_Unsubs)
        unsubscriber->Unsubscribe();
    g_Unsubs.clear();

    g_NitroApi = nullptr;
    g_SettingGuard = nullptr;
    g_pFileSystem = nullptr;
    g_pFileSystemNext = nullptr;
    g_pGameUi = nullptr;
    g_GameConsole = nullptr;
    g_GameConsoleNext = nullptr;
    g_pLocalize = nullptr;

    Q_memset(&gEngfuncs, 0, sizeof(gEngfuncs));
    Q_memset(&g_engfuncs, 0, sizeof(g_engfuncs));
    net_message = nullptr;
    net_from = nullptr;
    pMsg_readcount = nullptr;
    realtime = nullptr;
    host_frametime = nullptr;
    gfExtendedError = nullptr;
    gszDisconnectReason = nullptr;
    gDownloadFile = nullptr;
    cl = nullptr;
    cmd_source = nullptr;
    Q_memset(&cl_funcs, 0, sizeof(cl_funcs));
    cls = nullptr;
    g_psvs = nullptr;
    g_psv = nullptr;
    g_bMajorMapChange = nullptr;
    g_careerState = nullptr;
    loadmodel = nullptr;
    cl_playerindex = nullptr;
    p_loadcache = nullptr;
    p_loadbuf = nullptr;
    p_loadsize = nullptr;
    p_host_initialized = nullptr;
    p_g_engdstAddrs = nullptr;
    p_vid = nullptr;
    p_gl_mtexable = nullptr;
    p_currenttexture = nullptr;
    p_currententity = nullptr;
    p_r_blend = nullptr;
    p_r_entorigin = nullptr;
    p_g_bUserFogOn = nullptr;
    p_numTransObjs = nullptr;
    p_transObjects = nullptr;
    p_mtexenabled = nullptr;
    p_r_worldentity = nullptr;
    p_mirror = nullptr;
    p_c_brush_polys = nullptr;
    p_c_alias_polys = nullptr;
    p_cl_numvisedicts = nullptr;
    p_cl_visedicts = nullptr;
    p_isFogEnabled = nullptr;
    r_refdef = nullptr;
    p_scr_fov_value = nullptr;
    p_g_SND_VoiceOverdrive = nullptr;
    p_cszrawsentences = nullptr;
    p_rgpszrawsentence = nullptr;
    p_scr_con_current = nullptr;
    p_key_dest = nullptr;

    fs_startup_timings = nullptr;
    fs_lazy_precache = nullptr;
    fs_precache_timings = nullptr;
    cl_download_ingame = nullptr;
    cl_shownet = nullptr;
    cl_timeout = nullptr;
    scr_downloading = nullptr;
    host_framerate = nullptr;
    sys_ticrate = nullptr;
    gl_vsync = nullptr;
    fps_override = nullptr;
    fps_max = nullptr;
    sys_timescale = nullptr;
    developer = nullptr;
    sv_cheats = nullptr;
    gl_spriteblend = nullptr;
    r_drawentities = nullptr;
    r_norefresh = nullptr;
    r_speeds = nullptr;

    p_cl_entities = nullptr;
    p_noclip_anglehack = nullptr;

    p_cmd_argc = nullptr;
    p_cmd_argv = nullptr;
    p_cvar_vars = nullptr;
}

static void InitInternal(AnalyticsInterface* analytics, nitroapi::NitroApiInterface* nitro_api, void* mainwindow, HDC* pmaindc, HGLRC* pbaseRC, const char* pszDriver, const char* pszCmdLine, bool result)
{
    if (analytics)
        analytics->AddBreadcrumb("info", BREADCRUMBS_TAG " InitInternal");

    g_Analytics = analytics;

    nitro_utils::PtrValidator v;

    v.Assign(g_NitroApi, GET_VARIABLE_NAME(g_NitroApi), nitro_api);
    v.Assign(p_gl_mtexable, GET_VARIABLE_NAME(p_gl_mtexable), eng()->gl_mtexable);
    v.Assign(p_currenttexture, GET_VARIABLE_NAME(p_currenttexture), eng()->currenttexture);
    v.Assign(cl, GET_VARIABLE_NAME(cl), eng()->client_state);
    v.Assign(cls, GET_VARIABLE_NAME(cls), eng()->client_static);

    CreateInterfaceFn gameui_factory = Sys_GetFactory("gameui.dll");
    v.Assign(g_pGameUi, GET_VARIABLE_NAME(g_pGameUi), (IGameUI*)InitializeInterface(GAMEUI_INTERFACE_VERSION_GS, &gameui_factory, 1));
    v.Assign(g_GameConsoleNext, GET_VARIABLE_NAME(g_GameConsoleNext), (IGameConsoleNext*)InitializeInterface(GAMECONSOLE_NEXT_INTERFACE_VERSION, &gameui_factory, 1));
    v.Assign(g_GameConsole, GET_VARIABLE_NAME(g_GameConsole), (IGameConsole*)InitializeInterface(GAMECONSOLE_INTERFACE_VERSION_GS, &gameui_factory, 1));

    CreateInterfaceFn filesystem_factory = Sys_GetFactory("FileSystem_Proxy.dll");
    v.Assign(g_pFileSystem, GET_VARIABLE_NAME(g_pFileSystem), (IFileSystem*)filesystem_factory(FILESYSTEM_INTERFACE_VERSION, NULL));
    v.Assign(g_pFileSystemNext, GET_VARIABLE_NAME(g_pFileSystemNext), (IFileSystemNext*)filesystem_factory(FILESYSTEM_NEXT_INTERFACE_VERSION, NULL));

    g_pFileSystem->AddSearchPathNoWrite("\\cstrike_downloads_private", "GAME");

    if (v.Validate(eng()->cl_enginefunc, GET_VARIABLE_NAME(gEngfuncs)))
        std::memcpy(&gEngfuncs, eng()->cl_enginefunc, sizeof(gEngfuncs));

    if (v.Validate(eng()->enginefuncs, GET_VARIABLE_NAME(g_engfuncs)))
        std::memcpy(&g_engfuncs, eng()->enginefuncs, sizeof(g_engfuncs));

    if (v.Validate(eng()->modfuncs, GET_VARIABLE_NAME(g_modfuncs)))
        std::memcpy(&g_modfuncs, eng()->modfuncs, sizeof(g_modfuncs));

    if (v.Validate(eng()->cldll_func, GET_VARIABLE_NAME(cl_funcs)))
        std::memcpy(&cl_funcs, eng()->cldll_func, sizeof(cl_funcs));

    v.Assign(g_SettingGuard, GET_VARIABLE_NAME(g_Config), std::make_shared<nitro_utils::FileConfigProvider>("setting_guard.ini"));
    v.Assign(net_message, GET_VARIABLE_NAME(net_message), eng()->net_message);
    v.Assign(net_from, GET_VARIABLE_NAME(net_from), eng()->net_from);
    v.Assign(pMsg_readcount, GET_VARIABLE_NAME(pMsg_readcount), eng()->msg_readcount);
    v.Assign(realtime, GET_VARIABLE_NAME(realtime), eng()->realtime);
    v.Assign(host_frametime, GET_VARIABLE_NAME(host_frametime), eng()->host_frametime);
    v.Assign(gfExtendedError, GET_VARIABLE_NAME(gfExtendedError), eng()->gfExtendedError);
    v.Assign(gszDisconnectReason, GET_VARIABLE_NAME(gszDisconnectReason), eng()->gszDisconnectReason);
    v.Assign(gDownloadFile, GET_VARIABLE_NAME(gDownloadFile), eng()->gDownloadFile);
    v.Assign(cmd_source, GET_VARIABLE_NAME(cmd_source), eng()->cmd_source);
    v.Assign(g_psvs, GET_VARIABLE_NAME(g_psvs), eng()->server_static);
    v.Assign(g_psv, GET_VARIABLE_NAME(g_psvs), eng()->server);
    v.Assign(g_bMajorMapChange, GET_VARIABLE_NAME(g_bMajorMapChange), eng()->bMajorMapChange);
    v.Assign(g_careerState, GET_VARIABLE_NAME(g_careerState), eng()->careerState);
    v.Assign(loadmodel, GET_VARIABLE_NAME(loadmodel), eng()->loadmodel);
    v.Assign(cl_playerindex, GET_VARIABLE_NAME(cl_playerindex), eng()->cl_playerindex);
    v.Assign(p_loadcache, GET_VARIABLE_NAME(pLoadcache), eng()->loadcache);
    v.Assign(p_loadbuf, GET_VARIABLE_NAME(pLoadbuf), eng()->loadbuf);
    v.Assign(p_loadsize, GET_VARIABLE_NAME(pLoadsize), eng()->loadsize);
    v.Assign(p_host_initialized, GET_VARIABLE_NAME(p_host_initialized), eng()->host_initialized);
    v.Assign(p_g_engdstAddrs, GET_VARIABLE_NAME(p_g_engdstAddrs), eng()->g_engdstAddrs);
    v.Assign(p_vid, GET_VARIABLE_NAME(p_vid), eng()->vid);
    v.Assign(p_currententity, GET_VARIABLE_NAME(p_currententity), eng()->currententity);
    v.Assign(p_r_blend, GET_VARIABLE_NAME(p_r_blend), eng()->r_blend);
    v.Assign(p_r_entorigin, GET_VARIABLE_NAME(p_r_entorigin), eng()->r_entorigin);
    v.Assign(p_g_bUserFogOn, GET_VARIABLE_NAME(p_g_bUserFogOn), eng()->g_bUserFogOn);
    v.Assign(p_numTransObjs, GET_VARIABLE_NAME(p_numTransObjs), eng()->numTransObjs);
    v.Assign(p_transObjects, GET_VARIABLE_NAME(p_transObjects), eng()->transObjects);
    v.Assign(p_mtexenabled, GET_VARIABLE_NAME(p_mtexenabled), eng()->mtexenabled);
    v.Assign(p_r_worldentity, GET_VARIABLE_NAME(p_r_worldentity), eng()->r_worldentity);
    v.Assign(p_mirror, GET_VARIABLE_NAME(p_mirror), eng()->mirror);
    v.Assign(p_c_brush_polys, GET_VARIABLE_NAME(p_c_brush_polys), eng()->c_brush_polys);
    v.Assign(p_c_alias_polys, GET_VARIABLE_NAME(p_c_alias_polys), eng()->c_alias_polys);
    v.Assign(p_cl_numvisedicts, GET_VARIABLE_NAME(p_cl_numvisedicts), eng()->cl_numvisedicts);
    v.Assign(p_cl_visedicts, GET_VARIABLE_NAME(p_cl_visedicts), eng()->cl_visedicts);
    v.Assign(p_isFogEnabled, GET_VARIABLE_NAME(p_isFogEnabled), eng()->isFogEnabled);
    v.Assign(r_refdef, GET_VARIABLE_NAME(p_r_refdef), eng()->r_refdef);
    v.Assign(p_scr_fov_value, GET_VARIABLE_NAME(p_scr_fov_value), eng()->scr_fov_value);
    v.Assign(p_g_SND_VoiceOverdrive, GET_VARIABLE_NAME(p_g_SND_VoiceOverdrive), eng()->g_SND_VoiceOverdrive);
    v.Assign(p_cszrawsentences, GET_VARIABLE_NAME(p_cszrawsentences), eng()->cszrawsentences);
    v.Assign(p_rgpszrawsentence, GET_VARIABLE_NAME(p_rgpszrawsentence), eng()->rgpszrawsentence);
    v.Assign(p_scr_con_current, GET_VARIABLE_NAME(p_scr_con_current), eng()->scr_con_current);
    v.Assign(scr_downloading, GET_VARIABLE_NAME(scr_downloading), eng()->scr_downloading);
    v.Assign(p_cl_entities, GET_VARIABLE_NAME(p_cl_entities), eng()->cl_entities);
    v.Assign(p_noclip_anglehack, GET_VARIABLE_NAME(p_noclip_anglehack), eng()->noclip_anglehack);
    v.Assign(developer, GET_VARIABLE_NAME(developer), eng()->developer);
    v.Assign(p_cmd_argc, GET_VARIABLE_NAME(p_cmd_argc), eng()->cmd_argc);
    v.Assign(p_cmd_argv, GET_VARIABLE_NAME(p_cmd_argv), eng()->cmd_argv);
    v.Assign(p_cvar_vars, GET_VARIABLE_NAME(p_cvar_vars), eng()->cvar_vars);
    v.Assign(p_key_dest, GET_VARIABLE_NAME(p_key_dest), eng()->key_dest);

    if (v.HasNullPtr())
    {
        UninitializeInternal();

        std::string error = std::format(BREADCRUMBS_TAG " InitInternal: PtrValidator failed: {}", nitro_utils::join(v.GetNullPtrNames().cbegin(), v.GetNullPtrNames().cend(), ", "));
        g_NitroApi->GetEngineData()->Sys_Error.InvokeChained(error.c_str());
        return;
    }

    Con_Init();

    //
    // Hooks that completely replace engine functions
    //
    g_Unsubs.emplace_back(eng()->Netchan_CopyFileFragments   |= [](netchan_t *chan, const auto& next)                                  { return Netchan_CopyFileFragments(chan); } );
    g_Unsubs.emplace_back(eng()->SVC_TimeScale               |= [](const auto& next)                                                   { CL_Parse_Timescale(); });
    g_Unsubs.emplace_back(eng()->SVC_SendCvarValue           |= [](const auto& next)                                                   { CL_Send_CvarValue(); });
    g_Unsubs.emplace_back(eng()->CL_HTTPUpdate               |= [](const auto& next)                                                   { CL_HTTPUpdate(); } );
    g_Unsubs.emplace_back(eng()->CL_HTTPStop_f               |= [](const auto& next)                                                   { CL_HTTPStop_f(); } );
    g_Unsubs.emplace_back(eng()->CL_StartResourceDownloading |= [](const char* msg, int custom, const auto& next)                      { CL_StartResourceDownloading(msg, custom); });
    g_Unsubs.emplace_back(eng()->CL_ReadPackets              |= [](const auto& next)                                                   { CL_ReadPackets(); });
    g_Unsubs.emplace_back(eng()->CL_RequestMissingResources  |= [](const auto& next)                                                   { return CL_RequestMissingResources(); });
    g_Unsubs.emplace_back(eng()->Host_Map_f                  |= [](const auto& next)                                                   { Host_Map_f(); });
    g_Unsubs.emplace_back(eng()->Host_FilterTime             |= [](float delay, const auto& next)                                      { return Host_FilterTime(delay); });
    g_Unsubs.emplace_back(eng()->Mod_ClearAll                |= [](const auto& next)                                                   { Mod_ClearAll(); });
    g_Unsubs.emplace_back(eng()->Mod_FindName                |= [](qboolean trackCRC, const char* name, const auto& next)              { return Mod_FindName(trackCRC, name); });
    g_Unsubs.emplace_back(eng()->Mod_Print                   |= [](const auto& next)                                                   { Mod_Print(); });
    g_Unsubs.emplace_back(eng()->Mod_ValidateCRC             |= [](const char* name, CRC32_t crc, const auto& next)                    { return Mod_ValidateCRC(name, crc); });
    g_Unsubs.emplace_back(eng()->Mod_NeedCRC                 |= [](const char* name, qboolean needCRC, const auto& next)               { Mod_NeedCRC(name, needCRC); });
    g_Unsubs.emplace_back(eng()->Mod_LoadModel               |= [](model_t* mod, qboolean crash, qboolean trackCRC, const auto& next)  { return Mod_LoadModel(mod, crash, trackCRC); });
    g_Unsubs.emplace_back(eng()->SPR_Init                    |= [](const auto& next)                                                   { return SPR_Init(); });
    g_Unsubs.emplace_back(eng()->SPR_Shutdown                |= [](const auto& next)                                                   { return SPR_Shutdown(); });
    g_Unsubs.emplace_back(eng()->SPR_Shutdown_NoModelFree    |= [](const auto& next)                                                   { return SPR_Shutdown_NoModelFree(); });
    g_Unsubs.emplace_back(eng()->SPR_Load                    |= [](const char* str, const auto& next)                                  { return SPR_Load(str); });
    g_Unsubs.emplace_back(eng()->SPR_Draw                    |= [](int frame, int x, int y, const wrect_t* prc, const auto& next)      { SPR_Draw(frame, x, y, prc); });
    g_Unsubs.emplace_back(eng()->SPR_DrawAdditive            |= [](int frame, int x, int y, const wrect_t* prc, const auto& next)      { SPR_DrawAdditive(frame, x, y, prc); });
    g_Unsubs.emplace_back(eng()->SPR_DrawHoles               |= [](int frame, int x, int y, const wrect_t* prc, const auto& next)      { SPR_DrawHoles(frame, x, y, prc); });
    g_Unsubs.emplace_back(eng()->SPR_DrawGeneric             |= [](int frame, int x, int y, const wrect_t* prc, int src, int dest, int width, int height, const auto& next) { SPR_DrawGeneric(frame, x, y, prc, src, dest, width, height); });
    g_Unsubs.emplace_back(eng()->SPR_EnableScissor           |= [](int x, int y, int width, int height, const auto& next)              { SPR_EnableScissor(x, y, width, height); });
    g_Unsubs.emplace_back(eng()->SPR_DisableScissor          |= [](const auto& next)                                                   { SPR_DisableScissor(); });
    g_Unsubs.emplace_back(eng()->SPR_Set                     |= [](HSPRITE_t hsprite, int r, int g, int b, const auto& next)           { SPR_Set(hsprite, r, g, b); });
    g_Unsubs.emplace_back(eng()->SPR_Width                   |= [](HSPRITE_t hsprite, int frame, const auto& next)                     { return SPR_Width(hsprite, frame); });
    g_Unsubs.emplace_back(eng()->SPR_Height                  |= [](HSPRITE_t hsprite, int frame, const auto& next)                     { return SPR_Height(hsprite, frame); });
    g_Unsubs.emplace_back(eng()->SPR_Frames                  |= [](HSPRITE_t hsprite, const auto& next)                                { return SPR_Frames(hsprite); });
    g_Unsubs.emplace_back(eng()->SPR_GetModelPointer         |= [](HSPRITE_t hsprite, const auto& next)                                { return SPR_GetModelPointer(hsprite); });
    g_Unsubs.emplace_back(eng()->R_StudioReloadSkin          |= [](model_t* pModel, int index, skin_t* pskin, const auto& next)        { return R_StudioReloadSkin(pModel, index, pskin); });
    g_Unsubs.emplace_back(eng()->SetCrosshair                |= [](HSPRITE_t hspr, wrect_t rc, int r, int g, int b, const auto& next)  { SetCrosshair(hspr, rc, r, g, b); });
    g_Unsubs.emplace_back(eng()->DrawCrosshair               |= [](int x, int y, const auto& next)                                     { DrawCrosshair(x, y); });
    g_Unsubs.emplace_back(eng()->R_RenderView                |= [](const auto& next)                                                   { R_RenderView(); });
    g_Unsubs.emplace_back(eng()->GL_SelectTexture            |= [](GLenum target, const auto& next)                                    { GL_SelectTexture(target); });
    g_Unsubs.emplace_back(eng()->CL_ConnectClient            |= [](const auto& next)                                                   { CL_ConnectClient(); });
    g_Unsubs.emplace_back(eng()->CL_ClearClientState         |= [](const auto& next)                                                   { CL_ClearClientState(); });

    //
    // The rest of the hooks and subscribers
    //
    g_Unsubs.emplace_back(eng()->SVC_StuffText |= [](const auto& next) {
        int read_count = *pMsg_readcount;
        std::string cmd = MSG_ReadString();
        *pMsg_readcount = read_count;

        PrivateRes_ParseDownloadPath(cmd);
        next->Invoke();
    });

    g_Unsubs.emplace_back(eng()->SVC_ResourceLocation |= [](const auto &next) {
        int read_count = *pMsg_readcount;
        std::string url = MSG_ReadString();
        *pMsg_readcount = read_count;

        CL_HTTPSetDownloadUrl(url);
        next->Invoke();
    });

    g_Unsubs.emplace_back(eng()->CL_HTTPCancel_f += []() {
        CL_HTTPCancel_f();
    });

    g_Unsubs.emplace_back(eng()->CL_Disconnect |= [](const auto& next) {
        // We must clear the list of private resources before SPR_Shutdown_NoModelFree is called (which is called from CL_Disconnect)
        PrivateRes_Clear();
        client_stateex.resourcesNeeded.clear();
        next->Invoke();
    });

    g_Unsubs.emplace_back(eng()->NET_SendPacket += [](netsrc_t sock, int length, void *data, netadr_t to, int result) {
        NET_SendPacketPost(sock, length, data, to, result);
    });

    g_Unsubs.emplace_back(eng()->Cbuf_AddText += [](const char *text, sizebuf_t *buf, char* result) {
        // for cases when client downloading files through dlfile
        // give him a chance to use http download again
        if (std::strncmp(text, "disconnect", 10) == 0)
            gEngfuncs.pfnClientCmd("httpstop");
    });

    g_Unsubs.emplace_back(eng()->Sys_InitGame += [](char *pOrgCmdLine, char *pBaseDir, void *pwnd, int bIsDedicated, bool ret) {
        g_bIsDedicatedServer = bIsDedicated;
    });

    g_Unsubs.emplace_back(eng()->Cvar_Command |= [](const auto& next) -> qboolean {
        const auto cvar_name = g_engfuncs.pfnCmd_Argv(0);
        const auto cvar = g_engfuncs.pfnCVarGetPointer(cvar_name);

        if (cvar != nullptr && cvar->flags & FCVAR_CHEAT)
        {
            if (!Host_IsSinglePlayerGame() && sv_cheats->value == 0 && !cls->spectator && !cls->demoplayback)
            {
                Con_Printf("Can't use cheat cvar %s with disabled cheats\n", cvar_name);
                return TRUE;
            }
        }

        return next->Invoke();
    });

    g_Unsubs.emplace_back(eng()->AppendTEntity += [](cl_entity_t* ent) {
        AppendTEntity_Subscriber(ent);
    });

    g_Unsubs.emplace_back(eng()->GL_Init += GL_Init_Subscriber);

    g_Unsubs.emplace_back(eng()->Con_MessageMode_f += []() {
        if (*p_key_dest == key_message)
        {
            gEngfuncs.pfnServerCmd("chat_open");
        }
    });

    g_Unsubs.emplace_back(eng()->Con_MessageMode2_f += []() {
        if (*p_key_dest == key_message)
        {
            gEngfuncs.pfnServerCmd("chat_team_open");
        }
    });

    g_Unsubs.emplace_back(eng()->Key_Message += [](int key) {
        if (*p_key_dest != key_message)
        {
            gEngfuncs.pfnServerCmd("chat_close");
        }
    });

    g_Unsubs.emplace_back(client()->HUD_GetStudioModelInterface += [](int version, struct r_studio_interface_s **ppinterface, struct engine_studio_api_s *pstudio, int result) {
        pStudioAPI = *ppinterface;
    });

    TaskRun::Initialize(std::make_shared<TaskRunImpl>(napi()));

    GL_SetMode_Subscriber(mainwindow, pmaindc, pbaseRC, pszDriver, pszCmdLine, result);

    AUDIO_Init();
}

static void InitInternalPost()
{
    nitro_utils::PtrValidator v;

    CreateInterfaceFn vgui2_factory = Sys_GetFactory("vgui2.dll");
    g_pLocalize = v.Validate((vgui2::ILocalize*)InitializeInterface(VGUI_LOCALIZE_INTERFACE_VERSION, &vgui2_factory, 1), GET_VARIABLE_NAME(localize));
    g_pLocalize->AddFile(g_pFileSystem, "resource/nextclient_%language%.txt");

    KV_InitKeyValuesSystem2(vgui2_factory);

    v.Assign(fs_startup_timings, GET_VARIABLE_NAME(fs_startup_timings), g_engfuncs.pfnCVarGetPointer("fs_startup_timings"));
    v.Assign(fs_lazy_precache, GET_VARIABLE_NAME(fs_lazy_precache), g_engfuncs.pfnCVarGetPointer("fs_lazy_precache"));
    v.Assign(fs_precache_timings, GET_VARIABLE_NAME(fs_precache_timings), g_engfuncs.pfnCVarGetPointer("fs_precache_timings"));
    v.Assign(cl_download_ingame, GET_VARIABLE_NAME(cl_download_ingame), g_engfuncs.pfnCVarGetPointer("cl_download_ingame"));
    v.Assign(cl_shownet, GET_VARIABLE_NAME(cl_shownet), g_engfuncs.pfnCVarGetPointer("cl_shownet"));
    v.Assign(cl_timeout, GET_VARIABLE_NAME(cl_timeout), g_engfuncs.pfnCVarGetPointer("cl_timeout"));
    v.Assign(host_framerate, GET_VARIABLE_NAME(host_framerate), g_engfuncs.pfnCVarGetPointer("host_framerate"));
    v.Assign(sys_ticrate, GET_VARIABLE_NAME(sys_ticrate), g_engfuncs.pfnCVarGetPointer("sys_ticrate"));
    v.Assign(gl_vsync, GET_VARIABLE_NAME(gl_vsync), g_engfuncs.pfnCVarGetPointer("gl_vsync"));
    v.Assign(fps_override, GET_VARIABLE_NAME(fps_override), g_engfuncs.pfnCVarGetPointer("fps_override"));
    v.Assign(fps_max, GET_VARIABLE_NAME(fps_max), g_engfuncs.pfnCVarGetPointer("fps_max"));
    v.Assign(sv_cheats, GET_VARIABLE_NAME(sv_cheats), g_engfuncs.pfnCVarGetPointer("sv_cheats"));
    v.Assign(gl_spriteblend, GET_VARIABLE_NAME(gl_spriteblend), g_engfuncs.pfnCVarGetPointer("gl_spriteblend"));
    v.Assign(r_drawentities, GET_VARIABLE_NAME(r_drawentities), g_engfuncs.pfnCVarGetPointer("r_drawentities"));
    v.Assign(r_norefresh, GET_VARIABLE_NAME(r_norefresh), g_engfuncs.pfnCVarGetPointer("r_norefresh"));
    v.Assign(r_speeds, GET_VARIABLE_NAME(r_speeds), g_engfuncs.pfnCVarGetPointer("r_speeds"));

    v.Assign(sys_timescale, GET_VARIABLE_NAME(sys_timescale), eng()->sys_timescale);
    sys_timescale->flags |= FCVAR_CHEAT;
    g_engfuncs.pfnCvar_RegisterVariable(sys_timescale);

    if (v.HasNullPtr())
    {
        UninitializeInternal();

        std::string error = std::format(BREADCRUMBS_TAG " InitInternalPost: PtrValidator failed: {}", nitro_utils::join(v.GetNullPtrNames().cbegin(), v.GetNullPtrNames().cend(), ", "));
        g_NitroApi->GetEngineData()->Sys_Error.InvokeChained(error.c_str());
        return;
    }

    viewmodel_fov = gEngfuncs.pfnRegisterVariable("viewmodel_fov", std::to_string(90.f).c_str(), FCVAR_ARCHIVE);

    CL_CreateHttpDownloadManager(g_pGameUi, g_pLocalize, g_SettingGuard);
    InstallBrowserExtensions();
    AUDIO_RegisterCommands();
    CL_CvarsSandboxInit();
    PROTECTOR_Init(g_SettingGuard);
}

class EngineMini : public EngineMiniInterface
{
    std::vector<std::shared_ptr<nitroapi::Unsubscriber>> unsubs_;

public:
    void Init(nitroapi::NitroApiInterface* nitro_api, NextClientVersion client_version, AnalyticsInterface* analytics) override
    {
        g_NextClientVersion = client_version;

        if (analytics)
            analytics->AddBreadcrumb("info", BREADCRUMBS_TAG " EngineMini::Init");

        unsubs_.emplace_back(nitro_api->GetEngineData()->COM_InitArgv |= [](int argc, char** argv, const auto& next) {
            COM_InitArgv(argc, argv);
            next->Invoke(argc, argv);
        });

        unsubs_.emplace_back(nitro_api->GetEngineData()->GL_SetMode +=[nitro_api, analytics](void* mainwindow, HDC* pmaindc, HGLRC* pbaseRC, const char* pszDriver, const char* pszCmdLine, bool result) {
            InitInternal(analytics, nitro_api, mainwindow, pmaindc, pbaseRC, pszDriver, pszCmdLine, result);
        });

        unsubs_.emplace_back(nitro_api->GetEngineData()->Sys_InitGame += [](char* lpOrgCmdLine, char* pBaseDir, void* pwnd, int bIsDedicated, bool ret) {
            InitInternalPost();
        });
    }

    void Uninitialize() override
    {
        if (g_Analytics)
            g_Analytics->AddBreadcrumb("info", BREADCRUMBS_TAG " EngineMini::Uninitialize");

        for (auto &unsubscriber : unsubs_)
            unsubscriber->Unsubscribe();
        unsubs_.clear();

        UninitializeInternal();
    }

    void GetVersion(char* buffer, int size) override
    {
        if (buffer != nullptr)
            strcpy_s(buffer, size,  ENGINE_MINI_INTERFACE_VERSION ", " __DATE__ " " __TIME__);
    }

    HttpDownloadManagerInterface* GetHttpDownloadManager() override
    {
        return CL_GetHttpDownloadManager();
    }

    bool AddDownloadLogger(DownloadFileLoggerInterface* logger) override
    {
        return CL_AddDownloadFileLogger(logger);
    }

    bool RemoveDownloadLogger(DownloadFileLoggerInterface* logger) override
    {
        return CL_RemoveDonwloadFileLogger(logger);
    }

    bool AddCmdLogger(CommandLoggerInterface* logger) override
    {
        return PROTECTOR_AddCmdLogger(logger);
    }

    bool RemoveCmdLogger(CommandLoggerInterface* logger) override
    {
        return PROTECTOR_RemoveCmdLogger(logger);
    }
};

EXPOSE_SINGLE_INTERFACE(EngineMini, EngineMiniInterface, ENGINE_MINI_INTERFACE_VERSION);
