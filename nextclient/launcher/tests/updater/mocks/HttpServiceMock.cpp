#include "HttpServiceMock.h"

HttpServiceMock::HttpServiceMock(std::unordered_map<std::string, HttpResponse> method_response_map) :
        method_response_map_(std::move(method_response_map))
{
}

HttpResponse HttpServiceMock::Post(const std::string& method, const std::string& data)
{
    if (method_response_map_.contains(method))
        return method_response_map_[method];

    return {};
}
