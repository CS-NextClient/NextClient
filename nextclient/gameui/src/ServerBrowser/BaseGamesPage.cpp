#include "BaseGamesPage.h"
#include "ServerListCompare.h"
#include "ServerBrowserDialog.h"

#include <vgui/ILocalize.h>
#include <vgui/ISchemeNext.h>
#include <vgui/IVGui.h>
#include <vgui/KeyCode.h>
#include <vgui/IPanel.h>
#include <KeyValues.h>

#include <vgui_controls/CheckButton.h>
#include <vgui_controls/ComboBox.h>
#include <vgui_controls/ImageList.h>
#include <vgui_controls/ImagePanel.h>
#include <vgui_controls/ListPanel.h>
#include <vgui_controls/Tooltip.h>

#include <cstdio>
#include <format>
#include <iomanip>
#include <sstream>
#include <utility>

#include <Windows.h>

using namespace vgui2;

const std::vector<GameListColumnType> CBaseGamesPage::DefaultColumns
{
    GameListColumnType::Password,
    GameListColumnType::Bots,
    GameListColumnType::Secure,
    GameListColumnType::ServerName,
    GameListColumnType::GameDesc,
    GameListColumnType::Players,
    GameListColumnType::Map,
    GameListColumnType::Ping,
    GameListColumnType::Ip,
};

CGameListPanel::CGameListPanel(CBaseGamesPage *pOuter, const char *pName) : BaseClass(pOuter, pName)
{
    m_pOuter = pOuter;

    SetIgnoreDoubleClick(false);
}

void CGameListPanel::OnKeyCodeTyped(vgui2::KeyCode code)
{
    if (code == KEY_ENTER && m_pOuter->OnGameListEnterPressed())
        return;

    BaseClass::OnKeyCodeTyped(code);
}

CBaseGamesPage *CGameListPanel::GetOuterGamesPage() const
{
    return m_pOuter;
}

