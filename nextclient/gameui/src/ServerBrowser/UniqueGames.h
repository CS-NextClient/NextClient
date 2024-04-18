#pragma once

#include "BaseGamesPage.h"
#include <unordered_set>

class CUniqueGames : public CBaseGamesPage
{
    DECLARE_CLASS_SIMPLE(CUniqueGames, CBaseGamesPage);

    static const std::vector<GameListColumnType> PageColumns;

public:
    explicit CUniqueGames(vgui2::Panel *parent, bool bAutoRefresh = true, const char *panelName = "UniqueGames");
    ~CUniqueGames() override;

public:
    void OnPageShow() override;
    void OnPageHide() override;
    void OnViewGameInfo()override;

    GuiConnectionSource GetConnectionSource() override;

    MESSAGE_FUNC_INT(OnRefreshServer, "RefreshServer", serverID);

public:
    // void RulesResponded(uint32 ip, uint16 port, const char *pchRule, const char *pchValue) override;
    // void RulesFailedToRespond(uint32 ip, uint16 port) override;
    // void RulesRefreshComplete(uint32 ip, uint16 port) override;

public:
    bool SupportsItem(InterfaceItem item) override;
    void StartRefresh() override;
    void StopRefresh(CancelQueryReason reason) override;
    int GetRegionCodeToFilter() override;

    void ServerResponded(serveritem_t &server) override;
    void ServerFailedToRespond(serveritem_t &server) override;
    void RefreshComplete() override;

    MESSAGE_FUNC(GetNewServerList, "GetNewServerList");

private:
    MESSAGE_FUNC_INT(OnOpenContextMenu, "OpenContextMenu", itemID);

    void UpdateModDescription(const serveritem_t& serveritem, const char* description);
    void CancelSteamQueries();

private:
    std::unordered_set<HServerQuery> rule_queries_;

    float last_sort_ = .0f;
    int server_refresh_count_ = 0;
    bool auto_refresh_;
    bool list_refreshing_ = false;
};

