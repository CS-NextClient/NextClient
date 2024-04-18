#pragma once

#include <unordered_map>
#include <steam/steam_api.h>
#include "serveritem.h"
#include "IServerRefreshResponse.h"
#include "IGameList.h"

class CServerList : public ISteamMatchmakingServerListResponse
{
    IServerRefreshResponse* response_target_;
    HServerListRequest server_list_request_ = nullptr;

    // key - server id
    std::unordered_map<int, serveritem_t> servers_;

public:
    explicit CServerList(IServerRefreshResponse* response_target);
    ~CServerList();

    void SetRequest(HServerListRequest hRequest);
    void RequestFavorites(MatchMakingKeyValuePair_t **ppchFilters, uint32 nFilters);
    void RequestInternet(MatchMakingKeyValuePair_t **ppchFilters, uint32 nFilters);
    void RequestUnique(MatchMakingKeyValuePair_t **ppchFilters, uint32 nFilters);
    void RequestHistory(MatchMakingKeyValuePair_t **ppchFilters, uint32 nFilters);
    void RequestFriends(MatchMakingKeyValuePair_t **ppchFilters, uint32 nFilters);
    void RequestLan();

    bool IsServerExists(int iServer);
    serveritem_t &GetServer(int iServer);
    unsigned int ServerCount();
    void StartRefreshServer(int iServer);
    void StartRefresh();
    void StopRefresh(IGameList::CancelQueryReason reason);
    void Clear();
    bool IsRefreshing();

    std::unordered_map<int, serveritem_t>::iterator begin();
    std::unordered_map<int, serveritem_t>::iterator end();

    void ServerResponded(HServerListRequest hRequest, int iServer) override;
    void ServerFailedToRespond(HServerListRequest hRequest, int iServer) override;
    void RefreshComplete(HServerListRequest hRequest, EMatchMakingServerResponse response) override;

private:
    void UpdateServerItem(bool successful_response, int iServer);
};

