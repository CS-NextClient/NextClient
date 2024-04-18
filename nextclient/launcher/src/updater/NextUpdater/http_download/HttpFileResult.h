#pragma once

#include <string>
#include <utility>
#include "HttpFileRequest.h"

class HttpFileResult
{
    HttpFileRequest request_;
    std::string error_;
    std::vector<uint8_t> data_;

public:
    explicit HttpFileResult(HttpFileRequest request, std::string error) :
            request_(std::move(request)),
            error_(std::move(error))
    { }

    explicit HttpFileResult(HttpFileRequest request, std::vector<uint8_t>&& data) :
            request_(std::move(request)),
            data_(std::move(data))
    { }

    [[nodiscard]] bool has_error() const { return !error_.empty(); }
    [[nodiscard]] const std::string& get_error() const { return error_; }
    [[nodiscard]] const std::vector<uint8_t>& get_data() const { return data_; }
    [[nodiscard]] const HttpFileRequest& get_request() const { return request_; }
};