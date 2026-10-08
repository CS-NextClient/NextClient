#pragma once
#include <taskcoro/CancellationToken.h>
#include <taskcoro/TaskCoro.h>

#include "service/matchmaking/master/MasterServerEntry.h"

class MasterClientInterface
{
public:
    virtual ~MasterClientInterface() = default;

    virtual concurrencpp::result<std::vector<MasterServerEntry>> GetServerListAsync(
        std::function<void(const MasterServerEntry&)> entry_received_callback = {},
        std::shared_ptr<taskcoro::CancellationToken> cancellation_token = nullptr
    ) = 0;
};