CBaseGamesPage::CBaseGamesPage(vgui2::Panel *parent, const char *name, const char *pCustomResFilename, const std::vector<GameListColumnType>& columns) :
    PropertyPage(parent, name),
    m_pCustomResFilename(pCustomResFilename),
    m_Servers(this)
{
    SetSize(624, 278);

    m_szGameFilter[0] = 0;
    m_szMapFilter[0] = 0;
    m_iPingFilter = 0;
    m_bFilterNoFullServers = false;
    m_bFilterNoEmptyServers = false;
    m_bFilterNoPasswordedServers = false;
    m_hFont = NULL;

    wchar_t *all = g_pVGuiLocalize->Find("ServerBrowser_All");
    Q_UnicodeToUTF8(all, m_szComboAllText, sizeof(m_szComboAllText));

    m_pConnect = new Button(this, "ConnectButton", "#ServerBrowser_Connect");
    m_pConnect->SetEnabled(false);
    m_pRefreshAll = new Button(this, "RefreshButton", "#ServerBrowser_Refresh");
    m_pRefreshQuick = new Button(this, "RefreshQuickButton", "#ServerBrowser_RefreshQuick");
    m_pAddServer = new Button(this, "AddServerButton", "#ServerBrowser_AddServer");
    m_pAddCurrentServer = new Button(this, "AddCurrentServerButton", "#ServerBrowser_AddCurrentServer");
    m_pGameList = new CGameListPanel(this, "gamelist");
    m_pGameList->SetAllowUserModificationOfColumns(true);

    m_pAddToFavoritesButton = new vgui2::Button(this, "AddToFavoritesButton", "");
    m_pAddToFavoritesButton->SetEnabled(false);
    m_pAddToFavoritesButton->SetVisible(false);

    // TODO tooltips has bug: always shown behind all panels, fix this and uncomment tooltips code below

    int i = 0;
    for (const auto& type : columns)
    {
        switch (type)
        {
        case GameListColumnType::Password:
            m_pGameList->AddColumnHeader(i, "Password", "#ServerBrowser_Password", 16, ListPanel::COLUMN_FIXEDSIZE | ListPanel::COLUMN_IMAGE);
            m_pGameList->SetSortFunc(i, ServerIdCompare);
            // m_pGameList->SetColumnHeaderTooltip(i, "#ServerBrowser_PasswordColumn_Tooltip");
            break;

        case GameListColumnType::Bots:
            m_pGameList->AddColumnHeader(i, "Bots", "#ServerBrowser_Bots", 17, ListPanel::COLUMN_FIXEDSIZE | ListPanel::COLUMN_HIDDEN);
            m_pGameList->SetSortFunc(i, BotsCompare);
            //m_pGameList->SetColumnHeaderTooltip(i, "#ServerBrowser_BotColumn_Tooltip");
            break;
        case GameListColumnType::Secure:
            m_pGameList->AddColumnHeader(i, "Secure", "#ServerBrowser_Secure", 16, ListPanel::COLUMN_FIXEDSIZE | ListPanel::COLUMN_HIDDEN);
            m_pGameList->SetSortFunc(i, SecureCompare);
            //m_pGameList->SetColumnHeaderTooltip(i, "#ServerBrowser_SecureColumn_Tooltip");
            break;
        case GameListColumnType::ServerName:
            m_pGameList->AddColumnHeader(i, "Name", "#ServerBrowser_Servers", 50, ListPanel::COLUMN_RESIZEWITHWINDOW | ListPanel::COLUMN_UNHIDABLE);
            m_pGameList->SetSortFunc(i, ServerNameCompare);
            break;
        case GameListColumnType::ServerDesc:
            m_pGameList->AddColumnHeader(i, "ServerDesc", "#ServerBrowser_ServerDesc", 100, ListPanel::COLUMN_RESIZEWITHWINDOW);
            break;
        case GameListColumnType::GameDesc:
            m_pGameList->AddColumnHeader(i, "GameDesc", "#ServerBrowser_Game", 112, 112, 300);
            m_pGameList->SetSortFunc(i, GameCompare);
            break;
        case GameListColumnType::Players:
            m_pGameList->AddColumnHeader(i, "Players", "#ServerBrowser_Players", 55, 55, 300);
            m_pGameList->SetSortFunc(i, PlayersCompare);
            break;
        case GameListColumnType::Map:
            m_pGameList->AddColumnHeader(i, "Map", "#ServerBrowser_Map", 90, 90, 10000);
            m_pGameList->SetSortFunc(i, MapCompare);
            break;
        case GameListColumnType::Ping:
            m_pGameList->AddColumnHeader(i, "Ping", "#ServerBrowser_Latency", 55, 55, 10000);
            m_pGameList->SetSortFunc(i, PingCompare);
            break;
        case GameListColumnType::Ip:
            m_pGameList->AddColumnHeader(i, "Address", "#ServerBrowser_IPAddress", 95, 95, 10000, ListPanel::COLUMN_HIDDEN);
            break;
        case GameListColumnType::LastPlayed:
            m_pGameList->AddColumnHeader(i, "LastPlayed", "#ServerBrowser_LastPlayed", 95, 95, 120);
            m_pGameList->SetSortFunc(i, LastPlayedCompare);
            break;
        }

        if (type == GameListColumnType::LastPlayed)
            m_pGameList->SetSortColumnEx(i, -1, false);
        else if (type == GameListColumnType::Password)
            m_pGameList->SetSortColumn(i);

        m_ColumnsMap.emplace(type, i);

        i++;
    }

    ivgui()->AddTickSignal(GetVPanel());

    CreateFilters();
    LoadFilterSettings();
}

CBaseGamesPage::~CBaseGamesPage()
{
}

int CBaseGamesPage::GetInvalidServerListID()
{
    return m_pGameList->InvalidItemID();
}

void CBaseGamesPage::PerformLayout()
{
    BaseClass::PerformLayout();

    if (m_pGameList->GetSelectedItemsCount() < 1)
        m_pConnect->SetEnabled(false);
    else
        m_pConnect->SetEnabled(true);

    if (SupportsItem(InterfaceItem::GetNewList))
    {
        m_pRefreshQuick->SetVisible(true);
        m_pRefreshAll->SetText("#ServerBrowser_RefreshAll");
    }
    else
    {
        m_pRefreshQuick->SetVisible(false);
        m_pRefreshAll->SetText("#ServerBrowser_Refresh");
    }

    if (SupportsItem(InterfaceItem::AddServer))
    {
        m_pFilterString->SetWide(90);
        m_pAddServer->SetVisible(true);
    }
    else
    {
        m_pAddServer->SetVisible(false);
    }

    if (SupportsItem(InterfaceItem::AddCurrentServer))
        m_pAddCurrentServer->SetVisible(true);
    else
        m_pAddCurrentServer->SetVisible(false);

    if (IsRefreshing())
        m_pRefreshAll->SetText("#ServerBrowser_StopRefreshingList");

    if (m_pGameList->GetItemCount() > 0)
        m_pRefreshQuick->SetEnabled(true);
    else
        m_pRefreshQuick->SetEnabled(false);

    Repaint();
}

