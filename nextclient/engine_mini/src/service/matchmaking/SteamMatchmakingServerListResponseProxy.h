#pragma once
#include <steam/steam_api.h>

namespace service::matchmaking
{
    class SteamMatchmakingServerListResponseProxy : public ISteamMatchmakingServerListResponse
    {
        ISteamMatchmakingServerListResponse* response_{};
        HServerListRequest request_id_{};

    public:
        SteamMatchmakingServerListResponseProxy(ISteamMatchmakingServerListResponse* response, HServerListRequest request_id);

        void ServerResponded(HServerListRequest hRequest, int iServer) override;
        void ServerFailedToRespond(HServerListRequest hRequest, int iServer) override;
        void RefreshComplete(HServerListRequest hRequest, EMatchMakingServerResponse response) override;
    };
}
