#ifndef INTERNETGAMES_H
#define INTERNETGAMES_H

#ifdef _WIN32
#pragma once
#endif

#include "BaseGamesPage.h"

class CInternetGames : public CBaseGamesPage
{
    DECLARE_CLASS_SIMPLE(CInternetGames, CBaseGamesPage);

public:
    explicit CInternetGames(vgui2::Panel *parent, bool bAutoRefresh = true, const char *panelName = "InternetGames");
    ~CInternetGames() override;

public:
    void OnPageShow() override;
    void OnPageHide() override;
    void OnBeginConnect() override;
    void OnViewGameInfo() override;
    void OnSaveFilter(KeyValues* filter) override;

public:
    bool SupportsItem(InterfaceItem item) override;
    void StartRefresh() override;
    void StopRefresh(CancelQueryReason reason) override;
    int GetRegionCodeToFilter() override;

    void ServerResponded(serveritem_t &server) override;
    void ServerFailedToRespond(serveritem_t &server) override;
    void RefreshComplete() override;

    GuiConnectionSource GetConnectionSource() override;

protected:
    // messages overrides
    void OnItemSelected() override;
    void OnRefreshServer(int serverID) override;
    void GetNewServerList() override;

private:
    MESSAGE_FUNC_INT(OnOpenContextMenu, "OpenContextMenu", itemID);

private:
    int server_refresh_count_ = 0;
    bool auto_refresh_;
    bool list_refreshing_ = false;
};

#endif