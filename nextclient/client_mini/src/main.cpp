#include "main.h"
#include <cstring>
#include <next_client_mini/client_mini.h>
#include <parsemsg.h>
#include "studiorenderer.h"
#include "view.h"
#include "fov.h"
#include "color_chat_in_console.h"
#include "inspect.h"

nitroapi::NitroApiInterface* g_NitroApi;

IGameConsole* g_GameConsole;
IGameConsoleNext* g_GameConsoleNext;

cldll_func_t cl_funcs;
cl_enginefunc_t gEngfuncs;
enginefuncs_t g_engfuncs;
local_state_t g_LastPlayerState;
client_data_t g_LastClientData;
server_t* sv;
server_static_t* sv_static;

gamehud_t* gHUD;
int g_iUser1;
int g_iUser2;
int g_CurrentWeaponId;

engine_studio_api_t IEngineStudio;
r_studio_interface_t g_OriginalStudio;
playermove_t* pmove;

std::unique_ptr<GameHud> g_GameHud;
static std::vector<std::shared_ptr<nitroapi::Unsubscriber>> g_Unsub;

cvar_t* hud_draw;

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
        "cl_dlls/gameui.so"  // in linux version we have only original gameui under valve folder
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
    g_GameHud->Init();

    hud_draw = g_engfuncs.pfnCVarGetPointer("hud_draw");

    ColorChatInConsolePatch();
}

static void HUD_RedrawPost(float flTime, int iIntermission, int result)
{
    if(hud_draw->value != 0.0)
        g_GameHud->Draw(flTime);
}

static void HUD_ResetHandler(HUD_ResetNext next)
{
    ClearHudTxt();

    next->Invoke();

    g_GameHud->Reset();
}

static int HUD_VidInitHandler(HUD_VidInitNext next)
{
    ClearHudTxt();

    next->Invoke();

    ViewVidInit();
    g_GameHud->VidInit();

    return 1;
}

static void Hook_V_CalcRefdef(ref_params_s* pparams, V_CalcRefdefNext next)
{
    ViewCalcRefdef(pparams, next);
}

static void HUD_UpdateClientDataPost(client_data_t* cdata, float flTime, int result)
{
    std::memcpy(&g_LastClientData, cdata, sizeof(client_data_t));

    FovHUD_UpdateClientData(cdata, flTime, result);
    g_GameHud->Think(flTime);
}

static void HUD_PostRunCmdPost(struct local_state_s *from, struct local_state_s *to, struct usercmd_s *cmd, int runfuncs, double time, unsigned int random_seed)
{
    std::memcpy(&g_LastPlayerState, to, sizeof(local_state_t));
}

static void HUD_GetStudioModelInterfacePost(int version, r_studio_interface_t **ppinterface, engine_studio_api_t *pstudio, int result)
{
    std::memcpy(&IEngineStudio, pstudio, sizeof(IEngineStudio));
    std::memcpy(&g_OriginalStudio, *ppinterface, sizeof(g_OriginalStudio));

    (*ppinterface)->StudioDrawModel = StudioDrawModel;
}

static void HUD_PlayerMoveInitPost(playermove_t* ppmove)
{
    pmove = ppmove;
}

static int UserMsg_SetFOVHandler(const char* name, int size, void* data, UserMsg_SetFOVNext next)
{
    return FovMsgFunc_SetFOV(name, size, data, next);
}

static void UserMsg_CurWeaponPost(const char* name, int size, void* data, int result)
{
    BEGIN_READ(data, size);

    int state = READ_BYTE();
    int weaponId = READ_CHAR();

    if (weaponId < 1)
        g_CurrentWeaponId = 0;
    else if (state)
        g_CurrentWeaponId = weaponId;
}

static void UserMsg_InitHUDPost(const char* name, int size, void* data, int result)
{
    g_GameHud->InitHUDData();
}

static int UserMsg_TextMsgHandler(const char* name, int size, void* data, UserMsg_TextMsgNext next)
{
    const char *hiddenServerCmds[] = {
        "chat_open",
        "chat_team_open",
        "chat_close"
    };
    const size_t hiddenServerCmdsNum = std::size(hiddenServerCmds);

    BEGIN_READ(data, size);

    const int destType = READ_BYTE();
    if (destType == 2)
    {
        std::string message = READ_STRING();
        if (message == "#Game_unknown_command")
        {
            std::string command = READ_STRING();
            for (int i = 0; i < hiddenServerCmdsNum; i++)
            {
                if (command == hiddenServerCmds[i])
                    return 1;
            }
        }
    }

    return next->Invoke(name, size, data);
}

static void HUD_ProcessPlayerStateHandler(entity_state_s* dst, const entity_state_s* src, HUD_ProcessPlayerStateNext next)
{
    cl_entity_t* localPlayer = gEngfuncs.GetLocalPlayer();

    if (localPlayer->index == dst->number)
    {
        g_iUser1 = src->iuser1;
        g_iUser2 = src->iuser2;
    }

    next->Invoke(dst, src);
}

class ClientMini : public ClientMiniInterface
{
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
        g_Unsub.emplace_back(client_data->HUD_Redraw += HUD_RedrawPost);
        g_Unsub.emplace_back(client_data->HUD_UpdateClientData += HUD_UpdateClientDataPost);
        g_Unsub.emplace_back(client_data->V_CalcRefdef |= Hook_V_CalcRefdef);
        g_Unsub.emplace_back(client_data->HUD_PostRunCmd += HUD_PostRunCmdPost);
        g_Unsub.emplace_back(client_data->HUD_GetStudioModelInterface += HUD_GetStudioModelInterfacePost);
        g_Unsub.emplace_back(client_data->HUD_PlayerMoveInit += HUD_PlayerMoveInitPost);
        g_Unsub.emplace_back(client_data->UserMsg_SetFOV |= UserMsg_SetFOVHandler);
        g_Unsub.emplace_back(client_data->HUD_ProcessPlayerState |= HUD_ProcessPlayerStateHandler);
        g_Unsub.emplace_back(client_data->UserMsg_CurWeapon += UserMsg_CurWeaponPost);
        g_Unsub.emplace_back(client_data->UserMsg_InitHUD += UserMsg_InitHUDPost);
        g_Unsub.emplace_back(client_data->UserMsg_TextMsg |= UserMsg_TextMsgHandler);

        g_GameHud = std::make_unique<GameHud>(nitro_api);
    }

    void Uninitialize() override
    {
        for (auto& unsubscriber: g_Unsub) {
            unsubscriber->Unsubscribe();
        }
        g_Unsub.clear();

        g_GameHud.reset();
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
