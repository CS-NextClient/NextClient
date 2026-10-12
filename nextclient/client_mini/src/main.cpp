#include "main.h"
#include <cstring>
#include <ranges>
#include <thread>
#include <next_client_mini/client_mini.h>
#include <next_engine_mini/engine_mini.h>
#include <data_types/MessageReader.h>
#include <goldsrc/client_api.h>
#include <goldsrc/cvar.h>
#include <goldsrc/engine/server_state.h>
#include <goldsrc/entity.h>
#include <goldsrc/pmove.h>
#include <goldsrc/server_api.h>
#include <goldsrc/studio_api.h>
#include <goldsrc/usercmd.h>
#include <goldsrc/view.h>
#include <mathlib/mathlib.h>
#include <strtools.h>
#include <taskcoro/impl/TaskCoroImpl.h>
#include <taskcoro/TaskCoro.h>

#include "material_preview.h"
#include "camera.h"
#include "studiorenderer.h"
#include "view.h"
#include "fov.h"
#include "color_chat_in_console.h"
#include "inspect.h"
#include "commands.h"
#include "invert_mouse.h"
#include "hud/radar/integration/RadarHooks.h"
#include "hud/radar/render/text/HudTextZone.h"

nitroapi::NitroApiInterface* g_NitroApi;

IGameConsole* g_GameConsole;
IGameConsoleNext* g_GameConsoleNext;

goldsrc::cldll_func_t cl_funcs;
goldsrc::cl_enginefunc_t gEngfuncs;
goldsrc::enginefuncs_t g_engfuncs;
goldsrc::local_state_t g_LastPlayerState;
goldsrc::client_data_t g_LastClientData;
goldsrc::server_t* sv;
goldsrc::server_static_t* sv_static;

nitroapi::gamehud_t* gHUD;
int g_iUser1;
int g_iUser2;
int g_CurrentWeaponId;

goldsrc::engine_studio_api_t IEngineStudio;
goldsrc::r_studio_interface_t g_OriginalStudio;
goldsrc::playermove_t* pmove;

std::unique_ptr<GameHud> g_GameHud;
static std::vector<std::shared_ptr<nitroapi::Unsubscriber>> g_Unsub;

goldsrc::cvar_t* hud_draw;

nitroapi::EngineData* eng()
{
    return g_NitroApi->GetEngineData();
}

nitroapi::ClientData* client()
{
    return g_NitroApi->GetClientData();
}

static void ClearHudTxt()
{
    if (!gHUD)
        return;

    if (*gHUD->m_pSpriteList)
        g_NitroApi->GetEngineData()->Mem_Free(*gHUD->m_pSpriteList);

    *gHUD->m_pSpriteList = nullptr;
    *gHUD->m_iSpriteCountAllRes = 0;
}

static void HUD_InitPost()
{
    CreateInterfaceFn gameui_factory = Sys_GetFactory(
#ifdef _WIN32
        "cstrike\\cl_dlls\\gameui.dll"
#else
        "gameui.so"
#endif
        );

    g_GameConsole = (IGameConsole*)(InitializeInterface(GAMECONSOLE_INTERFACE_VERSION_GS, &gameui_factory, 1));
    g_GameConsoleNext = (IGameConsoleNext*)(InitializeInterface(GAMECONSOLE_NEXT_INTERFACE_VERSION, &gameui_factory, 1));

    std::memcpy(&cl_funcs, g_NitroApi->GetEngineData()->cldll_func, sizeof(cl_funcs));
    std::memcpy(&gEngfuncs, g_NitroApi->GetEngineData()->cl_enginefunc, sizeof(gEngfuncs));
    std::memcpy(&g_engfuncs, g_NitroApi->GetEngineData()->enginefuncs, sizeof(g_engfuncs));
    gHUD = g_NitroApi->GetClientData()->gHUD;
    sv = g_NitroApi->GetEngineData()->server;
    sv_static = g_NitroApi->GetEngineData()->server_static;

    ViewInit();
    FovInit();
    InspectInit();
    CommandsInit();
    CameraInit();
    g_GameHud->Init();

    hud_draw = g_engfuncs.pfnCVarGetPointer("hud_draw");

    InvertMouseInit();

    ColorChatInConsolePatch();

    RadarHooks_PatchHudText();

    CreateInterfaceFn engine_factory = Sys_GetFactory("next_engine_mini.dll");

    if (engine_factory != nullptr)
    {
        EngineMiniInterface* engine_mini =
            static_cast<EngineMiniInterface*>(engine_factory(ENGINE_MINI_INTERFACE_VERSION, nullptr));

        if (engine_mini != nullptr)
        {
            MaterialPreview_Init(engine_mini->GetMaterialEdit());
        }
    }
}

