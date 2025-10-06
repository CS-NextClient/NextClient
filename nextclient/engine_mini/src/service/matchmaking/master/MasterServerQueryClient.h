#pragma once
#include "MasterClientInterface.h"
#include "MasterRegionCode.h"
#include "constants.h"

class MasterServerQueryClient : public MasterClientInterface
{
    const sockaddr_in master_addr_{};
    const MasterRegionCode region_code_{};
    const std::chrono::milliseconds next_request_delay_{};
    const std::chrono::milliseconds query_delay_{};
    const std::chrono::milliseconds query_timeout_{};
    const std::string base_filter_{};

    std::mutex requests_mutex_{};
    concurrencpp::result<void> tail_result_ = concurrencpp::make_ready_result<void>();

    std::chrono::steady_clock::time_point next_query_time_{};
    uint32_t request_counter_{};

public:
    explicit MasterServerQueryClient(
        netadr_t master_addr,
        MasterRegionCode region_code,
        int appid,
        const std::string& gamedir,
        std::chrono::milliseconds next_request_delay = std::chrono::milliseconds{kDefaultNextRequestDelay},
        std::chrono::milliseconds query_delay = std::chrono::milliseconds{kDefaultQueryDelay},
        std::chrono::milliseconds query_timeout = std::chrono::milliseconds{kDefaultQueryTimeout});

    concurrencpp::result<std::vector<netadr_t>> GetServerAddressesAsync(
        std::function<void(const netadr_t&)> address_received_callback,
        std::shared_ptr<taskcoro::CancellationToken> cancellation_token
    ) override;

private:
    concurrencpp::result<std::vector<netadr_t>> GetServerAddressesInternalAsync(
        SOCKET sock,
        uint32_t request_num,
        std::function<void(const netadr_t&)> address_received_callback,
        std::shared_ptr<taskcoro::CancellationToken> cancellation_token
    );

    concurrencpp::result<concurrencpp::result_promise<void>> BeginSequentialRequest();

    static std::vector<netadr_t> ParseMasterServerResponse(ByteBuffer& buffer);
};
