#include "NullCacheMasterClient.h"

concurrencpp::result<std::vector<netadr_t>> NullCacheMasterClient::GetServerAddressesAsync(std::function<void(const netadr_t&)> address_received_callback,
    std::shared_ptr<taskcoro::CancellationToken> cancellation_token)
{
    co_return std::vector<netadr_t>{};
}

void NullCacheMasterClient::Save(const std::vector<netadr_t>& server_list)
{

}
