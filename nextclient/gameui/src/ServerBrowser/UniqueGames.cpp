#include "GameUi.h"
#include "UniqueGames.h"
#include "ServerContextMenu.h"
#include "ServerListCompare.h"
#include "ServerBrowserDialog.h"

#include <cstdio>
#include <ctime>

#include <vgui/ILocalize.h>
#include <vgui/IInput.h>
#include <vgui/IInputInternal.h>
#include <vgui/ISchemeNext.h>
#include <vgui/IVGui.h>
#include <vgui/MouseCode.h>
#include <KeyValues.h>

#include <vgui_controls/Menu.h>
#include <vgui_controls/ComboBox.h>

#include <steam/steam_api.h>

using namespace vgui2;

const std::vector<GameListColumnType> CUniqueGames::PageColumns
{
    GameListColumnType::Password,
    GameListColumnType::Bots,
    GameListColumnType::Secure,
    GameListColumnType::ServerName,
    GameListColumnType::ServerDesc,
    GameListColumnType::GameDesc,
    GameListColumnType::Players,
    GameListColumnType::Map,
    GameListColumnType::Ping,
    GameListColumnType::Ip,
};

CUniqueGames::CUniqueGames(vgui2::Panel *parent, bool bAutoRefresh, const char *panelName) :
    CBaseGamesPage(parent, panelName, nullptr, PageColumns),
    auto_refresh_(bAutoRefresh)
{
    m_pLocationFilter->DeleteAllItems();
}

CUniqueGames::~CUniqueGames()
{
    CancelSteamQueries();
}

void CUniqueGames::OnPageShow()
{
    if (!ServerBrowserDialog().IsVisible())
        return;

    if (m_pGameList->GetItemCount() == 0)
        GetNewServerList();
    else if (auto_refresh_)
        StartRefresh();
}

void CUniqueGames::OnPageHide()
{
    StopRefresh(CancelQueryReason::PageClosed);
    CancelSteamQueries();
}

void CUniqueGames::ServerResponded(serveritem_t &server)
{
    BaseClass::ServerResponded(server);

    server_refresh_count_++;

    if (m_pGameList->GetItemCount() > 0 && list_refreshing_)
    {
        wchar_t unicode[128], unicodePercent[6];
        char tempPercent[6];
        int percentDone = (server_refresh_count_ * 100) / m_pGameList->GetItemCount();

        if (percentDone < 0)
            percentDone = 0;
        else if (percentDone > 99)
            percentDone = 100;

        itoa(percentDone, tempPercent, 10);
        g_pVGuiLocalize->ConvertANSIToUnicode(tempPercent, unicodePercent, sizeof(unicodePercent));
        g_pVGuiLocalize->ConstructString(unicode, sizeof(unicode), g_pVGuiLocalize->Find("#ServerBrowser_RefreshingPercentDone"), 1, unicodePercent);
        ServerBrowserDialog().UpdateStatusText(unicode);
    }
}

void CUniqueGames::ServerFailedToRespond(serveritem_t &server)
{
    if (m_pGameList->IsValidItemID(server.listEntryID))
        m_pGameList->SetItemVisible(server.listEntryID, false);

    UpdateRefreshStatusText();

    server_refresh_count_++;
}

void CUniqueGames::RefreshComplete()
{
    SetRefreshing(false);
    UpdateFilterSettings();

    list_refreshing_ = false;
    server_refresh_count_ = 0;
    last_sort_ = engine->GetClientTime();

    if (m_pGameList->GetItemCount() == 0)
        m_pGameList->SetEmptyListText("#ServerBrowser_NoInternetGamesResponded");

    UpdateRefreshStatusText();
}

void CUniqueGames::GetNewServerList()
{
    if (!IsVisible())
        return;

    StopRefresh(CancelQueryReason::NewQuery);

    m_pGameList->DeleteAllItems();
    m_pGameList->SetSortColumnEx(m_ColumnsMap[GameListColumnType::Ping], -1, true);

    m_Servers.Clear();
    m_Servers.RequestUnique(GetFilter(), GetFilterCount());

    UpdateRefreshStatusText();
    SetRefreshing(true);
    ServerBrowserDialog().UpdateStatusText("#ServerBrowser_GettingNewServerList");
}

