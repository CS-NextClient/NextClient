
#include <cstdio>
#include <cstring>
#include <cstdarg>
#include <cassert>
#include <ctime>

#include <vgui/IInput.h>
#include <vgui/ISurfaceNext.h>
#include <vgui/ISchemeNext.h>
#include <vgui/IVGui.h>
#include <vgui/ISystem.h>
#include <KeyValues.h>
#include <vgui/MouseCode.h>
#include "FileSystem.h"

#include <vgui_controls/Button.h>
#include <vgui_controls/CheckButton.h>
#include <vgui_controls/ComboBox.h>
#include <vgui_controls/FocusNavGroup.h>
#include <vgui_controls/Frame.h>
#include <vgui_controls/ListPanel.h>
#include <vgui_controls/MessageBox.h>
#include <vgui_controls/Panel.h>
#include <vgui_controls/PropertySheet.h>
#include <vgui_controls/QueryBox.h>
#include <vgui_controls/AnimationController.h>

#include "ServerContextMenu.h"
#include "ServerBrowserDialog.h"
#include "DialogGameInfo.h"

#include "InternetGames.h"
#include "FavoriteGames.h"
#include "SpectateGames.h"
#include "LanGames.h"
#include "HistoryGames.h"
#include "GameUi.h"

#include <netadr.h>

using namespace vgui2;

static CServerBrowserDialog *s_InternetDlg = NULL;

CServerBrowserDialog &ServerBrowserDialog()
{
    return *CServerBrowserDialog::GetInstance();
}

CServerBrowserDialog *CServerBrowserDialog::GetInstance()
{
    return s_InternetDlg;
}

CServerBrowserDialog::CServerBrowserDialog(vgui2::Panel *parent) : Frame(parent, "CServerBrowserDialog")
{
    s_InternetDlg = this;

    m_szGameName[0] = 0;
    m_szModDir[0] = 0;
    m_pSavedData = nullptr;
    m_pFilterData = nullptr;
    m_pFavorites = nullptr;
    m_pHistory = nullptr;
    m_pUniqueGames = nullptr;
    m_pFriendsGames = nullptr;

    LoadUserData();

    m_pInternetGames = new CInternetGames(this, false);
    m_pFavorites = new CFavoriteGames(this);
    m_pHistory = new CHistoryGames(this);
    m_pUniqueGames = new CUniqueGames(this);
    //m_pSpectateGames = new CSpectateGames(this);
    m_pLanGames = new CLanGames(this);
    m_pFriendsGames = new CFriendsGames(this);

    SetMinimumSize(640, 384);
    SetSize(960, 640);
    SetVisible(false);

    m_pGameList = m_pInternetGames;
    m_pContextMenu = new CServerContextMenu(this);
    m_pContextMenu->SetVisible(false);

    m_pTabPanel = new PropertySheet(this, "GameTabs");
    m_pTabPanel->SetTabWidth(72);

    m_pTabPanel->AddPage(m_pInternetGames, "#ServerBrowser_InternetTab");
    m_pTabPanel->AddPage(m_pFavorites, "#ServerBrowser_FavoritesTab");
    m_pTabPanel->AddPage(m_pUniqueGames, "#ServerBrowser_UniqueTab");
    m_pTabPanel->AddPage(m_pHistory, "#ServerBrowser_HistoryTab");
    //m_pTabPanel->AddPage(m_pSpectateGames, "#ServerBrowser_SpectateTab");
    m_pTabPanel->AddPage(m_pLanGames, "#ServerBrowser_LanTab");
    if (m_pFriendsGames)
        m_pTabPanel->AddPage(m_pFriendsGames, "#ServerBrowser_FriendsTab");
    m_pTabPanel->AddActionSignalTarget(this);

    m_pStatusLabel = new Label(this, "StatusLabel", "");

    LoadControlSettings("servers/DialogServerBrowser.res");

    m_pStatusLabel->SetText("");

    // HACK!! to make work button "Add current server button" work properly
    m_pTabPanel->SetActivePage(m_pFavorites);
    m_pTabPanel->SetActivePage(m_pInternetGames);

    //LoadActiveTab();
}

CServerBrowserDialog::~CServerBrowserDialog()
{
    delete m_pContextMenu;

    SaveUserData();

    if (m_pSavedData)
        m_pSavedData->deleteThis();
}

void CServerBrowserDialog::OnKeyCodeTyped(vgui2::KeyCode code)
{
    if (!GameUI().IsInLevel() && code == vgui2::KEY_ESCAPE)
    {
        Close();
    }
    else
    {
        BaseClass::OnKeyCodeTyped(code);
    }
}