bool CBaseGamesPage::IsActivated()
{
    if (!ServerBrowserDialog().IsVisible() || !ipanel()->Render_GetPopupVisible(ServerBrowserDialog().GetVPanel()))
        return false;

    if (ServerBrowserDialog().GetActivePage() != this)
        return false;

    return true;
}

void CBaseGamesPage::OnTick()
{
    BaseClass::OnTick();
}

void CBaseGamesPage::ApplySchemeSettings(IScheme *pScheme)
{
    BaseClass::ApplySchemeSettings(pScheme);

    auto *imageList = new ImageList(false);
    m_iPasswordImage = imageList->AddImage(scheme()->GetImage("servers/icon_password", false));
    m_iBotImage = imageList->AddImage(scheme()->GetImage("servers/icon_bots", false));
    m_iSecureImage = imageList->AddImage(scheme()->GetImage("servers/icon_robotron", false));
    imageList->AddImage(scheme()->GetImage("servers/icon_secure_deny", false));

    int column_password = imageList->AddImage(scheme()->GetImage("servers/icon_password_column", false));
    int column_bots = imageList->AddImage(scheme()->GetImage("servers/icon_bots_column", false));
    int secure_column = imageList->AddImage(scheme()->GetImage("servers/icon_robotron_column", false));

    m_pGameList->SetImageList(imageList, true);
    m_hFont = pScheme->GetFont("ListSmall", IsProportional());

    if (!m_hFont)
        m_hFont = pScheme->GetFont("DefaultSmall", IsProportional());

    m_pGameList->SetFont(m_hFont);

    if (m_ColumnsMap.contains(GameListColumnType::Password))
        m_pGameList->SetColumnHeaderImage(m_ColumnsMap[GameListColumnType::Password], column_password);

    if (m_ColumnsMap.contains(GameListColumnType::Bots))
        m_pGameList->SetColumnHeaderImage(m_ColumnsMap[GameListColumnType::Bots], column_bots);

    if (m_ColumnsMap.contains(GameListColumnType::Secure))
        m_pGameList->SetColumnHeaderImage(m_ColumnsMap[GameListColumnType::Secure], secure_column);

    OnButtonToggled(m_pFilter, false);
}

void CBaseGamesPage::CreateFilters()
{
    m_bFiltersVisible = false;
    ClearMasterFilter();
    m_pFilter = new ToggleButton(this, "Filter", "#ServerBrowser_Filter");
    m_pFilterString = new Label(this, "FilterString", "");
    m_pGameFilter = new ComboBox(this, "GameFilter", 6, false);

    auto *pkv = new KeyValues("mod", "gamedir", "");
    m_pGameFilter->AddItem("#ServerBrowser_All", pkv);
    pkv->deleteThis();

    m_pLocationFilter = new ComboBox(this, "LocationFilter", 6, false);
    m_pLocationFilter->AddItem("", NULL);

    m_pMapFilter = new TextEntry(this, "MapFilter");
    m_pPingFilter = new ComboBox(this, "PingFilter", 6, false);
    m_pPingFilter->AddItem("#ServerBrowser_All", NULL);
    m_pPingFilter->AddItem("#ServerBrowser_LessThan50", NULL);
    m_pPingFilter->AddItem("#ServerBrowser_LessThan100", NULL);
    m_pPingFilter->AddItem("#ServerBrowser_LessThan150", NULL);
    m_pPingFilter->AddItem("#ServerBrowser_LessThan250", NULL);
    m_pPingFilter->AddItem("#ServerBrowser_LessThan350", NULL);
    m_pPingFilter->AddItem("#ServerBrowser_LessThan600", NULL);

    m_pSecureFilter = new ComboBox(this, "SecureFilter", 3, false);
    m_pSecureFilter->AddItem("#ServerBrowser_All", NULL);
    m_pSecureFilter->AddItem("#ServerBrowser_SecureOnly", NULL);
    m_pSecureFilter->AddItem("#ServerBrowser_InsecureOnly", NULL);

    m_pNoEmptyServersFilterCheck = new CheckButton(this, "ServerEmptyFilterCheck", "");
    m_pNoFullServersFilterCheck = new CheckButton(this, "ServerFullFilterCheck", "");
    m_pNoPasswordFilterCheck = new CheckButton(this, "NoPasswordFilterCheck", "");
    //m_pValidSteamAccountFilterCheck = new CheckButton(this, "ValidSteamAccountFilterCheck", "");
}

