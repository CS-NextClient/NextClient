#pragma once
#include <string>
#include <vector>
#include <taskcoro/TaskCoro.h>

namespace next_launcher
{
    class IBackendAddressResolver
    {
    public:
        virtual ~IBackendAddressResolver() = default;

        virtual concurrencpp::result<std::string> GetBackendAddressAsync(
            std::shared_ptr<taskcoro::CancellationToken> cancellation_token) = 0;
    };
}