static int HUD_RedrawHandler(float flTime, int iIntermission, HUD_RedrawNext next)
{
    // The scope mask covers the radar corner, so zoomed frames draw the radar after the stock HUD.
    goldsrc::SCREENINFO screen;
    screen.iSize = sizeof(screen);
    gEngfuncs.pfnGetScreenInfo(&screen);
    HudTextZone_BeginFrame(screen.iWidth, screen.iHeight);

    HudTextZone_ClearZone();
    bool scoped = gHUD->m_iFOV && *gHUD->m_iFOV > 0 && *gHUD->m_iFOV < 90;

    if (hud_draw->value != 0.0)
    {
        HudTextZone_SetPassthrough(true);
        g_GameHud->PrepareRadar();

        if (!scoped)
        {
            g_GameHud->DrawRadar(flTime);
        }

        HudTextZone_SetPassthrough(false);
    }

    int result = next->Invoke(flTime, iIntermission);

    // Resolve shared text anchors once the stock HUD has supplied every row.
    RadarHooks_FlushText();

    HudTextZone_SetPassthrough(true);
    if (hud_draw->value != 0.0)
    {
        if (scoped)
        {
            g_GameHud->DrawRadar(flTime);
        }

        g_GameHud->Draw(flTime);
    }

    // Stock HUD updates may recenter the scoreboard, so place it after that pass.
    g_GameHud->UpdateRadarScoreboard(hud_draw->value != 0.0);
    return result;
}

static void HUD_ResetHandler(HUD_ResetNext next)
{
    ClearHudTxt();

    next->Invoke();

    g_GameHud->Reset();
    ResetInvertMouse();
}

static int HUD_VidInitHandler(HUD_VidInitNext next)
{
    g_GameHud->UpdateRadarScoreboard(false);
    ClearHudTxt();

    next->Invoke();

    ViewVidInit();
    g_GameHud->VidInit();
    MaterialPreview_VidInit();

    return 1;
}

static void Hook_V_CalcRefdef(goldsrc::ref_params_t* pparams, V_CalcRefdefNext next)
{
    ViewCalcRefdef(pparams, next);
}

static void HUD_UpdateClientDataPost(goldsrc::client_data_t* cdata, float flTime, int result)
{
    std::memcpy(&g_LastClientData, cdata, sizeof(goldsrc::client_data_t));

    FovHUD_UpdateClientData(cdata, flTime, result);
    g_GameHud->Think(flTime);
}

static void HUD_PostRunCmdPost(goldsrc::local_state_t* from,
                               goldsrc::local_state_t* to,
                               goldsrc::usercmd_t* cmd,
                               int runfuncs,
                               double time,
                               unsigned int random_seed)
{
    std::memcpy(&g_LastPlayerState, to, sizeof(goldsrc::local_state_t));
}

static void HUD_GetStudioModelInterfacePost(int version,
                                            goldsrc::r_studio_interface_t** ppinterface,
                                            goldsrc::engine_studio_api_t* pstudio,
                                            int result)
{
    std::memcpy(&IEngineStudio, pstudio, sizeof(IEngineStudio));
    std::memcpy(&g_OriginalStudio, *ppinterface, sizeof(g_OriginalStudio));

    (*ppinterface)->StudioDrawModel = StudioDrawModel;
    (*ppinterface)->StudioDrawPlayer = StudioDrawPlayer;
}