void CServerBrowserDialog::OnCommand(const char* command)
{
    if (!Q_strcmp(command, "Close"))
    {
        for (int i = 0; i < m_pTabPanel->GetNumPages(); i++)
        {
            vgui2::Panel* pPanel = m_pTabPanel->GetPage(i);
            if (pPanel)
                pPanel->OnCommand("Close");
        }

        CloseAllGameInfoDialogs();
    }

    BaseClass::OnCommand(command);
}

void CServerBrowserDialog::Initialize()
{
    SetTitle("#ServerBrowser_Servers", true);
    SetVisible(false);
}

void CServerBrowserDialog::Open()
{
    LoadUserConfig("CServerBrowserDialog");
    Activate();

    m_pTabPanel->RequestFocus();

    ivgui()->PostMessage(m_pTabPanel->GetActivePage()->GetVPanel(), new KeyValues("PageShow"), GetVPanel());
}

void CServerBrowserDialog::LoadUserData()
{
    if (m_pSavedData)
        m_pSavedData->deleteThis();

    m_pSavedData = new KeyValues("ServerBrowser");
    m_pSavedData->LoadFromFile(g_pFullFileSystem, kUserSaveDataPath, "PLATFORMCONFIG");

    KeyValues *filters = m_pSavedData->FindKey("Filters", false);

    if (filters)
    {
        m_pFilterData = filters->MakeCopy();
        m_pSavedData->RemoveSubKey(filters);
    }
    else
        m_pFilterData = new KeyValues("Filters");

    InvalidateLayout();
    Repaint();
}

void CServerBrowserDialog::ActivateTab(ServerBrowserTab tab)
{
    if (tab == ServerBrowserTab::Internet)
        m_pTabPanel->SetActivePage(m_pInternetGames);
    else if (tab == ServerBrowserTab::Favorites)
        m_pTabPanel->SetActivePage(m_pFavorites);
    else if (tab == ServerBrowserTab::History)
        m_pTabPanel->SetActivePage(m_pHistory);
    else if (tab == ServerBrowserTab::Unique)
        m_pTabPanel->SetActivePage(m_pUniqueGames);
    else if (tab == ServerBrowserTab::LAN)
        m_pTabPanel->SetActivePage(m_pLanGames);
    else if (m_pFriendsGames && tab == ServerBrowserTab::Friends)
        m_pTabPanel->SetActivePage(m_pFriendsGames);
}

void CServerBrowserDialog::SaveUserData()
{
    if (m_pGameList == m_pFavorites)
        m_pSavedData->SetString("GameList", "favorites");
    else if (m_pGameList == m_pHistory)
        m_pSavedData->SetString("GameList", "history");
    else if (m_pGameList == m_pUniqueGames)
        m_pSavedData->SetString("GameList", "unique");
    else if (m_pGameList == m_pLanGames)
        m_pSavedData->SetString("GameList", "lan");
    else if (m_pFriendsGames && m_pGameList == m_pFriendsGames)
        m_pSavedData->SetString("GameList", "friends");
    else
        m_pSavedData->SetString("GameList", "internet");

    m_pSavedData->RemoveSubKey(m_pSavedData->FindKey("Filters"));
    m_pSavedData->AddSubKey(m_pFilterData->MakeCopy());
    m_pSavedData->SaveToFile(g_pFullFileSystem, kUserSaveDataPath, "PLATFORMCONFIG");
}

KeyValues *CServerBrowserDialog::GetFilterSaveData(const char *filterSet)
{
    return m_pFilterData->FindKey(filterSet, true);
}

Panel *CServerBrowserDialog::GetActivePage()
{
    return m_pTabPanel->GetActivePage();
}

void CServerBrowserDialog::RefreshCurrentPage()
{
    if (m_pGameList)
        m_pGameList->StartRefresh();
}

void CServerBrowserDialog::UpdateStatusText(const char *fmt, ...)
{
    if (!m_pStatusLabel)
        return;

    if (fmt && strlen(fmt) > 0)
    {
        char str[1024];
        va_list argptr;
            va_start(argptr, fmt);
        _vsnprintf(str, sizeof(str), fmt, argptr);
            va_end(argptr);

        m_pStatusLabel->SetText(str);
    }
    else
        m_pStatusLabel->SetText("");
}

void CServerBrowserDialog::UpdateStatusText(wchar_t *unicode)
{
    if (!m_pStatusLabel)
        return;

    if (unicode && wcslen(unicode) > 0)
        m_pStatusLabel->SetText(unicode);
    else
        m_pStatusLabel->SetText("");
}

