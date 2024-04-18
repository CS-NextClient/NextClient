#include "HistoryGames.h"

#include "ServerContextMenu.h"
#include "ServerListCompare.h"
#include "ServerBrowserDialog.h"
#include "InternetGames.h"
#include "FileSystem.h"
#include "utlbuffer.h"

#include <vgui/IScheme.h>
#include <vgui/ISystem.h>
#include <vgui/IVGui.h>
#include <KeyValues.h>

#include <vgui_controls/Button.h>
#include <vgui_controls/ListPanel.h>
#include <vgui_controls/ImagePanel.h>
#include <vgui_controls/MessageBox.h>

using namespace vgui2;

const std::vector<GameListColumnType> CHistoryGames::PageColumns
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
    GameListColumnType::LastPlayed,
};

CHistoryGames::CHistoryGames(vgui2::Panel *parent) :
    CBaseGamesPage(parent, "HistoryGames", nullptr, PageColumns)
{

}

CHistoryGames::~CHistoryGames()
{
}

void CHistoryGames::OnPageShow()
{
    if (!ServerBrowserDialog().IsVisible())
        return;

    GetNewServerList();
}

void CHistoryGames::OnPageHide()
{
    StopRefresh(CancelQueryReason::PageClosed);
}

bool CHistoryGames::SupportsItem(InterfaceItem item)
{
    switch (item)
    {
        case InterfaceItem::Filters: return true;
    }

    return false;
}

void CHistoryGames::StartRefresh()
{
    m_Servers.RequestHistory(nullptr, 0);

    CBaseGamesPage::StartRefresh();
}

void CHistoryGames::GetNewServerList()
{
    if (!IsVisible())
        return;

    StopRefresh(CancelQueryReason::NewQuery);

    m_pGameList->DeleteAllItems();

    m_Servers.Clear();
    m_Servers.RequestHistory(GetFilter(), GetFilterCount());

    UpdateRefreshStatusText();
    SetRefreshing(true);
}

void CHistoryGames::StopRefresh(CancelQueryReason reason)
{
    m_Servers.StopRefresh(reason);

    CBaseGamesPage::StopRefresh(reason);
}

void CHistoryGames::ListReceived(bool moreAvailable, int lastUnique)
{
    m_Servers.StartRefresh();
}

void CHistoryGames::OnBeginConnect()
{
    if (!m_pGameList->GetSelectedItemsCount())
        return;

    int serverID = m_pGameList->GetItemUserData(m_pGameList->GetSelectedItem(0));

    StopRefresh(CancelQueryReason::ConnectToServer);
    ServerBrowserDialog().JoinGame(this, serverID, GuiConnectionSource::ServersHistory);
}

void CHistoryGames::OnViewGameInfo()
{
    if (!m_pGameList->GetSelectedItemsCount())
        return;

    int serverID = m_pGameList->GetItemUserData(m_pGameList->GetSelectedItem(0));

    ServerBrowserDialog().OpenGameInfoDialog(this, serverID);
}

void CHistoryGames::ServerFailedToRespond(serveritem_t &server)
{

}

void CHistoryGames::RefreshComplete()
{
    SetRefreshing(false);
    UpdateFilterSettings();

    if (IsVisible())
        m_pGameList->SortList();

    m_pGameList->SetEmptyListText("#ServerBrowser_NoServersPlayed");
    m_pGameList->SortList();
}

GuiConnectionSource CHistoryGames::GetConnectionSource()
{
    return GuiConnectionSource::ServersHistory;
}

void CHistoryGames::OnOpenContextMenu(int itemID)
{
    CServerContextMenu *menu = ServerBrowserDialog().GetContextMenu(m_pGameList);

    if (m_pGameList->GetSelectedItemsCount())
    {
        int serverID = m_pGameList->GetItemUserData(m_pGameList->GetSelectedItem(0));

        menu->ShowMenu(this, serverID, true, true, true, true);
        menu->AddMenuItem("RemoveServer", "#ServerBrowser_RemoveServerFromHistory", new KeyValues("RemoveFromHistory"), this);
    }
    else
        menu->ShowMenu(this, (uint32)-1, false, false, false, false);
}

void CHistoryGames::OnRefreshServer(int serverID)
{
    if (m_Servers.IsRefreshing())
        return;

    m_Servers.StartRefreshServer(serverID);
}

void CHistoryGames::OnRemoveFromHistory()
{
    while (m_pGameList->GetSelectedItemsCount() > 0)
    {
        int itemID = m_pGameList->GetSelectedItem(0);
        unsigned int serverID = m_pGameList->GetItemData(itemID)->userData;

        if (serverID >= m_Servers.ServerCount())
            continue;

        serveritem_t &server = m_Servers.GetServer(serverID);

        uint32_t ip = server.gs.m_NetAdr.GetIP();
        uint32_t port = server.gs.m_NetAdr.GetConnectionPort();
        SteamMatchmaking()->RemoveFavoriteGame(SteamUtils()->GetAppID(), ip, port, port, k_unFavoriteFlagHistory);

        if (m_pGameList->IsValidItemID(server.listEntryID))
        {
            m_pGameList->RemoveItem(server.listEntryID);
            server.listEntryID = GetInvalidServerListID();
        }
    }

    UpdateRefreshStatusText();
    InvalidateLayout();
    Repaint();
}