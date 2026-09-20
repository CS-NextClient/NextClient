#pragma once
#include <vector>

#include "service/matchmaking/master/MasterServerEntry.h"

namespace service::matchmaking
{
    struct ServerListEntry
    {
        gameserveritem_t gameserver{};
        MasterDetails master_details{};
    };

    struct ServerListRequestData
    {
        HServerListRequest request_id{};
        ISteamMatchmakingServerListResponse* response_callback{};

        std::shared_ptr<taskcoro::CancellationToken> cancellation_token{};
        bool in_progress = true;

        std::vector<ServerListEntry> servers{};

        explicit ServerListRequestData() = default;

        explicit ServerListRequestData(
            HServerListRequest request_id,
            ISteamMatchmakingServerListResponse* response_callback,
            std::shared_ptr<taskcoro::CancellationToken> cancellation_token
        ) :
            request_id(request_id),
            response_callback(response_callback),
            cancellation_token(cancellation_token)
        { }
    };
}
