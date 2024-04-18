#pragma once

#include <future>
#include <next_launcher/UserInfoClient.h>
#include <updater/HttpServiceInterface.h>
#include <concurrencpp/concurrencpp.h>

class NextUpdaterHttpService : public HttpServiceInterface
{
    std::string service_url_;
    int connect_timeout_ms_;
    std::shared_ptr<next_launcher::UserInfoClient> user_info_;
    cpr::Header headers_;

    concurrencpp::runtime cc_runtime_;
    std::shared_ptr<concurrencpp::thread_pool_executor> thread_executor_;
    std::shared_ptr<concurrencpp::manual_executor> update_executor_;

public:
    explicit NextUpdaterHttpService(std::string service_url, int connect_timeout_ms, std::shared_ptr<next_launcher::UserInfoClient> user_info);
    ~NextUpdaterHttpService() override = default;

    HttpResponse Post(const std::string &method, const std::string& data) override;
    std::shared_ptr<CancelationToken> PostAsync(const std::string &method, const std::string& data, const std::function<void(const HttpResponse&)>& callback) override;
    void Update() override;

private:
    concurrencpp::result<void> PostCoroutine(std::string method, std::function<std::string()> data_reader_threaded, std::shared_ptr<CancelationToken> cancelation_token, std::function<void(const HttpResponse&)> callback);
    HttpResponse PostInternal(const std::string &method, const std::string& data);
    static std::string SerializeAes(const std::string& json_str);
    static std::string DeserializeAes(const std::string &json_str);
};