#pragma once

#include <resource/ResourceDescriptor.h>
#include <string>
#include <utility>

class QueuedRequest
{
    ResourceDescriptor file_resource_;
    int retry_;

public:
    QueuedRequest(ResourceDescriptor file_resource, int retry) :
        file_resource_(std::move(file_resource)),
        retry_(retry)
    { }

    [[nodiscard]] const ResourceDescriptor& get_file_resource() const { return file_resource_; }
    [[nodiscard]] int get_retry() const { return retry_; }
    void set_retry(int retry) { retry_ = retry; }
};