void CBaseGamesPage::LoadFilterSettings()
{
    KeyValues *filter = ServerBrowserDialog().GetFilterSaveData(GetName());

    if (ServerBrowserDialog().GetActiveModName())
        Q_strncpy(m_szGameFilter, ServerBrowserDialog().GetActiveModName(), sizeof(m_szGameFilter));
    else
        Q_strncpy(m_szGameFilter, filter->GetString("game"), sizeof(m_szGameFilter));

    Q_strncpy(m_szMapFilter, filter->GetString("map"), sizeof(m_szMapFilter));
    m_iPingFilter = filter->GetInt("ping");
    m_bFilterNoFullServers = filter->GetInt("NoFull");
    m_bFilterNoEmptyServers = filter->GetInt("NoEmpty");
    m_bFilterNoPasswordedServers = filter->GetInt("NoPassword");
    m_iSelectedSecureFilterRow = std::clamp(filter->GetInt("Secure"), 0, m_pSecureFilter->GetItemCount());
    m_bFilterValidSteamAccount = filter->GetInt("ValidSteamAccount");

    UpdateGameFilter();

    m_pMapFilter->SetText(m_szMapFilter);
    m_pLocationFilter->ActivateItem(filter->GetInt("location"));

    if (m_iPingFilter)
    {
        char buf[32];
        Q_snprintf(buf, sizeof(buf), "< %d", m_iPingFilter);
        m_pPingFilter->SetText(buf);
    }

    m_pSecureFilter->ActivateItemByRow(m_iSelectedSecureFilterRow);
    m_pNoFullServersFilterCheck->SetSelected(m_bFilterNoFullServers);
    m_pNoEmptyServersFilterCheck->SetSelected(m_bFilterNoEmptyServers);
    m_pNoPasswordFilterCheck->SetSelected(m_bFilterNoPasswordedServers);
    //m_pValidSteamAccountFilterCheck->SetSelected(m_bFilterValidSteamAccount);

    OnLoadFilter(filter);
    UpdateFilterSettings();
}

void CBaseGamesPage::UpdateGameFilter()
{
    bool bFound = false;

    for (int i = 0; i < m_pGameFilter->GetItemCount(); i++)
    {
        KeyValues *kv = m_pGameFilter->GetItemUserData(i);
        const char *pchGameDir = kv->GetString("gamedir");

        if (!m_szGameFilter[0] || !Q_strncmp(pchGameDir, m_szGameFilter, Q_strlen(pchGameDir)))
        {
            if (i != m_pGameFilter->GetActiveItem())
                m_pGameFilter->ActivateItem(i);

            bFound = true;
            break;
        }
    }

    if (!bFound)
    {
        if (m_pGameFilter->GetActiveItem() != 0)
            m_pGameFilter->ActivateItem(0);
    }

    if (ServerBrowserDialog().GetActiveModName())
    {
        m_pGameFilter->SetEnabled(false);
        m_pGameFilter->SetText(ServerBrowserDialog().GetActiveGameName());
    }
}

void CBaseGamesPage::ServerResponded(serveritem_t &server)
{
    if (!CheckPrimaryFilters(server) || !CheckSecondaryFilters(server))
        return;

    KeyValues *kv;
    bool newItem = !m_pGameList->IsValidItemID(server.listEntryID) || m_pGameList->GetItemUserData(server.listEntryID) != server.serverID;

    if (newItem)
        kv = new KeyValues("Server");
    else
        kv = m_pGameList->GetItem(server.listEntryID);

    kv->SetString("name", server.gs.GetName().c_str());
    kv->SetString("map", server.gs.m_szMap);
    kv->SetString("GameDir", server.gs.m_szGameDir);
    kv->SetString("GameDesc", server.gs.m_szGameDescription);
    kv->SetInt("password", server.gs.m_bPassword ? 1 : 0);
    kv->SetString("secure", server.gs.m_bSecure ? std::format("!img:{}", m_iSecureImage).c_str() : "");
    kv->SetString("bots", server.gs.m_nBotPlayers > 0 ? std::to_string(server.gs.m_nBotPlayers).c_str() : "");
    kv->SetString("address", server.gs.m_NetAdr.GetConnectionAddressString().c_str());
    kv->SetInt("_ip", server.gs.m_NetAdr.GetIP());
    kv->SetInt("_port", server.gs.m_NetAdr.GetConnectionPort());
    kv->SetWString("LastPlayed", FormatUnixTime("%a %e %b %H:%M", server.gs.m_ulTimeLastPlayed).c_str());

    if (server.gs.m_bHadSuccessfulResponse)
    {
        char buf[256];
        sprintf(buf, "%d / %d", server.gs.m_nPlayers, server.gs.m_nMaxPlayers);
        kv->SetString("Players", buf);
    }
    else
        kv->SetString("Players", "-");


    if (!server.gs.m_bHadSuccessfulResponse)
        kv->SetString("Ping", "-");
    else if (server.gs.m_nPing < 1200)
        kv->SetInt("Ping", server.gs.m_nPing);
    else
        kv->SetString("Ping", "");

    if (newItem)
        server.listEntryID = m_pGameList->AddItem(kv, server.serverID, false, true);
    else
        m_pGameList->ApplyItemChanges(server.listEntryID);

    UpdateRefreshStatusText();

    m_pGameList->InvalidateLayout();
    m_pGameList->Repaint();
}

