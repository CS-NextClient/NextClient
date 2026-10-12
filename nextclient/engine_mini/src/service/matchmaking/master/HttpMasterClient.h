#pragma once
#include <memory>
#include <optional>

#include <cpr/api.h>
#include <next_engine_mini/NextClientVersion.h>

#include "MasterClientInterface.h"

class HttpMasterClient : public MasterClientInterface
{
    static constexpr std::chrono::milliseconds kConnectTimeout{5000};
    static constexpr std::chrono::milliseconds kTimeout{15000};

    std::string url_{};
    std::unordered_map<std::string, std::string> headers_{};

public:
    explicit HttpMasterClient(NextClientVersion client_version, std::string url);

    concurrencpp::result<std::vector<MasterServerEntry>> GetServerListAsync(
        std::function<void(const MasterServerEntry&)> entry_received_callback,
        std::shared_ptr<taskcoro::CancellationToken> cancellation_token
    ) override;
};

// nullopt for a failed request; an answer that lists no server gives the lone 0.0.0.0:0 entry, which marks the list
// as final
std::optional<std::vector<MasterServerEntry>> HttpMasterClient_ReadServerList(const cpr::Response& response);
