#pragma once

#include "BaseGamesPage.h"
#include <unordered_set>

class CFriendsGames : public CBaseGamesPage
{
    DECLARE_CLASS_SIMPLE(CFriendsGames, CBaseGamesPage);

    static const std::vector<GameListColumnType> PageColumns;

public:
    explicit CFriendsGames(vgui2::Panel *parent, bool bAutoRefresh = true, const char *panelName = "FriendsGames");
    ~CFriendsGames() override;

public:
    void OnPageShow() override;
    void OnPageHide() override;
    void OnViewGameInfo()override;

    GuiConnectionSource GetConnectionSource() override;

    MESSAGE_FUNC_INT(OnRefreshServer, "RefreshServer", serverID);

public:
    bool SupportsItem(InterfaceItem item) override;
    void StartRefresh() override;
    void StopRefresh(CancelQueryReason reason) override;

    void ServerFailedToRespond(serveritem_t &server) override;
    void RefreshComplete() override;

    MESSAGE_FUNC(GetNewServerList, "GetNewServerList");

private:
    MESSAGE_FUNC_INT(OnOpenContextMenu, "OpenContextMenu", itemID);

private:
    bool auto_refresh_;
};

