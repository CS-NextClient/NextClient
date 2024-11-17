#pragma once

#include <ResourceDescriptor.h>
#include <cpr/api.h>
#include <string>
#include <queue>
#include <future>
#include <chrono>
#include <utility>

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
    ResourceDescriptor resource_descriptor;
    int retry_{};
    time_point start_time_;
    cpr::AsyncResponse response_;
    std::shared_ptr<Shared> shared_data_;

public:
    RequestContext(ResourceDescriptor file_resource, int retry, time_point start_time, std::shared_ptr<Shared> shared_data, cpr::AsyncResponse &&response):
            resource_descriptor(std::move(file_resource)),
            retry_(retry),
            start_time_(start_time),
            shared_data_(std::move(shared_data)),
            response_(std::move(response))
    { }

    RequestContext(const RequestContext &) = delete;
    RequestContext &operator=(const RequestContext &) = delete;

    RequestContext(RequestContext &&other) noexcept :
            resource_descriptor(std::move(other.resource_descriptor)),
            retry_(other.retry_),
            start_time_(other.start_time_),
            response_(std::move(other.response_)),
            shared_data_(std::move(other.shared_data_))
    { }

    RequestContext &operator=(RequestContext &&other) noexcept
    {
        resource_descriptor = std::move(other.resource_descriptor);
        retry_ = other.retry_;
        start_time_ = other.start_time_;
        response_ = std::move(other.response_);
        shared_data_ = std::move(other.shared_data_);

        return *this;
    }

    [[nodiscard]] time_point get_start_time() const { return start_time_; }
    cpr::AsyncResponse &get_response() { return response_; }
    [[nodiscard]] const ResourceDescriptor& get_file_resource() const { return resource_descriptor; }
    [[nodiscard]] std::shared_ptr<Shared> get_shared_data() const { return shared_data_; }
    [[nodiscard]] int get_retry() const { return retry_; }
    void set_retry(int retry) { retry_ = retry; }
};