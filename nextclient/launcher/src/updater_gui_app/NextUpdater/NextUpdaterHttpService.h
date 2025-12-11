#pragma once
#include <chrono>

#include <cpr/api.h>
#include <concurrencpp/concurrencpp.h>
#include <taskcoro/TaskCoro.h>
#include <next_launcher/UserInfoClient.h>
#include <next_launcher/IBackendAddressResolver.h>
#include <updater_gui_app/HttpServiceInterface.h>

using RequestCallback = std::function<void(const HttpResponse&)>;
using DataReader = std::function<std::string()>;

class NextUpdaterHttpService : public HttpServiceInterface
{
    static constexpr std::chrono::milliseconds kConnectTimeout{5000};
    static constexpr std::chrono::milliseconds kTimeout{5000};

    std::shared_ptr<next_launcher::UserInfoClient> user_info_;
    std::shared_ptr<next_launcher::IBackendAddressResolver> backend_address_resolver_;

    cpr::Header headers_;
    taskcoro::TaskTracker task_tracker_;
    std::shared_ptr<taskcoro::CancellationToken> ct_;

    bool is_backend_address_initialized_{};
    std::string backend_address_;

public:
    explicit NextUpdaterHttpService(
        std::shared_ptr<next_launcher::UserInfoClient> user_info,
        std::shared_ptr<next_launcher::IBackendAddressResolver> backend_address_resolver
    );
    ~NextUpdaterHttpService() override;

    concurrencpp::result<void> ShutdownAsync() override;

    concurrencpp::result<HttpResponse> PostAsync(
        std::string method,
        std::string data,
        std::shared_ptr<taskcoro::CancellationToken> cancellation_token) override;

private:
    HttpResponse PostInternal(
        const std::string& method,
        const std::string& data,
        std::shared_ptr<taskcoro::CancellationToken> cancellation_token);

    concurrencpp::result<void> InitializeBackendAddressIfNeeded(std::shared_ptr<taskcoro::CancellationToken> cancellation_token);
    void InitializeHeaders();
    cpr::Header GetHeadersForRequest() const;
    std::string BuildJsonPayload(const std::string& method, const std::string& data) const;
    HttpResponse ParseResponse(const cpr::Response& response) const;
    std::string SerializeAes(const std::string& data) const;
    std::string DeserializeAes(const std::string& json_str) const;
};
