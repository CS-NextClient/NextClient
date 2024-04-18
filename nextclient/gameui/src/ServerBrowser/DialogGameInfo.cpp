#include "GameUi.h"
#include "DialogGameInfo.h"
#include "IGameList.h"
#include "ServerBrowserDialog.h"
#include "DialogServerPassword.h"
#include <ModInfo.h>

#include <vgui/ISystem.h>
#include <vgui/ISurface.h>
#include <vgui/IVGui.h>
#include <KeyValues.h>

#include <vgui_controls/Label.h>
#include <vgui_controls/TextEntry.h>
#include <vgui_controls/Button.h>
#include <vgui_controls/ToggleButton.h>
#include <vgui_controls/RadioButton.h>

#include <cstdio>
#include <steam/steam_api.h>

using namespace vgui2;

static const long RETRY_TIME = 2500;

CDialogGameInfo::CDialogGameInfo(vgui2::Panel *parent, uint32 ip, uint16 port) :
    Frame(parent, "DialogGameInfo"),
    server_ip_(ip),
    server_port_(port)
{
    SetBounds(0, 0, 512, 512);
    SetMinimumSize(416, 340);
    SetDeleteSelfOnClose(true);

    SetTitle(g_pVGuiLocalize->Find("#ServerBrowser_GameInfoTitle"), true);

    m_bConnecting = false;
    m_bServerFull = false;
    m_bShowAutoRetryToggle = false;
    m_bServerNotResponding = false;

    m_szPassword[0] = 0;

    m_pConnectButton = new Button(this, "Connect", "#ServerBrowser_JoinGame");
    m_pCloseButton = new Button(this, "Close", "#ServerBrowser_Close");
    m_pRefreshButton = new Button(this, "Refresh", "#ServerBrowser_Refresh");
    m_pInfoLabel = new Label(this, "InfoLabel", "");
    m_pAutoRetry = new ToggleButton(this, "AutoRetry", "#ServerBrowser_AutoRetry");
    m_pAutoRetry->AddActionSignalTarget(this);

    m_pAutoRetryAlert = new RadioButton(this, "AutoRetryAlert", "#ServerBrowser_AlertMeWhenSlotOpens");
    m_pAutoRetryJoin = new RadioButton(this, "AutoRetryJoin", "#ServerBrowser_JoinWhenSlotOpens");
    m_pPlayerList = new ListPanel(this, "PlayerList");
    m_pPlayerList->AddColumnHeader(0, "PlayerName", "#ServerBrowser_PlayerName", 156);
    m_pPlayerList->AddColumnHeader(1, "Score", "#ServerBrowser_Score", 64);
    m_pPlayerList->AddColumnHeader(2, "Time", "#ServerBrowser_Time", 64);

    m_pPlayerList->SetSortFunc(2, &PlayerTimeColumnSortFunc);

    PostMessage(m_pPlayerList, new KeyValues("SetSortColumn", "column", 2));
    PostMessage(m_pPlayerList, new KeyValues("SetSortColumn", "column", 1));
    PostMessage(m_pPlayerList, new KeyValues("SetSortColumn", "column", 1));

    m_pAutoRetryAlert->SetSelected(true);

    m_pConnectButton->SetCommand(new KeyValues("Connect"));
    m_pCloseButton->SetCommand(new KeyValues("Close"));
    m_pRefreshButton->SetCommand(new KeyValues("Refresh"));

    m_iRequestRetry = 0;

    ivgui()->AddTickSignal(GetVPanel());

    LoadControlSettings("Servers/DialogGameInfo.res");
    RegisterControlSettingsFile("Servers/DialogGameInfo_SinglePlayer.res");
    RegisterControlSettingsFile("Servers/DialogGameInfo_AutoRetry.res");

    server_item_.m_NetAdr.Init(server_ip_, server_port_, server_port_);
}