// void CUniqueGames::RulesResponded(uint32 ip, uint16 port, const char *pchRule, const char *pchValue)
// {
//     if (std::strcmp(pchRule, "mod_description") == 0)
//     {
//         auto it = std::find_if(m_Servers.begin(), m_Servers.end(), [ip, port](const auto& serv_item) {
//             return serv_item.second.gs->m_NetAdr.GetIP() == ip
//                    && serv_item.second.gs->m_NetAdr.GetConnectionPort() == port;
//         });
//
//         if (it != m_Servers.end())
//             UpdateModDescription(it->second, pchValue);
//     }
// }
//
// void CUniqueGames::RulesFailedToRespond(uint32 ip, uint16 port)
// {
//
// }
//
// void CUniqueGames::RulesRefreshComplete(uint32 ip, uint16 port)
// {
//
// }

bool CUniqueGames::SupportsItem(CBaseGamesPage::InterfaceItem item)
{
    switch (item)
    {
        case CBaseGamesPage::InterfaceItem::Filters:
        case CBaseGamesPage::InterfaceItem::GetNewList:
            return true;

        default:
            return false;
    }
}

void CUniqueGames::StartRefresh()
{
    StopRefresh(CancelQueryReason::NewQuery);

    SetRefreshing(true);
    ServerBrowserDialog().UpdateStatusText("#ServerBrowser_GettingNewServerList");

    list_refreshing_ = true;
    server_refresh_count_ = 0;

    if (m_pGameList->GetItemCount() == 0)
        GetNewServerList();
    else
        m_Servers.StartRefresh();
}

void CUniqueGames::StopRefresh(CancelQueryReason reason)
{
    list_refreshing_ = false;
    server_refresh_count_ = 0;

    for (HServerQuery query : rule_queries_)
        SteamMatchmakingServers()->CancelServerQuery(query);

    CBaseGamesPage::StopRefresh(reason);
}

void CUniqueGames::OnViewGameInfo()
{
    if (!m_pGameList->GetSelectedItemsCount())
        return;

    int serverID = m_pGameList->GetItemUserData(m_pGameList->GetSelectedItem(0));

    ServerBrowserDialog().OpenGameInfoDialog(this, serverID);
}

GuiConnectionSource CUniqueGames::GetConnectionSource()
{
    return GuiConnectionSource::ServersUnique;
}

void CUniqueGames::OnOpenContextMenu(int itemID)
{
    if (!m_pGameList->GetSelectedItemsCount())
        return;

    int serverID = m_pGameList->GetItemData(m_pGameList->GetSelectedItem(0))->userData;

    CServerContextMenu *menu = ServerBrowserDialog().GetContextMenu(m_pGameList);
    menu->ShowMenu(this, serverID, true, true, true, true);
}

void CUniqueGames::UpdateModDescription(const serveritem_t& serveritem, const char* description)
{
    KeyValues *kv = m_pGameList->GetItem(serveritem.listEntryID);
    if (kv != nullptr)
    {
        kv->SetString("ServerDesc", description);
        m_pGameList->ApplyItemChanges(serveritem.listEntryID);
        m_pGameList->InvalidateLayout();
        m_pGameList->Repaint();
    }
}

void CUniqueGames::CancelSteamQueries()
{
    for (HServerQuery query : rule_queries_)
        SteamMatchmakingServers()->CancelServerQuery(query);

    rule_queries_.clear();
}

void CUniqueGames::OnRefreshServer(int serverID)
{
    if (m_Servers.IsRefreshing())
        return;

    m_Servers.StartRefreshServer(serverID);
}

int CUniqueGames::GetRegionCodeToFilter()
{
    KeyValues *kv = m_pLocationFilter->GetActiveItemUserData();

    if (kv)
        return kv->GetInt("code");
    else
        return 255;
}
