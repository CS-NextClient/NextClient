#pragma once
#include <stdexcept>

namespace taskcoro
{
    class OperationCanceledException : public std::runtime_error
    {
    public:
        OperationCanceledException(const std::string& message) :
            std::runtime_error(message)
        { }
    };
}
