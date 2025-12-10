#pragma once
#include <string>
#include <vector>

#include <cpr/cpr.h>
#include <taskcoro/TaskCoro.h>
#include <next_launcher/UserInfoClient.h>
#include <next_launcher/IBackendAddressResolver.h>

class BackendAddressResolver : public next_launcher::IBackendAddressResolver
{
    static constexpr std::chrono::milliseconds kPingTimeout{5000};

    std::shared_ptr<next_launcher::UserInfoClient> user_info_;

    cpr::Header headers_;
    concurrencpp::shared_result<void> initialize_backend_task_;
    std::shared_ptr<taskcoro::CancellationToken> ct_;

    std::string backend_address_;

public:
    explicit BackendAddressResolver(std::shared_ptr<next_launcher::UserInfoClient> user_info);
    ~BackendAddressResolver() override;

    // IBackendAddressResolver
    concurrencpp::result<std::string> GetBackendAddressAsync(
        std::shared_ptr<taskcoro::CancellationToken> cancellation_token) override;

private:
    concurrencpp::result<void> ResolveBackendAddress();
    concurrencpp::result<std::string> GetFirstRespondedAddress(const std::vector<std::string>& addresses) const;
    cpr::Response PingRequest(const std::string& address) const;
    cpr::Header GetHeadersForRequest() const;
    void InitializeHeaders();
};