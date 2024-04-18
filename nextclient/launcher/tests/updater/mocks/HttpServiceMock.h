#pragma once
#include <updater/HttpServiceInterface.h>

class HttpServiceMock : public HttpServiceInterface
{
    std::unordered_map<std::string, HttpResponse> method_response_map_;

public:
    explicit HttpServiceMock(std::unordered_map<std::string, HttpResponse> method_response_map);

    HttpResponse Post(const std::string& method, const std::string& data) override;
};