CServerContextMenu *CServerBrowserDialog::GetContextMenu(vgui2::Panel *pPanel)
{
    if (m_pContextMenu)
        delete m_pContextMenu;

    m_pContextMenu = new CServerContextMenu(this);
    m_pContextMenu->SetAutoDelete(false);

    if (!pPanel)
        m_pContextMenu->SetParent(this);
    else
        m_pContextMenu->SetParent(pPanel);

    m_pContextMenu->SetVisible(false);
    return m_pContextMenu;
}

CDialogGameInfo *CServerBrowserDialog::OpenGameInfoDialog(IGameList *gameList, unsigned int serverIndex)
{
    serveritem_t &server = gameList->GetServer(serverIndex);

    auto *gameDialog = new CDialogGameInfo(&ServerBrowserDialog(), server.gs.m_NetAdr.GetIP(), server.gs.m_NetAdr.GetQueryPort());
    gameDialog->AddActionSignalTarget(this);
    gameDialog->Run(server.gs.GetName().c_str());
    gameDialog->MoveToCenterOfScreen();

    int i = m_GameInfoDialogs.AddToTail();
    m_GameInfoDialogs[i] = gameDialog;
    return gameDialog;
}

CDialogGameInfo *CServerBrowserDialog::OpenGameInfoDialog(uint32 serverIP, uint16 serverPort, const char *titleName)
{
    auto *gameDialog = new CDialogGameInfo(&ServerBrowserDialog(), serverIP, serverPort);
    gameDialog->AddActionSignalTarget(this);
    gameDialog->Run(titleName);
    gameDialog->MoveToCenterOfScreen();

    int i = m_GameInfoDialogs.AddToTail();
    m_GameInfoDialogs[i] = gameDialog;
    return gameDialog;
}

void CServerBrowserDialog::CloseAllGameInfoDialogs()
{
    for (int i = 0; i < m_GameInfoDialogs.Count(); i++)
    {
        vgui2::Panel *dlg = m_GameInfoDialogs[i];

        if (dlg)
            vgui2::ivgui()->PostMessage(dlg->GetVPanel(), new KeyValues("Close"), NULL);
    }
}

const char *CServerBrowserDialog::GetActiveGameName()
{
    return m_szGameName[0] ? m_szGameName : NULL;
}

const char *CServerBrowserDialog::GetActiveModName()
{
    return m_szModDir[0] ? m_szModDir : NULL;
}

serveritem_t &CServerBrowserDialog::GetServer(unsigned int serverID)
{
    return m_pGameList->GetServer(serverID);
}

void CServerBrowserDialog::AddServerToFavorites(uint32_t ip, uint16_t port)
{
    m_pFavorites->AddNewServer(ip, port);
    m_pFavorites->StartRefresh();
}

void CServerBrowserDialog::AddServerToFavorites(const gameserveritem_t& serveritem)
{
    AddServerToFavorites(serveritem.m_NetAdr.GetIP(), serveritem.m_NetAdr.GetConnectionPort());
}

CDialogGameInfo *CServerBrowserDialog::JoinGame(IGameList *gameList, unsigned int serverIndex, GuiConnectionSource connection_source)
{
    serveritem_t& server = gameList->GetServer(serverIndex);

    GameUINext().SetLastConnectionInfo(server.gs.m_NetAdr, connection_source, server.gs.m_szMap);

    CDialogGameInfo *gameDialog = OpenGameInfoDialog(gameList, serverIndex);
    gameDialog->Connect();

    return gameDialog;
}

CDialogGameInfo *CServerBrowserDialog::JoinGame(uint32 serverIP, uint16 serverPort, const char *titleName)
{
    servernetadr_t addr{};
    addr.Init(serverIP, serverPort, serverPort);

    GameUINext().SetLastConnectionInfo(addr, GuiConnectionSource::Unknown, "");

    CDialogGameInfo *gameDialog = OpenGameInfoDialog(serverIP, serverPort, titleName);
    gameDialog->Connect();

    return gameDialog;
}

void CServerBrowserDialog::GetInternetFilterState(FilterState* out)
{
    m_pInternetGames->GetFilterState(out);
}

void CServerBrowserDialog::ReloadFilterSettings()
{
    m_pInternetGames->LoadFilterSettings();
    //m_pSpectateGames->LoadFilterSettings();
    m_pFavorites->LoadFilterSettings();
    m_pLanGames->LoadFilterSettings();
    m_pHistory->LoadFilterSettings();
    m_pUniqueGames->LoadFilterSettings();
    if (m_pFriendsGames)
        m_pFriendsGames->LoadFilterSettings();
}

void CServerBrowserDialog::GetMostCommonQueryPorts(CUtlVector<uint16> &ports)
{
    for (int i = 0; i <= 30; i++)
    {
        ports.AddToTail(27015 + i);
    }
#ifdef _DEBUG
    ports.AddToTail(4242);
#endif
}

