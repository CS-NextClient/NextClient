#pragma once
#include <stdexcept>

namespace taskcoro
{
    class OperationTimeoutException : public std::runtime_error
    {
    public:
        OperationTimeoutException(const std::string& message) :
            std::runtime_error(message)
        { }
    };
}