void CBaseGamesPage::OnButtonToggled(Panel *panel, int state)
{
    if (panel == m_pFilter)
    {
        int wide, tall;
        GetSize(wide, tall);
        SetSize(624, 278);

        if (m_pCustomResFilename)
        {
            m_bFiltersVisible = false;
        }
        else
        {
            if (m_pFilter->IsSelected())
                m_bFiltersVisible = true;
            else
                m_bFiltersVisible = false;
        }

        UpdateDerivedLayouts();

        m_pFilter->SetSelected(m_bFiltersVisible);

        UpdateGameFilter();

        if (m_hFont)
        {
            SETUP_PANEL(m_pGameList);
            m_pGameList->SetFont(m_hFont);
        }

        SetSize(wide, tall);
        InvalidateLayout();
    }
    else if (panel == m_pNoFullServersFilterCheck || panel == m_pNoEmptyServersFilterCheck || panel == m_pNoPasswordFilterCheck)
        OnTextChanged(panel, "");
}

void CBaseGamesPage::UpdateDerivedLayouts()
{
    char rgchControlSettings[MAX_PATH];

    if (m_pCustomResFilename)
    {
        Q_snprintf(rgchControlSettings, sizeof(rgchControlSettings), "%s", m_pCustomResFilename);
    }
    else
    {
        if (m_pFilter->IsSelected())
            Q_snprintf(rgchControlSettings, sizeof(rgchControlSettings), "Servers/%sPage_Filters.res", "InternetGames");
        else
            Q_snprintf(rgchControlSettings, sizeof(rgchControlSettings), "Servers/%sPage.res", "InternetGames");
    }

    LoadControlSettings(rgchControlSettings);
}

void CBaseGamesPage::OnTextChanged(Panel *panel, const char *text)
{
    if (!Q_stricmp(text, m_szComboAllText))
    {
        auto *box = dynamic_cast<ComboBox *>(panel);

        if (box)
        {
            box->SetText("");
            text = "";
        }
    }

    UpdateFilterSettings();
    ApplyFilters();

    if (m_bFiltersVisible && (panel == m_pGameFilter || panel == m_pLocationFilter))
    {
        StopRefresh(CancelQueryReason::FilterChanged);
        GetNewServerList();
    }
}

void CBaseGamesPage::ApplyFilters()
{
    ApplyGameFilters();
}

void CBaseGamesPage::ApplyGameFilters()
{
    for (auto& gameserver : m_Servers)
    {
        serveritem_t &server = gameserver.second;

        if (!CheckPrimaryFilters(server) || !CheckSecondaryFilters(server))
        {
            if (m_pGameList->IsValidItemID(server.listEntryID))
                m_pGameList->SetItemVisible(server.listEntryID, false);
        }
        else if (server.hadSuccessfulResponse)
        {
            if (!m_pGameList->IsValidItemID(server.listEntryID))
            {
                auto *kv = new KeyValues("Server");
                kv->SetString("name", server.gs.GetName().c_str());
                kv->SetString("map", server.gs.m_szMap);
                kv->SetString("GameDir", server.gs.m_szGameDir);
                kv->SetString("GameDesc", server.gs.m_szGameDescription);

                char buf[256];
                sprintf(buf, "%d / %d", server.gs.m_nPlayers, server.gs.m_nMaxPlayers);
                kv->SetString("Players", buf);
                kv->SetInt("Ping", server.gs.m_nPing);
                kv->SetInt("password", server.gs.m_bPassword ? 1 : 0);
                kv->SetInt("Secure", server.gs.m_bSecure ? 1 : 0);
                kv->SetInt("ValidSteamAccount", server.gs.m_steamID.IsValid() ? 1 : 0);

                server.listEntryID = m_pGameList->AddItem(kv, server.serverID, false, false);
            }

            m_pGameList->SetItemVisible(server.listEntryID, true);
        }
    }

    UpdateRefreshStatusText();

    m_pGameList->SortList();

    InvalidateLayout();
    Repaint();
}