static void HUD_CreateEntitiesPost()
{
    MaterialPreview_CreateEntities();
}

static void HUD_PlayerMoveInitPost(goldsrc::playermove_t* ppmove)
{
    pmove = ppmove;
}

static int UserMsg_SetFOVHandler(const char* name, int size, void* data, UserMsg_SetFOVNext next)
{
    return FovMsgFunc_SetFOV(name, size, data, next);
}

static void UserMsg_CurWeaponPost(const char* name, int size, void* data, int result)
{
    MessageReader reader(data, size);

    int state = reader.ReadByte();
    int weaponId = reader.ReadChar();

    if (weaponId < 1)
        g_CurrentWeaponId = 0;
    else if (state)
        g_CurrentWeaponId = weaponId;
}

static void UserMsg_InitHUDPost(const char* name, int size, void* data, int result)
{
    g_GameHud->InitHUDData();
    ResetInvertMouse();
}

static int UserMsg_ResetHUDHandler(const char* name, int size, void* data, UserMsg_RadarMsgNext next)
{
    // Sent on every round restart / respawn; reset the NextClient HUD here (trails,
    // death marks, objectives) since the one-shot gmsgInitHUD does not repeat.
    int result = next->Invoke(name, size, data);
    g_GameHud->Reset();
    return result;
}

static int UserMsg_TextMsgHandler(const char* name, int size, void* data, UserMsg_TextMsgNext next)
{
    static const std::string hiddenServerCmds[] = {
        "client_chat_open",
        "client_chat_team_open",
        "client_chat_close",
    };

    MessageReader reader(data, size);

    const int destType = reader.ReadByte();
    if (destType == 2)
    {
        std::string message = reader.ReadString();
        if (message == "#Game_unknown_command")
        {
            // the Linux cs.so sends the bare command, other servers with a newline after it
            std::string command = reader.ReadString();
            while (!command.empty() && (command.back() == '\n' || command.back() == '\r'))
                command.pop_back();

            if (std::ranges::contains(hiddenServerCmds, command))
            {
                return 1;
            }
        }
    }

    return next->Invoke(name, size, data);
}

static void CL_CreateMoveHandler(float frametime, goldsrc::usercmd_t* cmd, int active, CL_CreateMoveNext next)
{
    CL_CreateMove_InvertMousePre(frametime, cmd, active);

    next->Invoke(frametime, cmd, active);

    CL_CreateMove_InvertMousePost(frametime, cmd, active);
}

static void HUD_ProcessPlayerStateHandler(goldsrc::entity_state_t* dst, const goldsrc::entity_state_t* src, HUD_ProcessPlayerStateNext next)
{
    goldsrc::cl_entity_t* localPlayer = gEngfuncs.GetLocalPlayer();

    if (localPlayer->index == dst->number)
    {
        g_iUser1 = src->iuser1;
        g_iUser2 = src->iuser2;
    }

    next->Invoke(dst, src);
}

class ClientMini : public ClientMiniInterface
{
    std::shared_ptr<taskcoro::TaskCoroImpl> task_runtime_;

