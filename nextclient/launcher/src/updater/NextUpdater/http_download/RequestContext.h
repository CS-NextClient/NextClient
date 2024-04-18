#pragma once

#include <string>
#include <queue>
#include <future>
#include <chrono>
#include <utility>
#include <cpr/api.h>

class RequestContext
{
public:
    using time_point = std::chrono::time_point<std::chrono::system_clock>;

    struct Shared
    {
        std::atomic_uint32_t download_total;
        std::atomic_uint32_t download_now;
        std::atomic_bool stop_download = false;
    };

private:
    HttpFileRequest request_;
    int retry_{};
    time_point start_time_;
    cpr::AsyncResponse response_;
    std::shared_ptr<Shared> shared_data_;

public:
    RequestContext(HttpFileRequest request, int retry, time_point start_time, std::shared_ptr<Shared> shared_data, cpr::AsyncResponse &&response):
        request_(std::move(request)),
        retry_(retry),
        start_time_(start_time),
        shared_data_(std::move(shared_data)),
        response_(std::move(response))
    { }

    RequestContext(const RequestContext &) = delete;
    RequestContext &operator=(const RequestContext &) = delete;

    RequestContext(RequestContext &&other) noexcept :
        request_(std::move(other.request_)),
        retry_(other.retry_),
        start_time_(other.start_time_),
        response_(std::move(other.response_)),
        shared_data_(std::move(other.shared_data_))
    { }

    RequestContext &operator=(RequestContext &&other) noexcept
    {
        request_ = std::move(other.request_);
        retry_ = other.retry_;
        start_time_ = other.start_time_;
        response_ = std::move(other.response_);
        shared_data_ = std::move(other.shared_data_);

        return *this;
    }

    [[nodiscard]] time_point get_start_time() const { return start_time_; }
    cpr::AsyncResponse& get_response() { return response_; }
    [[nodiscard]] const HttpFileRequest& get_request() const { return request_; }
    [[nodiscard]] std::shared_ptr<Shared> get_shared_data() const { return shared_data_; }
    [[nodiscard]] int get_retry() const { return retry_; }
    void set_retry(int retry) { retry_ = retry; }
};
