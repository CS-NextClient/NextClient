#pragma once
#include <tier1/netadr.h>
#include <taskcoro/CancellationToken.h>
#include <taskcoro/TaskCoro.h>

class MasterClientInterface
{
public:
    virtual ~MasterClientInterface() = default;

    virtual concurrencpp::result<std::vector<netadr_t>> GetServerAddressesAsync(
        std::function<void(const netadr_t&)> address_received_callback = {},
        std::shared_ptr<taskcoro::CancellationToken> cancellation_token = nullptr
    ) = 0;
};