bool CServerBrowserDialog::GetDefaultScreenPosition(int &x, int &y, int &wide, int &tall)
{
    int wx, wy, ww, wt;
    vgui2::surface()->GetWorkspaceBounds(wx, wy, ww, wt);

    wide = ww * .86f;
    tall = wt * .86f;

    x = wx + ((ww - wide) / 2);
    y = wy + ((wt - tall) / 2);

    return true;
}

void CServerBrowserDialog::ActivateBuildMode()
{
    EditablePanel *panel = dynamic_cast<EditablePanel *>(m_pTabPanel->GetActivePage());

    if (!panel)
        return;

    panel->ActivateBuildMode();
}

void CServerBrowserDialog::OnGameListChanged()
{
    m_pGameList = dynamic_cast<IGameList *>(m_pTabPanel->GetActivePage());

    UpdateStatusText("");
    InvalidateLayout();
    Repaint();
}

void CServerBrowserDialog::OnActiveGameName(KeyValues *pKV)
{
    Q_strncpy(m_szModDir, pKV->GetString("name"), sizeof(m_szModDir));
    Q_strncpy(m_szGameName, pKV->GetString("game"), sizeof(m_szGameName));

    ReloadFilterSettings();
}

void CServerBrowserDialog::OnConnectToGame(KeyValues *pMessageValues)
{
    uint32_t ip = BigLong(pMessageValues->GetInt("ip")); // byte swap for convert network order -> host order
    uint16_t connectionPort = pMessageValues->GetInt("connectionport");

    if (!ip)
        return;

    m_CurrentConnection.Init(ip, connectionPort, connectionPort);

    if (m_pHistory)
        SteamMatchmaking()->AddFavoriteGame(SteamUtils()->GetAppID(), ip, connectionPort, connectionPort, k_unFavoriteFlagHistory,std::time(nullptr));

    for (int i = 0; i < m_GameInfoDialogs.Count(); i++)
    {
        vgui2::Panel *dlg = m_GameInfoDialogs[i];

        if (dlg)
        {
            auto *kv = new KeyValues("ConnectedToGame", "ip", ip, "connectionport", connectionPort);
            vgui2::ivgui()->PostMessage(dlg->GetVPanel(), kv, NULL);
        }
    }

    vgui2::ivgui()->PostMessage(m_pInternetGames->GetVPanel(), new KeyValues("ConnectedToGame", "ip", ip, "connectionport", connectionPort), NULL);
    vgui2::ivgui()->PostMessage(m_pFavorites->GetVPanel(), new KeyValues("ConnectedToGame", "ip", ip, "connectionport", connectionPort), NULL);
    vgui2::ivgui()->PostMessage(m_pHistory->GetVPanel(), new KeyValues("ConnectedToGame", "ip", ip, "connectionport", connectionPort), NULL);
    vgui2::ivgui()->PostMessage(m_pUniqueGames->GetVPanel(), new KeyValues("ConnectedToGame", "ip", ip, "connectionport", connectionPort), NULL);
    vgui2::ivgui()->PostMessage(m_pLanGames->GetVPanel(), new KeyValues("ConnectedToGame", "ip", ip, "connectionport", connectionPort), NULL);
    if (m_pFriendsGames)
        vgui2::ivgui()->PostMessage(m_pFriendsGames->GetVPanel(), new KeyValues("ConnectedToGame", "ip", ip, "connectionport", connectionPort), NULL);

    m_bCurrentlyConnected = true;
}

void CServerBrowserDialog::OnDisconnectFromGame()
{
    vgui2::ivgui()->PostMessage(m_pInternetGames->GetVPanel(), new KeyValues("DisconnectedFromGame"), NULL);
    vgui2::ivgui()->PostMessage(m_pFavorites->GetVPanel(),  new KeyValues("DisconnectedFromGame"), NULL);
    vgui2::ivgui()->PostMessage(m_pHistory->GetVPanel(),  new KeyValues("DisconnectedFromGame"), NULL);
    vgui2::ivgui()->PostMessage(m_pUniqueGames->GetVPanel(),  new KeyValues("DisconnectedFromGame"), NULL);
    vgui2::ivgui()->PostMessage(m_pLanGames->GetVPanel(),  new KeyValues("DisconnectedFromGame"), NULL);
    if (m_pFriendsGames)
        vgui2::ivgui()->PostMessage(m_pFriendsGames->GetVPanel(),  new KeyValues("DisconnectedFromGame"), NULL);

    m_bCurrentlyConnected = false;
    m_CurrentConnection = servernetadr_t();
}