CDialogGameInfo::~CDialogGameInfo()
{
    CancelPingQuery();
    CancelPlayerQuery();
}

void CDialogGameInfo::Run(const char *titleName)
{
    if (titleName)
        SetTitle("#ServerBrowser_GameInfoWithNameTitle", true);
    else
        SetTitle("#ServerBrowser_GameInfoTitle", true);

    SetDialogVariable("game", titleName);

    SendPingQueryIfNotAny();
    Activate();
}

void CDialogGameInfo::Connect()
{
    OnConnect();
}

servernetadr_t CDialogGameInfo::GetAddress()
{
    servernetadr_t addr{};
    addr.Init(server_ip_, server_port_, server_port_);

    return addr;
}

void CDialogGameInfo::OnConnect()
{
    m_bConnecting = true;

    m_bServerFull = false;
    m_bServerNotResponding = false;

    InvalidateLayout();
    SendPingQueryIfNotAny();
}

void CDialogGameInfo::OnRefresh()
{
    SendPingQueryIfNotAny();
}

void CDialogGameInfo::OnButtonToggled(Panel *panel)
{
    if (panel == m_pAutoRetry)
    {
        ShowAutoRetryOptions(m_pAutoRetry->IsSelected());
        if (!m_pAutoRetry->IsSelected())
            FlashWindowStop();
    }

    InvalidateLayout();
}

void CDialogGameInfo::OnJoinServerWithPassword(const char *password)
{
    Q_strncpy(m_szPassword, password, sizeof(m_szPassword));

    OnConnect();
}

void CDialogGameInfo::OnConnectToGame(int ip, int port)
{
    if (server_item_.m_NetAdr.GetIP() == ip && server_item_.m_NetAdr.GetConnectionPort() == port)
        Close();
}

void CDialogGameInfo::OnKeyCodeTyped(KeyCode code)
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

void CDialogGameInfo::OnTick()
{
    if (m_iRequestRetry && m_iRequestRetry < system()->GetTimeMillis())
        SendPingQueryIfNotAny();
}

void CDialogGameInfo::PerformLayout()
{
    BaseClass::PerformLayout();

    SetControlStringNoLocalize("ServerText", server_item_.GetName().c_str());
    SetControlStringNoLocalize("GameText", server_item_.m_szGameDescription);
    SetControlStringNoLocalize("MapText", server_item_.m_szMap);


    if (!m_bServerHadSuccessfulResponse)
        SetControlString("SecureText", "");
    else if (server_item_.m_bSecure)
        SetControlString("SecureText", "#VAC_Secure");
    else
        SetControlString("SecureText", "#ServerBrowser_NotSecure");

    char buf[128];

    if (server_item_.m_nMaxPlayers > 0)
    {
        Q_snprintf(buf, sizeof(buf), "%d / %d", server_item_.m_nPlayers, server_item_.m_nMaxPlayers);
    }
    else
        buf[0] = 0;

    SetControlStringNoLocalize("PlayersText", buf);

    if (server_item_.m_NetAdr.GetIP() && server_item_.m_NetAdr.GetConnectionPort())
    {
        char buf[64];
        sprintf(buf, "%s", server_item_.m_NetAdr.GetConnectionAddressString().c_str());
        SetControlStringNoLocalize("ServerIPText", buf);
        m_pConnectButton->SetEnabled(true);

        if (m_pAutoRetry->IsSelected())
        {
            m_pAutoRetryAlert->SetVisible(true);
            m_pAutoRetryJoin->SetVisible(true);
        }
        else
        {
            m_pAutoRetryAlert->SetVisible(false);
            m_pAutoRetryJoin->SetVisible(false);
        }
    }
    else
    {
        SetControlStringNoLocalize("ServerIPText", "");
        m_pConnectButton->SetEnabled(false);
    }

    if (m_bServerHadSuccessfulResponse && server_item_.m_nPing < 1200)
    {
        Q_snprintf(buf, sizeof(buf), "%d", server_item_.m_nPing);
        SetControlStringNoLocalize("PingText", buf);
    }
    else
    {
        SetControlStringNoLocalize("PingText", "");
    }

    if (m_pAutoRetry->IsSelected())
    {
        if ((server_item_.m_nPlayers + server_item_.m_nBotPlayers) < server_item_.m_nMaxPlayers)
            m_pInfoLabel->SetText("#ServerBrowser_PressJoinToConnect");
        else if (m_pAutoRetryJoin->IsSelected())
            m_pInfoLabel->SetText("#ServerBrowser_JoinWhenSlotIsFree");
        else
            m_pInfoLabel->SetText("#ServerBrowser_AlertWhenSlotIsFree");
    }
    else if (m_bServerFull)
    {
        m_pInfoLabel->SetText("#ServerBrowser_CouldNotConnectServerFull");
    }
    else if (m_bServerNotResponding)
    {
        m_pInfoLabel->SetText("#ServerBrowser_ServerNotResponding");
    }
    else
    {
        m_pInfoLabel->SetText("");
    }

    if (m_bServerHadSuccessfulResponse && (server_item_.m_nPlayers + server_item_.m_nBotPlayers) == 0)
        m_pPlayerList->SetEmptyListText("#ServerBrowser_ServerHasNoPlayers");
    else
        m_pPlayerList->SetEmptyListText("#ServerBrowser_ServerNotResponding");

    m_pAutoRetry->SetVisible(m_bShowAutoRetryToggle);

    Repaint();
}