    void ShutdownHud()
    {
        RadarHooks_UnpatchHudText();
        g_GameHud.reset();

        if (task_runtime_)
        {
            taskcoro::TaskCoro::UnInitialize();
            task_runtime_.reset();
        }
    }

public:
    void Init(nitroapi::NitroApiInterface* nitro_api) override
    {
        g_NitroApi = nitro_api;
        gHUD = nullptr;

        MathLib_Init();

        nitroapi::ClientData* client_data = nitro_api->GetClientData();
        g_Unsub.emplace_back(client_data->HUD_VidInit |= HUD_VidInitHandler);
        g_Unsub.emplace_back(client_data->HUD_Reset |= HUD_ResetHandler);
        g_Unsub.emplace_back(client_data->HUD_Init += HUD_InitPost);
        g_Unsub.emplace_back(client_data->HUD_Redraw |= HUD_RedrawHandler);
        g_Unsub.emplace_back(client_data->HUD_UpdateClientData += HUD_UpdateClientDataPost);
        g_Unsub.emplace_back(client_data->V_CalcRefdef |= Hook_V_CalcRefdef);
        g_Unsub.emplace_back(client_data->HUD_PostRunCmd += HUD_PostRunCmdPost);
        g_Unsub.emplace_back(client_data->HUD_GetStudioModelInterface += HUD_GetStudioModelInterfacePost);
        g_Unsub.emplace_back(client_data->HUD_CreateEntities += HUD_CreateEntitiesPost);
        g_Unsub.emplace_back(client_data->HUD_PlayerMoveInit += HUD_PlayerMoveInitPost);
        g_Unsub.emplace_back(client_data->UserMsg_SetFOV |= UserMsg_SetFOVHandler);
        g_Unsub.emplace_back(client_data->HUD_ProcessPlayerState |= HUD_ProcessPlayerStateHandler);
        g_Unsub.emplace_back(client_data->UserMsg_CurWeapon += UserMsg_CurWeaponPost);
        g_Unsub.emplace_back(client_data->UserMsg_InitHUD += UserMsg_InitHUDPost);
        g_Unsub.emplace_back(client_data->UserMsg_TextMsg |= UserMsg_TextMsgHandler);
        RadarHooks_Subscribe(nitro_api, g_Unsub);
        g_Unsub.emplace_back(client_data->UserMsg_ResetHUD |= UserMsg_ResetHUDHandler);
        g_Unsub.emplace_back(client_data->CL_CreateMove |= CL_CreateMoveHandler);

        task_runtime_ = std::make_shared<taskcoro::TaskCoroImpl>(std::this_thread::get_id(), false);
        taskcoro::TaskCoro::Initialize(task_runtime_);
        g_GameHud = std::make_unique<GameHud>(nitro_api);
        g_Unsub.emplace_back(client_data->HUD_Shutdown += [this] { ShutdownHud(); });
        g_Unsub.emplace_back(eng()->Sys_Error |= [this](const char* error, const auto& next) {
            // Sys_Error exits the process without Host_Shutdown, so HUD_Shutdown never fires on that path.
            ShutdownHud();
            next->Invoke(error);
        });
    }

    void Uninitialize() override
    {
        RadarHooks_UnpatchHudText();

        ResetInvertMouse();

        for (auto& unsubscriber: g_Unsub) {
            unsubscriber->Unsubscribe();
        }
        g_Unsub.clear();

        ShutdownHud();
        g_NitroApi = nullptr;
        g_GameConsole = nullptr;
        g_GameConsoleNext = nullptr;
        Q_memset(&cl_funcs, 0, sizeof(cl_funcs));
        Q_memset(&gEngfuncs, 0, sizeof(gEngfuncs));
        Q_memset(&g_LastPlayerState, 0, sizeof(g_LastPlayerState));
        Q_memset(&g_LastClientData, 0, sizeof(g_LastClientData));
        gHUD = nullptr;
        Q_memset(&IEngineStudio, 0, sizeof(IEngineStudio));
        Q_memset(&g_OriginalStudio, 0, sizeof(g_OriginalStudio));
        pmove = nullptr;
    }

    void GetVersion(char* buffer, int size) override
    {
        if (buffer != nullptr)
            V_strncpy(buffer, CLIENT_MINI_INTERFACE_VERSION ", " __DATE__ " " __TIME__, size);
    }
};

EXPOSE_SINGLE_INTERFACE(ClientMini, ClientMiniInterface, CLIENT_MINI_INTERFACE_VERSION);
