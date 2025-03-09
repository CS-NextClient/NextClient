#include "DialogAddServer.h"
#include "ServerBrowserDialog.h"
#include "ServerListCompare.h"
#include "IGameList.h"

#include <vgui/IVGui.h>
#include <vgui/ILocalize.h>
#include <KeyValues.h>

#include <vgui_controls/ImageList.h>
#include <vgui_controls/Button.h>
#include <vgui_controls/MessageBox.h>
#include <vgui_controls/PropertySheet.h>
#include <vgui_controls/ListPanel.h>
#include <vgui_controls/TextEntry.h>

#include <nitro_utils/net_utils.h>

using namespace vgui2;

#undef PostMessage
#undef MessageBox

CAddServerGameList::CAddServerGameList(Panel *parent, const char *panelName) : BaseClass(parent, panelName)
{
}

void CAddServerGameList::OnKeyCodeTyped(vgui2::KeyCode code)
{
    if (!IsInEditMode())
    {
        if (code == KEY_ESCAPE)
        {
            PostMessage(GetParent(), new KeyValues("KeyCodeTyped", "code", code));
            return;
        }

        if (code == KEY_ENTER)
        {
            PostActionSignal(new KeyValues("Command", "command", "addselected"));
            return;
        }
    }

    BaseClass::OnKeyCodeTyped(code);
}

CDialogAddServer::CDialogAddServer(vgui2::Panel *parent) : Frame(parent, "DialogAddServer")
{
    SetDeleteSelfOnClose(true);

    SetTitle("#ServerBrowser_AddServersTitle", true);
    SetSizeable(false);

    m_pTabPanel = new PropertySheet(this, "GameTabs");
    m_pTabPanel->SetTabWidth(72);

    m_pDiscoveredGames = new CAddServerGameList(this, "Servers");

    m_pDiscoveredGames->AddColumnHeader(0, "Password", "#ServerBrowser_Password", 16, ListPanel::COLUMN_FIXEDSIZE | ListPanel::COLUMN_IMAGE);
    m_pDiscoveredGames->AddColumnHeader(1, "Bots", "#ServerBrowser_Bots", 16, ListPanel::COLUMN_FIXEDSIZE | ListPanel::COLUMN_HIDDEN);
    m_pDiscoveredGames->AddColumnHeader(2, "Name", "#ServerBrowser_Servers", 30, ListPanel::COLUMN_RESIZEWITHWINDOW | ListPanel::COLUMN_UNHIDABLE);
    m_pDiscoveredGames->AddColumnHeader(3, "GameDesc", "#ServerBrowser_Game", 112, 112, 300, 0);
    m_pDiscoveredGames->AddColumnHeader(4, "Players", "#ServerBrowser_Players", 55, ListPanel::COLUMN_FIXEDSIZE);
    m_pDiscoveredGames->AddColumnHeader(5, "Map", "#ServerBrowser_Map", 75, 75, 300, 0);
    m_pDiscoveredGames->AddColumnHeader(6, "Ping", "#ServerBrowser_Latency", 55, ListPanel::COLUMN_FIXEDSIZE);

    m_pDiscoveredGames->SetColumnHeaderTooltip(0, "#ServerBrowser_PasswordColumn_Tooltip");
    m_pDiscoveredGames->SetColumnHeaderTooltip(1, "#ServerBrowser_BotColumn_Tooltip");

    m_pDiscoveredGames->SetSortFunc(0, PasswordCompare);
    m_pDiscoveredGames->SetSortFunc(1, BotsCompare);
    m_pDiscoveredGames->SetSortFunc(2, ServerNameCompare);
    m_pDiscoveredGames->SetSortFunc(3, GameCompare);
    m_pDiscoveredGames->SetSortFunc(4, PlayersCompare);
    m_pDiscoveredGames->SetSortFunc(5, MapCompare);
    m_pDiscoveredGames->SetSortFunc(6, PingCompare);

    m_pDiscoveredGames->SetSortColumn(6);

    m_pTextEntry = new vgui2::TextEntry(this, "ServerNameText");
    m_pTextEntry->AddActionSignalTarget(this);

    m_pTestServersButton = new vgui2::Button(this, "TestServersButton", "");
    m_pAddServerButton = new vgui2::Button(this, "OKButton", "");
    m_pAddSelectedServerButton = new vgui2::Button(this, "SelectedOKButton", "", this, "addselected");
    m_pTabPanel->AddPage(m_pDiscoveredGames, "#ServerBrowser_Servers");

    LoadControlSettings("Servers/DialogAddServer.res");

    m_pAddServerButton->SetEnabled(false);
    m_pTestServersButton->SetEnabled(false);
    m_pAddSelectedServerButton->SetEnabled(false);
    m_pAddSelectedServerButton->SetVisible(false);
    m_pTabPanel->SetVisible(false);

    m_pTextEntry->RequestFocus();

    int x, y;
    m_pTabPanel->GetPos(x, y);
    m_OriginalHeight = m_pTabPanel->GetTall() + y + 50;
    SetTall(y);

    ivgui()->AddTickSignal(GetVPanel());
}

