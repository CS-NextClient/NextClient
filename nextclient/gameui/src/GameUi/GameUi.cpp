#include "GameUi.h"

#include <sys/types.h>
#include <direct.h>
#include <filesystem>

#include <tier1/tier1.h>
#include <tier2/tier2.h>

#include "vgui_controls/Controls.h"
#include "vgui_controls/MessageBox.h"

#include <IEngineVGui.h>
#include <IBaseUI.h>
#include <IBaseSystem.h>
#include <FileSystem.h>
#include "../IServerBrowserEx.h"

#include <vgui/ILocalize.h>
#include <vgui/IScheme.h>
#include <vgui/IVGui.h>
#include <vgui/ISystem.h>
#include <format>

#include "BasePanel.h"
#include "ModInfo.h"
#include "GameConsole.h"
#include "LoadingDialog.h"
#include "DemoPlayerDialog.h"
#include "OptionsSubMiscellaneous.h"
#include "IClientVGUI.h"

#include "Browser/AcceptedDomains.h"
#include "Browser/ExtensionCommon.h"
#include "Browser/ExtensionMatchmaking.h"
#include "Browser/ExtensionMatchmakingListings.h"
#include "Browser/ExtensionGameUiApi.h"

#include "DemoPlayerDialog.h"
#include <shellapi.h>

namespace fs = std::filesystem;

static IServerBrowserEx *g_pServerBrowser = nullptr;
static IBaseSystem* g_pBaseSystem = nullptr;
static IGameClientExports* g_pGameClientExports = nullptr;
static vgui2::DHANDLE<CDemoPlayerDialog> g_hDemoPlayerDialog;
vgui2::DHANDLE<CLoadingDialog> g_hLoadingDialog;
static CGameUI g_GameUI;
static HWND g_MainWindow = nullptr;
static WNDPROC g_MainWindowProc = nullptr;

cl_enginefunc_t* engine = nullptr;

CGameUI &GameUI() { return g_GameUI; }
IBaseSystem* SystemWrapper() { return g_pBaseSystem; }
IGameClientExports* GameClientExports() { return g_pGameClientExports; }

namespace vgui2
{
    static HScheme g_DefaultScheme = -1;

    HScheme VGui_GetDefaultScheme()
    {
        if (g_DefaultScheme != -1)
            return g_DefaultScheme;

        auto miscellaneous_settings = KeyValues::AutoDelete(OptionsSubMiscellaneous::GetSettings());

        const char* scheme_file = miscellaneous_settings->GetString(OptionsSubMiscellaneous::kSchemeKey, OptionsSubMiscellaneous::kDefaultScheme);
        std::string scheme_path = std::format("{}/{}", OptionsSubMiscellaneous::kSchemesPath, scheme_file);

        if (g_pFullFileSystem->FileExists(scheme_path.c_str()))
            g_DefaultScheme = scheme()->LoadSchemeFromFilePath(scheme_path.c_str(), "GAMECONFIG", nullptr);
        else
            g_DefaultScheme = 0;

        return g_DefaultScheme;
    }
}

LRESULT CALLBACK WindowGlobalProcedure(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
        case WM_DROPFILES:
        {
            bool bIsDemoFound = false;
            HDROP hdrop = (HDROP)wParam;
            UINT fileCount = DragQueryFileW(hdrop, 0xFFFFFFFF, NULL, 0);
            for (UINT i = 0; i < fileCount; ++i)
            {
                wchar_t wszFilePath[512];
                DragQueryFileW(hdrop, i, wszFilePath, sizeof(wszFilePath) / sizeof(wchar_t));

                auto uiLength = V_wcslen(wszFilePath);
                if (uiLength > 4 && _wcsicmp(&wszFilePath[uiLength - 4], L".dem") == 0)
                {
                    g_pFullFileSystem->AddSearchPathNoWrite(fs::path(wszFilePath).parent_path().string().c_str(), "GAME");
                    g_GameUI.ActivateDemoUI();
                    g_hDemoPlayerDialog->DemoSelected(fs::path(wszFilePath).filename().string().c_str());
                    bIsDemoFound = true;
                    break;
                }
            }
            DragFinish(hdrop);
            if (bIsDemoFound)
                return 0;
        }
    }
    return CallWindowProc(g_MainWindowProc, hwnd, uMsg, wParam, lParam);
}

