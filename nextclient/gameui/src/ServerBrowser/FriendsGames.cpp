#include "GameUi.h"
#include "FriendsGames.h"
#include "ServerContextMenu.h"
#include "ServerListCompare.h"
#include "ServerBrowserDialog.h"

#include <vgui_controls/Menu.h>
#include <vgui_controls/ComboBox.h>

#include <steam/steam_api.h>

using namespace vgui2;

CFriendsGames::CFriendsGames(vgui2::Panel *parent, bool bAutoRefresh, const char *panelName) :
    CBaseGamesPage(parent, panelName, nullptr),
    auto_refresh_(bAutoRefresh)
{
    m_pLocationFilter->DeleteAllItems();
}

CFriendsGames::~CFriendsGames()
{
}

void CFriendsGames::OnPageShow()
{
    BaseClass::OnPageShow();

    if (m_pGameList->GetItemCount() == 0 || auto_refresh_)
        GetNewServerList();
}

void CFriendsGames::OnPageHide()
{
    BaseClass::OnPageHide();
}

void CFriendsGames::ServerFailedToRespond(serveritem_t &server)
{
    if (m_pGameList->IsValidItemID(server.listEntryID))
        m_pGameList->SetItemVisible(server.listEntryID, false);

    UpdateRefreshStatusText();
}

void CFriendsGames::RefreshComplete()
{
    SetRefreshing(false);
    UpdateFilterSettings();

    if (m_pGameList->GetItemCount() == 0)
        m_pGameList->SetEmptyListText("#ServerBrowser_NoFriendsServers");

    UpdateRefreshStatusText();
}

void CFriendsGames::GetNewServerList()
{
    if (!IsVisible())
        return;

    StopRefresh(CancelQueryReason::NewQuery);

    m_pGameList->DeleteAllItems();
    m_pGameList->SetSortColumnEx(m_ColumnsMap[GameListColumnType::Ping], -1, true);

    m_Servers.Clear();
    m_Servers.RequestFriends(GetFilter(), GetFilterCount());

    UpdateRefreshStatusText();
    SetRefreshing(true);
    ServerBrowserDialog().UpdateStatusText("#ServerBrowser_GettingNewServerList");
}

bool CFriendsGames::SupportsItem(CBaseGamesPage::InterfaceItem item)
{
    switch (item)
    {
        case CBaseGamesPage::InterfaceItem::Filters:
            return true;

        default:
            return false;
    }
}

void CFriendsGames::StartRefresh()
{
    StopRefresh(CancelQueryReason::NewQuery);

    SetRefreshing(true);
    ServerBrowserDialog().UpdateStatusText("#ServerBrowser_GettingNewServerList");

    if (m_pGameList->GetItemCount() == 0)
        GetNewServerList();
    else
        m_Servers.StartRefresh();
}

void CFriendsGames::StopRefresh(CancelQueryReason reason)
{
    CBaseGamesPage::StopRefresh(reason);
}

void CFriendsGames::OnViewGameInfo()
{
    if (!m_pGameList->GetSelectedItemsCount())
        return;

    int serverID = m_pGameList->GetItemUserData(m_pGameList->GetSelectedItem(0));

    ServerBrowserDialog().OpenGameInfoDialog(this, serverID);
}

GuiConnectionSource CFriendsGames::GetConnectionSource()
{
    return GuiConnectionSource::Unknown;
}

void CFriendsGames::OnOpenContextMenu(int itemID)
{
    if (!m_pGameList->GetSelectedItemsCount())
        return;

    int serverID = m_pGameList->GetItemData(m_pGameList->GetSelectedItem(0))->userData;

    CServerContextMenu *menu = ServerBrowserDialog().GetContextMenu(m_pGameList);
    menu->ShowMenu(this, serverID, true, true, true, true);
}

void CFriendsGames::OnRefreshServer(int serverID)
{
    if (m_Servers.IsRefreshing())
        return;

    m_Servers.StartRefreshServer(serverID);
}
