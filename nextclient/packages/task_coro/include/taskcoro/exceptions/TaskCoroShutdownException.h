#pragma once
#include <stdexcept>

namespace taskcoro
{
    class TaskCoroShutdownException : public std::runtime_error
    {
    public:
        TaskCoroShutdownException() :
            std::runtime_error("TaskCoro is shutting down")
        { }

        TaskCoroShutdownException(const std::string& message) :
            std::runtime_error(message)
        { }
    };
}