void CBaseGamesPage::UpdateRefreshStatusText()
{
    if (m_pGameList->GetItemCount() > 1)
    {
        wchar_t header[256];
        wchar_t count[128];

        _itow(m_pGameList->GetItemCount(), count, 10);
        g_pVGuiLocalize->ConstructString(header, sizeof(header), g_pVGuiLocalize->Find("#ServerBrowser_ServersCount"), 1, count);

        if (m_ColumnsMap.contains(GameListColumnType::ServerName))
            m_pGameList->SetColumnHeaderText(m_ColumnsMap[GameListColumnType::ServerName], header);
    }
    else
    {
        if (m_ColumnsMap.contains(GameListColumnType::ServerName))
            m_pGameList->SetColumnHeaderText(m_ColumnsMap[GameListColumnType::ServerName], g_pVGuiLocalize->Find("#ServerBrowser_Servers"));
    }
}

void CBaseGamesPage::UpdateFilterSettings()
{
    if (ServerBrowserDialog().GetActiveModName())
    {
        Q_strncpy(m_szGameFilter, ServerBrowserDialog().GetActiveModName(), sizeof(m_szGameFilter));
        RecalculateMasterFilter();
        UpdateGameFilter();
    }
    else
    {
        KeyValues *data = m_pGameFilter->GetActiveItemUserData();

        if (data)
            Q_strncpy(m_szGameFilter, data->GetString("gamedir"), sizeof(m_szGameFilter));

        m_pGameFilter->SetEnabled(true);
    }

    Q_strlower(m_szGameFilter);
    m_pMapFilter->GetText(m_szMapFilter, sizeof(m_szMapFilter) - 1);
    Q_strlower(m_szMapFilter);

    m_iSelectedSecureFilterRow = m_pSecureFilter->GetRowByItemId(m_pSecureFilter->GetActiveItem());

    char buf[256];
    m_pPingFilter->GetText(buf, sizeof(buf));

    if (buf[0])
        m_iPingFilter = atoi(buf + 2);
    else
        m_iPingFilter = 0;

    m_bFilterNoFullServers = m_pNoFullServersFilterCheck->IsSelected();
    m_bFilterNoEmptyServers = m_pNoEmptyServersFilterCheck->IsSelected();
    m_bFilterNoPasswordedServers = m_pNoPasswordFilterCheck->IsSelected();
    //m_bFilterValidSteamAccount = m_pValidSteamAccountFilterCheck->IsSelected();

    buf[0] = 0;

    if (m_szGameFilter[0])
    {
        strcat(buf, "\\gamedir\\");
        strcat(buf, m_szGameFilter);
    }

    if (m_bFilterNoEmptyServers)
    {
        strcat(buf, "\\empty\\1");
    }

    if (m_bFilterNoFullServers)
    {
        strcat(buf, "\\full\\1");
    }

    int regCode = GetRegionCodeToFilter();

    if (regCode > 0)
    {
        char szRegCode[32];
        Q_snprintf(szRegCode, sizeof(szRegCode), "%i", regCode);
        strcat(buf, "\\region\\");
        strcat(buf, szRegCode);
    }

    KeyValues *filter = ServerBrowserDialog().GetFilterSaveData(GetName());

    if (!ServerBrowserDialog().GetActiveModName())
        filter->SetString("game", m_szGameFilter);

    filter->SetString("map", m_szMapFilter);
    filter->SetInt("ping", m_iPingFilter);

    if (m_pLocationFilter->GetItemCount() > 1)
        filter->SetInt("location", m_pLocationFilter->GetActiveItem());

    filter->SetInt("NoFull", m_bFilterNoFullServers);
    filter->SetInt("NoEmpty", m_bFilterNoEmptyServers);
    filter->SetInt("NoPassword", m_bFilterNoPasswordedServers);

    OnSaveFilter(filter);

    RecalculateMasterFilter();
}

void CBaseGamesPage::OnSaveFilter(KeyValues *filter)
{
}

