#include "GameUi.h"
#include "InternetGames.h"
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

CInternetGames::CInternetGames(vgui2::Panel *parent, bool bAutoRefresh, const char *panelName) :
    CBaseGamesPage(parent, panelName),
    auto_refresh_(bAutoRefresh)
{
    m_pLocationFilter->DeleteAllItems();
}

CInternetGames::~CInternetGames()
{

}

void CInternetGames::OnPageShow()
{
    if (!ServerBrowserDialog().IsVisible())
        return;

    if (m_pGameList->GetItemCount() == 0)
        GetNewServerList();
    else if (auto_refresh_)
        StartRefresh();
}

void CInternetGames::OnPageHide()
{
    StopRefresh(CancelQueryReason::PageClosed);
}

void CInternetGames::ServerResponded(serveritem_t &server)
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

void CInternetGames::ServerFailedToRespond(serveritem_t &server)
{
    if (m_pGameList->IsValidItemID(server.listEntryID))
        m_pGameList->SetItemVisible(server.listEntryID, false);

    UpdateRefreshStatusText();

    server_refresh_count_++;
}

void CInternetGames::RefreshComplete()
{
    SetRefreshing(false);
    UpdateFilterSettings();

    list_refreshing_ = false;
    server_refresh_count_ = 0;

    if (m_pGameList->GetItemCount() == 0)
        m_pGameList->SetEmptyListText("#ServerBrowser_NoInternetGamesResponded");

    UpdateRefreshStatusText();
}

GuiConnectionSource CInternetGames::GetConnectionSource()
{
    return GuiConnectionSource::ServersInternet;
}

void CInternetGames::OnItemSelected()
{
    BaseClass::OnItemSelected();

    if (!IsActivated())
        return;

    if (m_pGameList->GetSelectedItemsCount() < 1)
        return;

    int selected_id = m_pGameList->GetSelectedItem(0);
    int row = m_pGameList->GetItemCurrentRow(selected_id) + 1;
    KeyValues* kv = m_pGameList->GetItem(selected_id);

    int ip = kv->GetInt("_ip");
    int port = kv->GetInt("_port");

    GameUINext().InvokeInternetServerSelected(ip, port, row, m_pGameList->GetItemCount());
}

void CInternetGames::GetNewServerList()
{
    if (!IsVisible())
        return;

    StopRefresh(CancelQueryReason::NewQuery);

    m_pGameList->DeleteAllItems();
    m_pGameList->SetSortColumnEx(m_ColumnsMap[GameListColumnType::Password], -1, true);

    m_Servers.Clear();
    m_Servers.RequestInternet(GetFilter(), GetFilterCount());

    UpdateRefreshStatusText();
    SetRefreshing(true);
    ServerBrowserDialog().UpdateStatusText("#ServerBrowser_GettingNewServerList");
}

bool CInternetGames::SupportsItem(CBaseGamesPage::InterfaceItem item)
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

void CInternetGames::StartRefresh()
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

void CInternetGames::StopRefresh(CancelQueryReason reason)
{
    list_refreshing_ = false;
    server_refresh_count_ = 0;

    CBaseGamesPage::StopRefresh(reason);
}

void CInternetGames::OnBeginConnect()
{
    if (!m_pGameList->GetSelectedItemsCount())
        return;

    int serverID = m_pGameList->GetItemUserData(m_pGameList->GetSelectedItem(0));

    StopRefresh(CancelQueryReason::ConnectToServer);
    ServerBrowserDialog().JoinGame(this, serverID, GuiConnectionSource::ServersInternet);
}

void CInternetGames::OnViewGameInfo()
{
    if (!m_pGameList->GetSelectedItemsCount())
        return;

    int serverID = m_pGameList->GetItemUserData(m_pGameList->GetSelectedItem(0));

    ServerBrowserDialog().OpenGameInfoDialog(this, serverID);
}

void CInternetGames::OnSaveFilter(KeyValues* filter)
{
    ServerBrowserDialog().GetFilterSaveData(GetName())->Clear();
}

void CInternetGames::OnOpenContextMenu(int itemID)
{
    if (!m_pGameList->GetSelectedItemsCount())
        return;

    int serverID = m_pGameList->GetItemData(m_pGameList->GetSelectedItem(0))->userData;

    CServerContextMenu *menu = ServerBrowserDialog().GetContextMenu(m_pGameList);
    menu->ShowMenu(this, serverID, true, true, true, true);
}

void CInternetGames::OnRefreshServer(int serverID)
{
    if (m_Servers.IsRefreshing())
        return;

    m_Servers.StartRefreshServer(serverID);
}

int CInternetGames::GetRegionCodeToFilter()
{
    KeyValues *kv = m_pLocationFilter->GetActiveItemUserData();

    if (kv)
        return kv->GetInt("code");
    else
        return 255;
}


