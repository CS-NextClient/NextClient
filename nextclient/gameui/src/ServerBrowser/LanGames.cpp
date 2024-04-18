#include "LanGames.h"

#include "ServerContextMenu.h"
#include "ServerListCompare.h"
#include "ServerBrowserDialog.h"
#include "InternetGames.h"

#include <KeyValues.h>
#include <vgui/IScheme.h>
#include <vgui/IVGui.h>

#include <vgui_controls/Button.h>
#include <vgui_controls/ToggleButton.h>
#include <vgui_controls/ImagePanel.h>
#include <vgui_controls/ListPanel.h>

using namespace vgui2;

CLanGames::CLanGames(vgui2::Panel *parent, bool bAutoRefresh, const char *pCustomResFilename) :
    CBaseGamesPage(parent, "LanGames", pCustomResFilename),
    auto_refresh_(bAutoRefresh)
{
}

CLanGames::~CLanGames(void)
{
}

void CLanGames::OnPageShow(void)
{
    BaseClass::OnPageShow();

    if (m_pGameList->GetItemCount() == 0 || auto_refresh_)
        GetNewServerList();
}

void CLanGames::OnPageHide(void)
{
    BaseClass::OnPageHide();
}

bool CLanGames::SupportsItem(InterfaceItem item)
{
    switch (item)
    {
        case InterfaceItem::Filters: return true;
    }

    return false;
}

void CLanGames::StartRefresh(void)
{
    StopRefresh(CancelQueryReason::NewQuery);

    SetRefreshing(true);
    ServerBrowserDialog().UpdateStatusText("#ServerBrowser_GettingNewServerList");

    if (m_pGameList->GetItemCount() == 0)
        GetNewServerList();
    else
        m_Servers.StartRefresh();
}

void CLanGames::GetNewServerList(void)
{
    if (!IsVisible())
        return;

    StopRefresh(CancelQueryReason::NewQuery);

    m_pGameList->DeleteAllItems();
    m_pGameList->SetSortColumnEx(m_ColumnsMap[GameListColumnType::Password], -1, true);

    m_Servers.Clear();
    m_Servers.RequestLan();

    UpdateRefreshStatusText();
    SetRefreshing(true);
    ServerBrowserDialog().UpdateStatusText("#ServerBrowser_GettingNewServerList");
}

void CLanGames::ServerFailedToRespond(serveritem_t &server)
{
    if (m_pGameList->IsValidItemID(server.listEntryID))
        m_pGameList->SetItemVisible(server.listEntryID, false);

    UpdateRefreshStatusText();
}

void CLanGames::RefreshComplete()
{
    SetRefreshing(false);
    UpdateFilterSettings();

    if (m_pGameList->GetItemCount() == 0)
        m_pGameList->SetEmptyListText("#ServerBrowser_NoInternetGamesResponded");

    UpdateRefreshStatusText();
}

GuiConnectionSource CLanGames::GetConnectionSource()
{
    return GuiConnectionSource::Unknown;
}

void CLanGames::OnViewGameInfo(void)
{
    if (!m_pGameList->GetSelectedItemsCount())
        return;

    int serverID = m_pGameList->GetItemUserData(m_pGameList->GetSelectedItem(0));

    ServerBrowserDialog().OpenGameInfoDialog(this, serverID);
}

void CLanGames::OnRefreshServer(int serverID)
{
    if (m_Servers.IsRefreshing())
        return;

    m_Servers.StartRefreshServer(serverID);
}

void CLanGames::OnOpenContextMenu(int row)
{
    if (!m_pGameList->GetSelectedItemsCount())
        return;

    int serverID = m_pGameList->GetItemUserData(m_pGameList->GetSelectedItem(0));

    CServerContextMenu *menu = ServerBrowserDialog().GetContextMenu(m_pGameList);
    menu->ShowMenu(this, serverID, true, true, true, false);
}