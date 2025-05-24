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

        int server_list_request_counter_{};
        std::unordered_map<HServerListRequest, std::variant<SteamServersListRequestData, ServerListRequestData>> server_requests_{};
        std::shared_ptr<MultiSourceQuery> source_query_{};
        std::shared_ptr<MatchmakingService> matchmaking_service_{};

    public:
        explicit MatchmakingSteamComp();

        // ISteamMatchmakingServers
        HServerListRequest RequestInternetServerList(AppId_t iApp, MatchMakingKeyValuePair_t** ppchFilters, uint32 nFilters, ISteamMatchmakingServerListResponse* response_callback) override;
        HServerListRequest RequestLANServerList(AppId_t iApp, ISteamMatchmakingServerListResponse* response_callback) override;
        HServerListRequest RequestFriendsServerList(AppId_t iApp, MatchMakingKeyValuePair_t** ppchFilters, uint32 nFilters, ISteamMatchmakingServerListResponse* response_callback) override;
        HServerListRequest RequestFavoritesServerList(AppId_t iApp, MatchMakingKeyValuePair_t** ppchFilters, uint32 nFilters, ISteamMatchmakingServerListResponse* response_callback) override;
        HServerListRequest RequestHistoryServerList(AppId_t iApp, MatchMakingKeyValuePair_t** ppchFilters, uint32 nFilters, ISteamMatchmakingServerListResponse* response_callback) override;
        HServerListRequest RequestSpectatorServerList(AppId_t iApp, MatchMakingKeyValuePair_t** ppchFilters, uint32 nFilters, ISteamMatchmakingServerListResponse* response_callback) override;
        void ReleaseRequest(HServerListRequest request_id) override;
        gameserveritem_t* GetServerDetails(HServerListRequest request_id, int iServer) override;
        void CancelQuery(HServerListRequest hRequest) override;
        void RefreshQuery(HServerListRequest hRequest) override;
        bool IsRefreshing(HServerListRequest hRequest) override;
        int GetServerCount(HServerListRequest hRequest) override;
        void RefreshServer(HServerListRequest hRequest, int iServer) override;
        HServerQuery PingServer(uint32 unIP, uint16 usPort, ISteamMatchmakingPingResponse* response_callback) override;
        HServerQuery PlayerDetails(uint32 unIP, uint16 usPort, ISteamMatchmakingPlayersResponse* response_callback) override;
        HServerQuery ServerRules(uint32 unIP, uint16 usPort, ISteamMatchmakingRulesResponse* response_callback) override;
        void CancelServerQuery(HServerQuery hServerQuery) override;

    private:
        concurrencpp::result<void> RequestServerList(
            HServerListRequest request_id,
            MatchmakingService::ServerListSource server_list_source,
            ISteamMatchmakingServerListResponse* response_callback,
            std::shared_ptr<taskcoro::CancellationToken> ct
        );
        concurrencpp::result<void> RefreshServerList(
            HServerListRequest request_id,
            const std::vector<gameserveritem_t>& gameservers,
            ISteamMatchmakingServerListResponse* response_callback,
            std::shared_ptr<taskcoro::CancellationToken> ct
        );
        void ServerAnsweredHandler(
            HServerListRequest request_id,
            ISteamMatchmakingServerListResponse* response_callback,
            const MatchmakingService::ServerInfo& server_info
        );
        void InitEmptyGameServerItem(gameserveritem_t& gameserver, uint32_t ip, uint16_t port);
    };
}
