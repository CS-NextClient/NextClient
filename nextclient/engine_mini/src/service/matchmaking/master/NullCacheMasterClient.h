#pragma once
#include "MasterClientInterface.h"
#include "MasterClientCacheInterface.h"

class NullCacheMasterClient : public MasterClientCacheInterface
{
public:
    concurrencpp::result<std::vector<netadr_t>> GetServerAddressesAsync(
        std::function<void(const netadr_t&)> address_received_callback,
        std::shared_ptr<taskcoro::CancellationToken> cancellation_token
    ) override;

    void Save(const std::vector<netadr_t>& server_list) override;
};
