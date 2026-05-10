#pragma once
#include "updater_gui_app/HttpServiceInterface.h"

class HttpServiceMock : public HttpServiceInterface
{
    std::unordered_map<std::string, HttpResponse> method_response_map_;

public:
    explicit HttpServiceMock(std::unordered_map<std::string, HttpResponse> method_response_map) :
        method_response_map_(std::move(method_response_map))
    {}

    concurrencpp::result<HttpResponse> PostAsync(
        std::string method,
        std::string data,
        std::shared_ptr<taskcoro::CancellationToken> cancellation_token
    ) override
    {
        if (method_response_map_.contains(method))
        {
            co_return method_response_map_[method];
        }

        co_return HttpResponse{};
    }

    concurrencpp::result<void> ShutdownAsync() override
    {
        co_return;
    }
};