void CBaseGamesPage::OnLoadFilter(KeyValues *filter)
{
}

void CBaseGamesPage::ClearMasterFilter()
{
    for (int i = 0; i < m_iMasterFilterCount; i++)
        delete m_MasterFilter[i];

    m_iMasterFilterCount = 0;
}

void CBaseGamesPage::RecalculateMasterFilter()
{
    ClearMasterFilter();

    if (m_pLocationFilter->GetActiveItem() > 0)
    {
        // TODO remove location filter completely?
    }

    if (m_iPingFilter)
    {
        // TODO special logic, no steam handling
    }

    if (m_bFilterNoFullServers)
    {
        m_MasterFilter[m_iMasterFilterCount++] = new MatchMakingKeyValuePair_t("notfull", "");
    }

    if (m_bFilterNoEmptyServers)
    {
        m_MasterFilter[m_iMasterFilterCount++] = new MatchMakingKeyValuePair_t("hasplayers", "");
    }

    if (m_bFilterNoPasswordedServers)
    {
        // TODO no password
        m_MasterFilter[m_iMasterFilterCount++] = new MatchMakingKeyValuePair_t("nor", "no_password");
        m_MasterFilter[m_iMasterFilterCount++] = new MatchMakingKeyValuePair_t("gametagsand", "no_password");
    }

    if (m_szMapFilter[0])
    {
        m_MasterFilter[m_iMasterFilterCount++] = new MatchMakingKeyValuePair_t("map", m_szMapFilter);
    }
}

bool CBaseGamesPage::CheckPrimaryFilters(serveritem_t &server)
{
    if (m_szGameFilter[0] && (server.gs.m_szGameDir[0] || server.gs.m_nPing) && Q_stricmp(m_szGameFilter, server.gs.m_szGameDir))
        return false;

    return true;
}

bool CBaseGamesPage::CheckSecondaryFilters(serveritem_t &server)
{
    if (m_bFilterNoEmptyServers && (server.gs.m_nPlayers - server.gs.m_nBotPlayers) < 1)
        return false;

    if (m_bFilterNoFullServers && server.gs.m_nPlayers >= server.gs.m_nMaxPlayers)
        return false;

    if (m_iPingFilter && server.gs.m_nPing > m_iPingFilter)
        return false;

    if (m_bFilterNoPasswordedServers && server.gs.m_bPassword)
        return false;

    // server.gs.m_steamID.IsValid() not working yet, always false
//    if (m_bFilterValidSteamAccount && !server.gs.m_steamID.IsValid())
//        return false;

    if (m_iSelectedSecureFilterRow && (server.gs.m_bSecure && m_iSelectedSecureFilterRow == kSecureFilterRowUnSecure) ||
                                      (!server.gs.m_bSecure && m_iSelectedSecureFilterRow == kSecureFilterRowSecure))
        return false;

    int count = Q_strlen(m_szMapFilter);

    if (count && Q_strnicmp(server.gs.m_szMap, m_szMapFilter, count))
        return false;

    return true;
}

MatchMakingKeyValuePair_t** CBaseGamesPage::GetFilter()
{
    return m_MasterFilter;
}

int CBaseGamesPage::GetFilterCount()
{
    return m_iMasterFilterCount;
}

void CBaseGamesPage::SetRefreshing(bool state)
{
    if (state)
    {
        ServerBrowserDialog().UpdateStatusText("#ServerBrowser_RefreshingServerList");

        m_pGameList->SetEmptyListText("");
        m_pRefreshAll->SetText("#ServerBrowser_StopRefreshingList");
        m_pRefreshAll->SetCommand("stoprefresh");
        m_pRefreshQuick->SetEnabled(false);
    }
    else
    {
        ServerBrowserDialog().UpdateStatusText("");

        if (SupportsItem(InterfaceItem::GetNewList))
            m_pRefreshAll->SetText("#ServerBrowser_RefreshAll");
        else
            m_pRefreshAll->SetText("#ServerBrowser_Refresh");

        m_pRefreshAll->SetCommand("GetNewList");

        if (m_pGameList->GetItemCount() > 0)
            m_pRefreshQuick->SetEnabled(true);
        else
            m_pRefreshQuick->SetEnabled(false);
    }
}