EXPOSE_SINGLE_INTERFACE_GLOBALVAR(CGameUI, IGameUI, GAMEUI_INTERFACE_VERSION_GS, g_GameUI);

CGameUI::CGameUI()
{
    m_szPreviousStatusText[0] = 0;
    m_bLoadlingLevel = false;
}

CGameUI::~CGameUI()
{
}

void CGameUI::Initialize(CreateInterfaceFn *factories, int count)
{
    g_MainWindow = GetActiveWindow();

    ConnectTier1Libraries(factories, count);
    ConnectTier2Libraries(factories, count, &g_MainWindow);

    if (!vgui2::VGui_InitInterfacesList("GameUI", factories, count))
        return;

    g_MainWindowProc = (WNDPROC)SetWindowLongPtr(g_MainWindow, GWLP_WNDPROC, (LONG_PTR)WindowGlobalProcedure);

    for (int i = 0; i < count; ++i)
    {
        if (!g_pGameClientExports)
            g_pGameClientExports = (IGameClientExports*) factories[i](GAMECLIENTEXPORTS_INTERFACE_VERSION, NULL);
    }

    g_pServerBrowser = (IServerBrowserEx *)CreateInterface(SERVERBROWSEREX_INTERFACE_VERSION, NULL);

    g_pVGuiLocalize->AddFile(g_pFullFileSystem, "resource/gameui_%language%.txt");
    g_pVGuiLocalize->AddFile(g_pFullFileSystem, "resource/valve_%language%.txt");
    g_pVGuiLocalize->AddFile(g_pFullFileSystem, "resource/vgui_%language%.txt");
    g_pVGuiLocalize->AddFile(g_pFullFileSystem, "resource/nextclient_%language%.txt");

    if (FindPlatformDirectory(m_szPlatformDir, ARRAYSIZE(m_szPlatformDir)))
    {
        char szConfigDir[512];

        strcpy(szConfigDir, m_szPlatformDir);

        auto uiLength = strlen(szConfigDir);

        szConfigDir[uiLength++] = CORRECT_PATH_SEPARATOR;

        strcpy(&szConfigDir[uiLength], "config");
        szConfigDir[uiLength + strlen("config")] = '\0';

        g_pFullFileSystem->AddSearchPath(szConfigDir, "PLATFORMCONFIG");

        _mkdir(szConfigDir);

        vgui2::ivgui()->DPrintf("Platform config directory: %s\n", szConfigDir);

        vgui2::system()->SetUserConfigFile("InGameDialogConfig.vdf", "PLATFORMCONFIG");

        g_pVGuiLocalize->AddFile(g_pFullFileSystem, "resource/platform_%language%.txt");
        g_pVGuiLocalize->AddFile(g_pFullFileSystem, "resource/vgui_%language%.txt");
    }

    auto base_panel = new CBasePanel();
    base_panel->SetBounds(0, 0, 400, 300);
    base_panel->SetPaintBorderEnabled(false);
    base_panel->SetPaintBackgroundEnabled(true);
    base_panel->SetPaintEnabled(false);
    base_panel->SetVisible(true);
    base_panel->SetMouseInputEnabled(false);
    base_panel->SetKeyBoardInputEnabled(false);

    base_panel->SetParent(g_pEngineVGui->GetPanel(PANEL_GAMEUIDLL));

    if (g_pServerBrowser)
        g_pServerBrowser->Initialize(factories, count);

    if (g_pServerBrowser)
        g_pServerBrowser->SetParent(base_panel->GetVPanel());

    vgui2::surface()->SetAllowHTMLJavaScript(true);
}

void CGameUI::Start(cl_enginefuncs_s *engineFuncs, int interfaceVersion, void *system)
{
    engine = engineFuncs;
    g_pBaseSystem = (IBaseSystem*)system;

    ModInfo().LoadCurrentGameInfo();

    if (g_pServerBrowser)
    {
        g_pServerBrowser->ActiveGameName(ModInfo().GetGameDescription(), engine->pfnGetGameDirectory());
        g_pServerBrowser->Reactivate();
    }

    RegisterExtensionCommon(engineFuncs);
    RegisterExtensionMatchmaking();
    RegisterExtensionMatchmakingListings();
    LoadAcceptedDomainsFromDisk("platform/accepted_domains.txt");

    browserExtensionGameUiApi = new ContainerExtensionGameUiApi();
}

