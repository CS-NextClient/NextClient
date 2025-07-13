#pragma once
#include <updater/HttpServiceInterface.h>

class HttpServiceMock : public HttpServiceInterface
{
    std::unordered_map<std::string, HttpResponse> method_response_map_;

public:
    explicit HttpServiceMock(std::unordered_map<std::string, HttpResponse> method_response_map);

    HttpResponse Post(
        const std::string& method,
        const std::string& data,
        std::function<bool(cpr::cpr_off_t total, cpr::cpr_off_t downloaded)> progress,
        int timeout_ms) override;
};
