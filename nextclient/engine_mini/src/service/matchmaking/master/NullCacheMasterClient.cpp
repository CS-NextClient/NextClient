#include "NullCacheMasterClient.h"

concurrencpp::result<std::vector<MasterServerEntry>> NullCacheMasterClient::GetServerListAsync(
    std::function<void(const MasterServerEntry&)> entry_received_callback,
    std::shared_ptr<taskcoro::CancellationToken> cancellation_token)
{
    co_return std::vector<MasterServerEntry>{};
}

void NullCacheMasterClient::Save(const std::vector<MasterServerEntry>& server_list)
{

}