void CDialogGameInfo::AddPlayerToList(const char *playerName, int score, float timePlayedSeconds)
{
    if (!first_player_responded_)
    {
        ClearPlayerList();
        first_player_responded_ = true;
    }

    auto *player = new KeyValues("player");
    player->SetString("PlayerName", playerName);
    player->SetInt("Score", score);
    player->SetInt("TimeSec", (int)timePlayedSeconds);

    int seconds = (int)timePlayedSeconds;
    int minutes = seconds / 60;
    int hours = minutes / 60;

    seconds %= 60;
    minutes %= 60;

    char buf[64];
    buf[0] = 0;

    if (hours)
        Q_snprintf(buf, sizeof(buf), "%dh %dm %ds", hours, minutes, seconds);
    else if (minutes)
        Q_snprintf(buf, sizeof(buf), "%dm %ds", minutes, seconds);
    else
        Q_snprintf(buf, sizeof(buf), "%ds", seconds);

    player->SetString("Time", buf);

    m_pPlayerList->AddItem(player, 0, false, true);
    player->deleteThis();
}

void CDialogGameInfo::PlayersFailedToRespond()
{
    ClearPlayerList();
    players_server_query_ = 0;
}

void CDialogGameInfo::PlayersRefreshComplete()
{
    players_server_query_ = 0;
}

void CDialogGameInfo::ServerResponded(gameserveritem_t &server)
{
    m_hPingServerQuery = 0;
    m_bServerHadSuccessfulResponse = true;
    server_item_ = server;

    bool connect_success = false;

    if (m_bConnecting)
    {
        m_bConnecting = false;
        connect_success = ConnectToServer();
    }
    else if (m_pAutoRetry->IsSelected())
    {
        if ((server_item_.m_nPlayers + server_item_.m_nBotPlayers) < server_item_.m_nMaxPlayers)
        {
            surface()->PlaySound("servers/game_ready.wav");
            FlashWindow();

            if (m_pAutoRetryJoin->IsSelected())
                connect_success = ConnectToServer();
        }
        else
        {
            FlashWindowStop();
        }
    }

    // we are stay in dialog, so update players list
    if (!connect_success)
        SendPlayerQuery();

    m_bServerNotResponding = false;

    InvalidateLayout();
    Repaint();
}

