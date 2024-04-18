#pragma once

#include <string>
#include <cpr/cpr.h>

struct HttpResponse
{
    long status_code{};
    cpr::Error error;
    std::string data;

    [[nodiscard]] bool has_error() const { return error.code != cpr::ErrorCode::OK; }
};

class CancelationToken
{
    bool canceled_ = false;

public:
    [[nodiscard]] bool IsCanceled() const { return canceled_; }
    void SetCanceled() { canceled_ = true; }
};

class HttpServiceInterface
{
public:
    virtual ~HttpServiceInterface() = default;
    virtual HttpResponse Post(const std::string& method, const std::string& data) = 0;
    virtual std::shared_ptr<CancelationToken> PostAsync(const std::string &method, const std::string& data, const std::function<void(const HttpResponse&)>& callback) = 0;
    virtual void Update() = 0;
};