#ifndef HISTORYGAMES_H
#define HISTORYGAMES_H

#ifdef _WIN32
#pragma once
#endif

#include "BaseGamesPage.h"

#include "IGameList.h"
#include "serveritem.h"

class CHistoryGames : public CBaseGamesPage
{
    DECLARE_CLASS_SIMPLE(CHistoryGames, CBaseGamesPage);

    static const std::vector<GameListColumnType> PageColumns;

public:
    explicit CHistoryGames(vgui2::Panel *parent);
    ~CHistoryGames() override;

public:
    void OnPageShow() override;
    void OnPageHide() override;
    void OnBeginConnect() override;
    void OnViewGameInfo() override;

public:
    bool SupportsItem(InterfaceItem item) override;
    void StartRefresh() override;
    void GetNewServerList() override;
    void StopRefresh(CancelQueryReason reason) override;
    virtual void ListReceived(bool moreAvailable, int lastUnique);
    void ServerFailedToRespond(serveritem_t &server) override;
    void RefreshComplete() override;

    GuiConnectionSource GetConnectionSource() override;

private:
    void OnRefreshServer(int serverID);

private:
    MESSAGE_FUNC_INT(OnOpenContextMenu, "OpenContextMenu", itemID);
    MESSAGE_FUNC(OnRemoveFromHistory, "RemoveFromHistory");
};

#endif