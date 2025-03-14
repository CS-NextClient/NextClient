#pragma once
#include <vector>

namespace service::matchmaking
{
    struct ServerListRequestData
    {
        HServerListRequest request_id{};
        ISteamMatchmakingServerListResponse* response_callback{};

        std::shared_ptr<taskcoro::CancellationToken> cancellation_token = taskcoro::CancellationToken::Create();
        bool in_progress = true;

        std::vector<gameserveritem_t> servers{};

        explicit ServerListRequestData() = default;

        explicit ServerListRequestData(
            HServerListRequest request_id,
            ISteamMatchmakingServerListResponse* response_callback
        ) :
            request_id(request_id),
            response_callback(response_callback)
        { }
    };
}
