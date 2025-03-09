#pragma once
#include <steam/steam_api.h>

#include "SteamMatchmakingServerListResponseProxy.h"

namespace service::matchmaking
{
    struct SteamServersListRequestData
    {
        HServerListRequest request_id{};
        ISteamMatchmakingServerListResponse* response_callback{};
        HServerListRequest steam_request_id{};
        SteamMatchmakingServerListResponseProxy* steam_response_callback{};

        explicit SteamServersListRequestData() = default;

        explicit SteamServersListRequestData(
            HServerListRequest request_id,
            ISteamMatchmakingServerListResponse* response_callback,
            HServerListRequest steam_request_id,
            SteamMatchmakingServerListResponseProxy* steam_response_callback
        ) :
            request_id(request_id),
            response_callback(response_callback),
            steam_request_id(steam_request_id),
            steam_response_callback(steam_response_callback)
        { }
    };
}