CDialogAddServer::~CDialogAddServer()
{
    for (auto request : m_ServerQueries)
        SteamMatchmakingServers()->CancelServerQuery(request);
}

void CDialogAddServer::OnTextChanged()
{
    bool bAnyText = (m_pTextEntry->GetTextLength() > 0);

    m_pAddServerButton->SetEnabled(bAnyText);
    m_pTestServersButton->SetEnabled(bAnyText);
}

void CDialogAddServer::OnCommand(const char *command)
{
    if (!Q_stricmp(command, "OK"))
    {
        OnOK();
    }
    else if (!Q_stricmp(command, "TestServers"))
    {
        SetTall(m_OriginalHeight);

        m_pTabPanel->SetVisible(true);
        m_pAddSelectedServerButton->SetVisible(true);

        TestServers();
    }
    else if (!Q_stricmp(command, "addselected"))
    {
        if (m_pDiscoveredGames->GetSelectedItemsCount())
        {
            while (m_pDiscoveredGames->GetSelectedItemsCount() > 0)
            {
                int itemID = m_pDiscoveredGames->GetSelectedItem(0);
                int serverID = m_pDiscoveredGames->GetItemUserData(itemID);

                m_pDiscoveredGames->RemoveItem(itemID);
                ServerBrowserDialog().AddServerToFavorites(m_DiscoveredServers[serverID]);
            }

            m_pDiscoveredGames->SetEmptyListText("");
        }
    }
    else
        BaseClass::OnCommand(command);
}

void CDialogAddServer::OnOK()
{
    const char *address = GetControlString("ServerNameText", "");

    uint32_t ip{}; uint16_t port{};

    if (address != nullptr && address[0] != '\0')
        nitro_utils::ParseAddress(address, ip, port, true);

    netadr_t netaddr(ip, port);
    if (netaddr.IsValid())
    {
        ServerBrowserDialog().AddServerToFavorites(netaddr.GetIPHostByteOrder(), netaddr.GetPortHostByteOrder());
        PostMessage(this, new KeyValues("Close"));
    }
    else
    {
        MessageBox *dlg = new MessageBox("#ServerBrowser_AddServerErrorTitle", "#ServerBrowser_AddServerError");
        dlg->DoModal();
    }
}

void CDialogAddServer::OnKeyCodeTyped(KeyCode code)
{
    if (code == vgui2::KEY_ESCAPE)
    {
        Close();
    }
    else if (code == KEY_ENTER)
    {
        OnOK();
    }
    else
    {
        BaseClass::OnKeyCodeTyped(code);
    }
}