void CDialogGameInfo::ServerFailedToRespond()
{
    m_hPingServerQuery = 0;
    m_bServerNotResponding = true;
    m_bServerHadSuccessfulResponse = false;

    InvalidateLayout();
    Repaint();
}

void CDialogGameInfo::ShowAutoRetryOptions(bool state)
{
    int growSize = 60;

    if (!state)
        growSize = -growSize;

    int x, y, wide, tall;
    GetBounds(x, y, wide, tall);
    SetMinimumSize(416, 340);

    if (state)
        LoadControlSettings("Servers/DialogGameInfo_AutoRetry.res");
    else
        LoadControlSettings("Servers/DialogGameInfo.res");

    m_pAutoRetryAlert->SetSelected(true);

    SetBounds(x, y, wide, tall + growSize);
    InvalidateLayout();
}

void CDialogGameInfo::SendPlayerQuery()
{
    if (players_server_query_)
        return;

    first_player_responded_ = false;
    players_server_query_ = SteamMatchmakingServers()->PlayerDetails(server_ip_, server_port_, this);
}

void CDialogGameInfo::CancelPlayerQuery()
{
    if (players_server_query_)
    {
        SteamMatchmakingServers()->CancelServerQuery(players_server_query_);
        players_server_query_ = 0;
    }
}

void CDialogGameInfo::SendPingQueryIfNotAny()
{
    if (m_hPingServerQuery)
        return;

    m_iRequestRetry = system()->GetTimeMillis() + RETRY_TIME;
    m_hPingServerQuery = SteamMatchmakingServers()->PingServer(server_ip_, server_port_, this);
}

void CDialogGameInfo::CancelPingQuery()
{
    if (m_hPingServerQuery)
    {
        SteamMatchmakingServers()->CancelServerQuery(m_hPingServerQuery);
        m_iRequestRetry = 0;
        m_hPingServerQuery = 0;
    }
}

void CDialogGameInfo::ClearPlayerList()
{
    m_pPlayerList->DeleteAllItems();

    Repaint();
}

void CDialogGameInfo::ApplyConnectCommand(const gameserveritem_t &server)
{
    char command[256];

    if (m_szPassword[0])
    {
        // Remove model from setinfo because we need place for password. model is not used in cs 1.6
        if (!stricmp(ModInfo().GetGameDescription(), "Counter-Strike"))
            engine->pfnClientCmd("setinfo model \"\"");

        Q_snprintf(command, Q_ARRAYSIZE(command), "password \"%s\"\n", m_szPassword);
        engine->pfnClientCmd(command);
    }

    char buf[64];
    Q_snprintf(buf, sizeof(buf), "%s", server.m_NetAdr.GetConnectionAddressString().c_str());
    Q_snprintf(command, Q_ARRAYSIZE(command), "connect %s\n", buf);

    engine->pfnClientCmd(command);
}

bool CDialogGameInfo::ConnectToServer()
{
    if (server_item_.m_bPassword && !m_szPassword[0])
    {
        auto *box = new CDialogServerPassword(this);
        box->AddActionSignalTarget(this);
        box->Activate(server_item_.GetName().c_str());
        box->DoModal();
        return false;
    }

    if ((server_item_.m_nPlayers + server_item_.m_nBotPlayers) >= server_item_.m_nMaxPlayers)
    {
        m_bServerFull = true;
        m_bShowAutoRetryToggle = true;

        InvalidateLayout();
        return false;
    }

    ApplyConnectCommand(server_item_);
    PostMessage(this, new KeyValues("Close"));
    return true;
}

int CDialogGameInfo::PlayerTimeColumnSortFunc(ListPanel *pPanel, const ListPanelItem &p1, const ListPanelItem &p2)
{
    int p1time = p1.kv->GetInt("TimeSec");
    int p2time = p2.kv->GetInt("TimeSec");

    if (p1time > p2time)
        return -1;
    if (p1time < p2time)
        return 1;

    return 0;
}