#pragma once
#include <memory>

#include <cpr/api.h>
#include <next_engine_mini/NextClientVersion.h>

#include "MasterClientInterface.h"

class HttpMasterClient : public MasterClientInterface
{
    std::string url_{};
    std::unordered_map<std::string, std::string> headers_{};

public:
    explicit HttpMasterClient(NextClientVersion client_version, std::string url);

    concurrencpp::result<std::vector<netadr_t>> GetServerAddressesAsync(
        std::function<void(const netadr_t&)> address_received_callback,
        std::shared_ptr<taskcoro::CancellationToken> cancellation_token
    ) override;

private:
    static std::vector<netadr_t> ParseResponse(const std::string& data);
};
