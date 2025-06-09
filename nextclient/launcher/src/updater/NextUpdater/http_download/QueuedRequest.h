#pragma once

#include <string>
#include <utility>
#include "HttpFileRequest.h"

class QueuedRequest
{
    HttpFileRequest request_;
    int retry_;

public:
    QueuedRequest(HttpFileRequest request, int retry) :
            request_(std::move(request)),
            retry_(retry)
    { }

    [[nodiscard]] const HttpFileRequest& get_request() const { return request_; }
    [[nodiscard]] int get_retry() const { return retry_; }
    void set_retry(int retry) { retry_ = retry; }
};