void CDialogAddServer::TestServers()
{
    CUtlVector<servernetadr_t> vecAdress;

    for (auto request : m_ServerQueries)
        SteamMatchmakingServers()->CancelServerQuery(request);

    m_DiscoveredServers.RemoveAll();
    m_pDiscoveredGames->RemoveAll();
    m_pDiscoveredGames->SetEmptyListText("");

    const char *address = GetControlString("ServerNameText", "");

    uint32_t ip = 0;
    uint16_t port = 0;
    nitro_utils::ParseAddress(address, ip, port, true);

    servernetadr_t servernetadr{};
    servernetadr.Init(ip, port, port);

    CUtlVector<uint16> portsToTry;
    CServerBrowserDialog::GetMostCommonQueryPorts(portsToTry);

    for (int i = 0; i < portsToTry.Count(); i++)
    {
        servernetadr_t newAddr{};
        newAddr.Init(ip, portsToTry[i], portsToTry[i]);
        vecAdress.AddToTail(newAddr);
    }

    m_pTabPanel->RemoveAllPages();

    wchar_t wstr[512];

    if (address[0] == 0)
    {
        Q_wcsncpy(wstr, g_pVGuiLocalize->Find("#ServerBrowser_ServersRespondingLocal"), sizeof(wstr));
    }
    else
    {
        wchar_t waddress[512];
        Q_UTF8ToUnicode(address, waddress, sizeof(waddress));
        g_pVGuiLocalize->ConstructString(wstr, sizeof(wstr), g_pVGuiLocalize->Find("#ServerBrowser_ServersResponding"), 1, waddress);
    }

    char str[512];
    Q_UnicodeToUTF8(wstr, str, sizeof(str));
    m_pTabPanel->AddPage(m_pDiscoveredGames, str);
    m_pTabPanel->InvalidateLayout();

    for (int i = 0; i < vecAdress.Size(); i++)
        m_ServerQueries.push_back(SteamMatchmakingServers()->PingServer(vecAdress[i].GetIP(), vecAdress[i].GetConnectionPort(), this));
}

void CDialogAddServer::ServerResponded(gameserveritem_t &server)
{
    KeyValues *kv = new KeyValues("Server");

    kv->SetString("name", server.GetName().c_str());
    kv->SetString("map", server.m_szMap);
    kv->SetString("GameDir", server.m_szGameDir);
    kv->SetString("GameDesc", server.m_szGameDescription);
    kv->SetInt("password", server.m_bPassword ? 1 : 0);
    kv->SetInt("bots", server.m_nBotPlayers ? 2 : 0);

    char buf[32];
    Q_snprintf(buf, sizeof(buf), "%d / %d", server.m_nPlayers, server.m_nMaxPlayers);
    kv->SetString("Players", buf);

    if (server.m_nPing < 1200)
        kv->SetInt("Ping", server.m_nPing);
    else
        kv->SetString("Ping", "");

    int iServer = m_DiscoveredServers.AddToTail(server);
    int iListID = m_pDiscoveredGames->AddItem(kv, iServer, false, false);

    if (m_pDiscoveredGames->GetItemCount() == 1)
        m_pDiscoveredGames->AddSelectedItem(iListID);

    kv->deleteThis();

    m_pDiscoveredGames->InvalidateLayout();
}

void CDialogAddServer::ServerFailedToRespond()
{
    //m_pDiscoveredGames->SetEmptyListText("#ServerBrowser_ServerNotResponding");
}

void CDialogAddServer::ApplySchemeSettings(IScheme *pScheme)
{
    BaseClass::ApplySchemeSettings(pScheme);

    ImageList *imageList = new ImageList(false);
    imageList->AddImage(scheme()->GetImage("servers/icon_password", false));
    imageList->AddImage(scheme()->GetImage("servers/icon_bots", false));
    imageList->AddImage(scheme()->GetImage("servers/icon_robotron", false));
    imageList->AddImage(scheme()->GetImage("servers/icon_secure_deny", false));

    int passwordColumnImage = imageList->AddImage(scheme()->GetImage("servers/icon_password_column", false));
    int botColumnImage = imageList->AddImage(scheme()->GetImage("servers/icon_bots_column", false));
    int secureColumnImage = imageList->AddImage(scheme()->GetImage("servers/icon_robotron_column", false));

    vgui2::HFont hFont = pScheme->GetFont("ListSmall", IsProportional());

    if (!hFont)
        hFont = pScheme->GetFont("DefaultSmall", IsProportional());

    m_pDiscoveredGames->SetFont(hFont);
    m_pDiscoveredGames->SetImageList(imageList, true);
    m_pDiscoveredGames->SetColumnHeaderImage(0, passwordColumnImage);
    m_pDiscoveredGames->SetColumnHeaderImage(1, botColumnImage);
}

void CDialogAddServer::OnTick(void)
{
    BaseClass::OnTick();
}

void CDialogAddServer::OnItemSelected(void)
{
    int nSelectedItem = m_pDiscoveredGames->GetSelectedItem(0);

    if (nSelectedItem != -1)
        m_pAddSelectedServerButton->SetEnabled(true);
    else
        m_pAddSelectedServerButton->SetEnabled(false);
}