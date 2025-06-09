#pragma once

#include <string>
#include <cpr/cpr.h>

struct HttpResponse
{
    long status_code{};
    cpr::Error error;
    std::string data;

    [[nodiscard]] bool has_error() const { return error.code != cpr::ErrorCode::OK; }
    [[nodiscard]] bool has_connection_error() const
    {
        return error.code == cpr::ErrorCode::CONNECTION_FAILURE ||
               error.code == cpr::ErrorCode::OPERATION_TIMEDOUT ||
               error.code == cpr::ErrorCode::SSL_CONNECT_ERROR;
    }
};

class HttpServiceInterface
{
public:
    virtual ~HttpServiceInterface() = default;
    virtual HttpResponse Post(
        const std::string& method,
        const std::string& data,
        std::function<bool(cpr::cpr_off_t total, cpr::cpr_off_t downloaded)> progress = nullptr,
        int timeout_ms = 0) = 0;
};