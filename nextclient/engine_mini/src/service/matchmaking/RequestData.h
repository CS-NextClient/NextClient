#pragma once
#include <vector>

#include <steam/steam_api.h>
#include <taskcoro/CancellationToken.h>

namespace service::matchmaking
{
    struct RequestData
    {
        HServerListRequest request_id{};
        ISteamMatchmakingServerListResponse* response_callback{};
        std::vector<gameserveritem_t> servers{};
        bool broadcast{};

        bool in_progress{};
        std::shared_ptr<taskcoro::CancellationToken> cancellation_token{};

        explicit RequestData() = default;

        explicit RequestData(
            HServerListRequest request_id,
            ISteamMatchmakingServerListResponse* response_callback,
            std::vector<gameserveritem_t> servers,
            bool in_progress,
            bool broadcast = false
        ) :
            request_id(request_id),
            response_callback(response_callback),
            servers(std::move(servers)),
            in_progress(in_progress),
            cancellation_token(taskcoro::CancellationToken::Create()),
            broadcast(broadcast)
        { }
    };
}