void CGameUI::Shutdown(void)
{
    vgui2::system()->SaveUserConfigFile();

    if (g_pServerBrowser)
    {
        g_pServerBrowser->Deactivate();
        g_pServerBrowser->Shutdown();
    }

    ModInfo().FreeModInfo();

    DisconnectTier1Libraries();
    DisconnectTier2Libraries();

    delete browserExtensionGameUiApi;
}

int CGameUI::ActivateGameUI(void)
{
    if (!m_bLoadlingLevel && g_hLoadingDialog.Get() && IsInLevel())
    {
        g_hLoadingDialog->Close();
        g_hLoadingDialog = NULL;
    }

    if(!IsGameUIActive())
        browserExtensionGameUiApi->OnActivateGameUI();

    BasePanel()->OnGameUIActivated();
    BasePanel()->SetVisible(true);

    if (!m_bLoadlingLevel)
        OpenServerBrowserIfNeeded();

    engine->pfnClientCmd("setpause");
        
    m_bFirstActivatePassed = true;
    return 1;
}

int CGameUI::ActivateDemoUI(void)
{
	if (!g_hDemoPlayerDialog.Get())
	{
        g_hDemoPlayerDialog = new CDemoPlayerDialog(BasePanel());
	}

    g_hDemoPlayerDialog->Activate();

    return 1;
}

int CGameUI::HasExclusiveInput(void)
{
    return IsGameUIActive();
}

void CGameUI::RunFrame(void)
{
    int wide, tall;
    vgui2::surface()->GetScreenSize(wide, tall);
    BasePanel()->SetSize(wide, tall);

    if (BasePanel()->IsVisible())
        BasePanel()->RunFrame();

    browserExtensionGameUiApi->RunFrame();
}

void CGameUI::ConnectToServer(const char *game, int IP, int port)
{
    if (g_pServerBrowser)
        g_pServerBrowser->ConnectToGame(IP, port);

    if (m_bNeedApplyMultiplayerGameSettings)
    {
        m_bNeedApplyMultiplayerGameSettings = false;
        engine->pfnClientCmd("echo Applying multiplayer game settings from New Game\n");
        BasePanel()->ApplyMultiplayerGameSettings();
    }

    engine->pfnClientCmd("mp3 stop\n");

    g_pBaseUI->HideGameUI();
}

void CGameUI::DisconnectFromServer(void)
{
    if (g_pServerBrowser)
        g_pServerBrowser->DisconnectFromGame();

    g_pBaseUI->ActivateGameUI();
}

void CGameUI::HideGameUI()
{
    if (!IsInLevel())
        return;

    engine->pfnClientCmd("unpause");
    engine->pfnClientCmd("hideconsole");

    if (!IsGameUIActive())
        return;

    BasePanel()->SetVisible(false);

    if (GameConsole().IsConsoleVisible())
        GameConsole().Hide();

    if (!m_bLoadlingLevel && g_hLoadingDialog.Get())
    {
        g_hLoadingDialog->Close();
        g_hLoadingDialog = NULL;
    }

    browserExtensionGameUiApi->OnHideGameUI();
}

bool CGameUI::IsGameUIActive(void)
{
    return BasePanel()->IsVisible();
}

void CGameUI::LoadingStarted(const char *resourceType, const char *resourceName)
{
    m_bLoadlingLevel = true;

    GameConsole().Hide();
    BasePanel()->OnLevelLoadingStarted(resourceName);
}

void CGameUI::LoadingFinished(const char *resourceType, const char *resourceName)
{
    m_bLoadlingLevel = false;

    BasePanel()->OnLevelLoadingFinished();
    g_pBaseUI->HideGameUI();
}

void CGameUI::StartProgressBar(const char *progressType, int progressSteps)
{
    if (!g_hLoadingDialog.Get())
        g_hLoadingDialog = new CLoadingDialog(BasePanel());

    m_szPreviousStatusText[0] = 0;
    g_hLoadingDialog->SetProgressRange(0, progressSteps);
    g_hLoadingDialog->SetProgressPoint(0.0f);
    g_hLoadingDialog->Open();
}

