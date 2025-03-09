#pragma once
#include <variant>
#include <steam/steam_api.h>

#include "MatchmakingService.h"
#include "ServerListRequestData.h"
#include "SteamServerListRequestData.h"

namespace service::matchmaking
{
    class MatchmakingSteamComp : public ISteamMatchmakingServers
    {
        uint32 app_id_{};

        int server_list_request_counter_ = 0;
        std::unordered_map<HServerListRequest, std::variant<SteamServersListRequestData, ServerListRequestData>> server_requests_{};

        MatchmakingService matchmaking_service_;

    public:
        explicit MatchmakingSteamComp();

        HServerListRequest RequestInternetServerList(AppId_t iApp, MatchMakingKeyValuePair_t** ppchFilters, uint32 nFilters, ISteamMatchmakingServerListResponse* response_callback) override;
        HServerListRequest RequestLANServerList(AppId_t iApp, ISteamMatchmakingServerListResponse* pRequestServersResponse) override;
        HServerListRequest RequestFriendsServerList(AppId_t iApp, MatchMakingKeyValuePair_t** ppchFilters, uint32 nFilters, ISteamMatchmakingServerListResponse* pRequestServersResponse) override;
        HServerListRequest RequestFavoritesServerList(AppId_t iApp, MatchMakingKeyValuePair_t** ppchFilters, uint32 nFilters, ISteamMatchmakingServerListResponse* pRequestServersResponse) override;
        HServerListRequest RequestHistoryServerList(AppId_t iApp, MatchMakingKeyValuePair_t** ppchFilters, uint32 nFilters, ISteamMatchmakingServerListResponse* pRequestServersResponse) override;
        HServerListRequest RequestSpectatorServerList(AppId_t iApp, MatchMakingKeyValuePair_t** ppchFilters, uint32 nFilters, ISteamMatchmakingServerListResponse* pRequestServersResponse) override;
        void ReleaseRequest(HServerListRequest request_id) override;
        gameserveritem_t* GetServerDetails(HServerListRequest request_id, int iServer) override;
        void CancelQuery(HServerListRequest hRequest) override;
        void RefreshQuery(HServerListRequest hRequest) override;
        bool IsRefreshing(HServerListRequest hRequest) override;
        int GetServerCount(HServerListRequest hRequest) override;
        void RefreshServer(HServerListRequest hRequest, int iServer) override;
        HServerQuery PingServer(uint32 unIP, uint16 usPort, ISteamMatchmakingPingResponse* pRequestServersResponse) override;
        HServerQuery PlayerDetails(uint32 unIP, uint16 usPort, ISteamMatchmakingPlayersResponse* pRequestServersResponse) override;
        HServerQuery ServerRules(uint32 unIP, uint16 usPort, ISteamMatchmakingRulesResponse* pRequestServersResponse) override;
        void CancelServerQuery(HServerQuery hServerQuery) override;

    private:
        void InitEmptyGameServerItem(gameserveritem_t& gameserveritem);
    };
}
