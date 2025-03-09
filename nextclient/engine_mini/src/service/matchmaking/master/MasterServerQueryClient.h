#pragma once
#include "MasterClientInterface.h"
#include "MasterRegionCode.h"

class MasterServerQueryClient : public MasterClientInterface
{
    netadr_t master_addr_;
    MasterRegionCode region_code_;
    std::string base_filter_;

public:
    explicit MasterServerQueryClient(netadr_t master_addr, MasterRegionCode region_code, int appid, const std::string& gamedir);

    concurrencpp::result<std::vector<netadr_t>> GetServerAddressesAsync(
        std::function<void(const netadr_t&)> address_received_callback,
        std::shared_ptr<taskcoro::CancellationToken> cancellation_token
    ) override;

private:
    concurrencpp::result<std::vector<netadr_t>> GetServerAddressesAsyncInternal(
        SOCKET sock,
        std::function<void(const netadr_t&)> address_received_callback,
        std::shared_ptr<taskcoro::CancellationToken> cancellation_token
    );

    static std::vector<netadr_t> ParseMasterServerResponse(ByteBuffer& buffer);
};
