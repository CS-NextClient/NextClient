#pragma once
#include "MasterClientInterface.h"
#include "MasterClientCacheInterface.h"

class NullCacheMasterClient : public MasterClientCacheInterface
{
public:
    concurrencpp::result<std::vector<MasterServerEntry>> GetServerListAsync(
        std::function<void(const MasterServerEntry&)> entry_received_callback,
        std::shared_ptr<taskcoro::CancellationToken> cancellation_token
    ) override;

    void Save(const std::vector<MasterServerEntry>& server_list) override;
};
