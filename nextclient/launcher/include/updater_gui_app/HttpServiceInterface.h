#pragma once
#include <string>
#include <cpr/cpr.h>
#include <taskcoro/TaskCoro.h>

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

using RequestCallback = std::function<void(const HttpResponse&)>;

class HttpServiceInterface
{
public:
    virtual ~HttpServiceInterface() = default;

    virtual concurrencpp::result<HttpResponse> PostAsync(
        std::string method,
        std::string data,
        std::shared_ptr<taskcoro::CancellationToken> cancellation_token) = 0;

    virtual concurrencpp::result<void> ShutdownAsync() = 0;
};
