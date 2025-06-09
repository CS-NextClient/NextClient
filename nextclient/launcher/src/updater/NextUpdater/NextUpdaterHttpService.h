#pragma once

#include <future>
#include <next_launcher/UserInfoClient.h>
#include <updater/HttpServiceInterface.h>

class NextUpdaterHttpService : public HttpServiceInterface
{
    std::string service_url_;
    int connect_timeout_ms_;
    std::shared_ptr<next_launcher::UserInfoClient> user_info_;
    cpr::Header headers_;

public:
    explicit NextUpdaterHttpService(std::string service_url, int connect_timeout_ms, std::shared_ptr<next_launcher::UserInfoClient> user_info);
    ~NextUpdaterHttpService() override = default;

    HttpResponse Post(
        const std::string& method,
        const std::string& data,
        std::function<bool(cpr::cpr_off_t total, cpr::cpr_off_t downloaded)> progress = nullptr,
        int timeout_ms = 0) override;

private:
    static std::string SerializeAes(const std::string& json_str);
    static std::string DeserializeAes(const std::string &json_str);
};