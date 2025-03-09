#pragma once
#include <steam/steam_api.h>

namespace service::matchmaking
{
    class ServerListResponseProxy : public ISteamMatchmakingServerListResponse
    {
        ISteamMatchmakingServerListResponse* response_{};
        HServerListRequest replace_to_request_{};

    public:
        explicit ServerListResponseProxy(ISteamMatchmakingServerListResponse* response, HServerListRequest replace_to_request) :
            response_(response),
            replace_to_request_(replace_to_request)
        { }

        void ServerResponded(HServerListRequest hRequest, int iServer) override
        {
            response_->ServerResponded(replace_to_request_, iServer);
        }

        void ServerFailedToRespond(HServerListRequest hRequest, int iServer) override
        {
            response_->ServerFailedToRespond(replace_to_request_, iServer);
        }

        void RefreshComplete(HServerListRequest hRequest, EMatchMakingServerResponse response) override
        {
            response_->RefreshComplete(replace_to_request_, response);
        }
    };
}