int CGameUI::ContinueProgressBar(int progressPoint, float progressFraction)
{
    if (!g_hLoadingDialog.Get())
        return 0;

    g_hLoadingDialog->Activate();
    return g_hLoadingDialog->SetProgressPoint(progressPoint);
}

void CGameUI::StopProgressBar(bool bError, const char *failureReason, const char *extendedReason)
{
    if (!g_hLoadingDialog.Get() && bError)
        g_hLoadingDialog = new CLoadingDialog(BasePanel());

    if (!g_hLoadingDialog.Get())
        return;

    if (bError)
    {
        g_hLoadingDialog->DisplayGenericError(failureReason, extendedReason);
    }
    else
    {
        g_hLoadingDialog->Close();
        g_hLoadingDialog = NULL;
    }
}

int CGameUI::SetProgressBarStatusText(const char *statusText)
{
    if (!g_hLoadingDialog.Get())
        return false;

    if (!statusText)
        return false;

    if (!stricmp(statusText, m_szPreviousStatusText))
        return false;

    g_hLoadingDialog->SetStatusText(statusText);
    Q_strncpy(m_szPreviousStatusText, statusText, sizeof(m_szPreviousStatusText));
    return true;
}

void CGameUI::SetSecondaryProgressBar(float progress)
{
    if (!g_hLoadingDialog.Get())
        return;

    g_hLoadingDialog->SetSecondaryProgress(progress);
}

void CGameUI::SetSecondaryProgressBarText(const char *statusText)
{
    if (!g_hLoadingDialog.Get())
        return;

    g_hLoadingDialog->SetSecondaryProgressText(statusText);
}

void CGameUI::ValidateCDKey(bool force, bool inConnect)
{

}

bool CGameUI::IsServerBrowserValid(void)
{
    return g_pServerBrowser != NULL;
}

void CGameUI::ActivateServerBrowser(void)
{
    if (g_pServerBrowser)
    {
        if (!m_bServBrowserFirstActivatePassed)
        {
            m_bServBrowserFirstActivatePassed = true;

            auto misc_settings = KeyValues::AutoDelete(OptionsSubMiscellaneous::GetSettings());

            int tab = misc_settings->GetInt(OptionsSubMiscellaneous::kServerBrowserInitialTabKey, (int)ServerBrowserTab::Internet);
            tab = std::clamp(tab, (int)ServerBrowserTab::Internet, (int)ServerBrowserTab::LAN);

            g_pServerBrowser->Activate((ServerBrowserTab)tab);
        }
        else
        {
            g_pServerBrowser->Activate();
        }
    }
}

bool CGameUI::IsInLevel(void)
{
    const char *levelName = engine->pfnGetLevelName();

    if (levelName[0] != '\0')
        return true;

    return false;
}

bool CGameUI::IsInMultiplayer(void)
{
    return (IsInLevel() && engine->GetMaxClients() > 1);
}

void CGameUI::OpenServerBrowserIfNeeded()
{
    if (m_bFirstActivatePassed)
        return;

    if (!g_pServerBrowser)
        return;

    auto misc_settings = KeyValues::AutoDelete(OptionsSubMiscellaneous::GetSettings());
    if (!misc_settings->GetBool(OptionsSubMiscellaneous::kDisableAutoOpenServerBrowserKey))
    {
        int tab = misc_settings->GetInt(OptionsSubMiscellaneous::kServerBrowserInitialTabKey, (int)ServerBrowserTab::Internet);
        tab = std::clamp(tab, (int)ServerBrowserTab::Internet, (int)ServerBrowserTab::LAN);

        g_pServerBrowser->Activate((ServerBrowserTab)tab);
    }
}

bool CGameUI::FindPlatformDirectory(char* platformDir, int bufferSize)
{
    strncpy(platformDir, "platform", bufferSize);
    return true;
}

void CGameUI::NeedApplyMultiplayerGameSettings()
{
    m_bNeedApplyMultiplayerGameSettings = true;
}

void CGameUI::OnDisconnectFromServer(int eSteamLoginFailure, const char *username)
{
    m_bNeedApplyMultiplayerGameSettings = false;
}