void CBaseGamesPage::OnCommand(const char *command)
{
    if (!Q_stricmp(command, "Connect"))
        OnBeginConnect();
    else if (!Q_stricmp(command, "stoprefresh"))
        StopRefresh(CancelQueryReason::UserCancellation);
    else if (!Q_stricmp(command, "refresh"))
        StartRefresh();
    else if (!Q_stricmp(command, "GetNewList"))
        GetNewServerList();
    else if (!Q_strcmp(command, "Close"))
        StopRefresh(CancelQueryReason::PageClosed);
    else
        BaseClass::OnCommand(command);
}

void CBaseGamesPage::OnItemSelected()
{
    if (m_pGameList->GetSelectedItemsCount() < 1)
        m_pConnect->SetEnabled(false);
    else
        m_pConnect->SetEnabled(true);
}

void CBaseGamesPage::OnKeyCodePressed(vgui2::KeyCode code)
{
    if (code == KEY_F5)
        StartRefresh();
    else
        BaseClass::OnKeyCodePressed(code);
}

bool CBaseGamesPage::OnGameListEnterPressed()
{
    return false;
}

bool CBaseGamesPage::IsRefreshing()
{
    return m_Servers.IsRefreshing();
}

serveritem_t &CBaseGamesPage::GetServer(int serverID)
{
    return m_Servers.GetServer(serverID);
}

int CBaseGamesPage::GetSelectedItemsCount()
{
    return m_pGameList->GetSelectedItemsCount();
}

void CBaseGamesPage::GetFilterState(FilterState* out)
{
    out->no_empty_servers = m_bFilterNoEmptyServers;
    out->no_full_servers = m_bFilterNoFullServers;
    out->ping_filter = m_iPingFilter;
    out->no_passworded_servers = m_bFilterNoPasswordedServers;
    out->secure_filter = (SecureFilter)m_iSelectedSecureFilterRow;
    out->sort_column = m_pGameList->GetSortColumn();
    V_strcpy_safe(out->map_filter, m_szMapFilter);
}

void CBaseGamesPage::OnAddToFavorites(int serverID)
{
    if (!m_pGameList->GetSelectedItemsCount())
        return;

    if (!m_Servers.IsServerExists(serverID))
        return;

    ServerBrowserDialog().AddServerToFavorites(m_Servers.GetServer(serverID).gs);
}

void CBaseGamesPage::OnRefreshServer(int serverID)
{
}

void CBaseGamesPage::StartRefresh()
{
    SetRefreshing(true);
}

void CBaseGamesPage::ClearServerList()
{
    m_pGameList->RemoveAll();
}

void CBaseGamesPage::StopRefresh(CancelQueryReason reason)
{
    m_Servers.StopRefresh(reason);

    RefreshComplete();
}

void CBaseGamesPage::OnBeginConnect()
{
    if (!m_pGameList->GetSelectedItemsCount())
        return;

    int serverID = m_pGameList->GetItemUserData(m_pGameList->GetSelectedItem(0));

    StopRefresh(CancelQueryReason::ConnectToServer);
    ServerBrowserDialog().JoinGame(this, serverID, GetConnectionSource());
}

void CBaseGamesPage::ConnectedToGame(int ip, int connPort)
{
    m_pAddCurrentServer->SetEnabled(true);
}

void CBaseGamesPage::DisconnectedFromGame()
{
    m_pAddCurrentServer->SetEnabled(false);
}

void CBaseGamesPage::OnViewGameInfo()
{
    if (!m_pGameList->GetSelectedItemsCount())
        return;

    int serverID = m_pGameList->GetItemUserData(m_pGameList->GetSelectedItem(0));

    ServerBrowserDialog().OpenGameInfoDialog(this, serverID);
}

void CBaseGamesPage::OnKeyCodeTyped(KeyCode code)
{
    if (code == vgui2::KEY_ENTER)
        OnBeginConnect();

    PropertyPage::OnKeyCodeTyped(code);
}

std::wstring CBaseGamesPage::FormatUnixTime(const char* format, uint32_t unix_time)
{
    std::time_t time = unix_time;
    std::tm* date = std::gmtime(&time);

    std::stringstream ss;
    ss.imbue(std::locale(setlocale(LC_TIME, nullptr)));
    ss << std::put_time(date, format);

    std::wstring wstr_time;
    int convert_result = MultiByteToWideChar(CP_ACP, 0, ss.str().c_str(), ss.str().size(), NULL, 0);
    if (convert_result != 0)
    {
        wstr_time.resize(convert_result);
        MultiByteToWideChar(CP_ACP, 0, ss.str().c_str(), ss.str().size(), wstr_time.data(), (int)wstr_time.size());
    }

    return wstr_time;
}