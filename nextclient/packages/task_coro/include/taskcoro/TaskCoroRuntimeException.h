#pragma once
#include <stdexcept>

namespace taskcoro
{
    enum class TaskCoroRuntimeExceptionType
    {
        ExecutorNotInitialized,
        ExecutorShutdown,
    };

    class TaskCoroRuntimeException : public std::runtime_error
    {
        TaskCoroRuntimeExceptionType exception_type_;

    public:
        TaskCoroRuntimeException(TaskCoroRuntimeExceptionType exception_type) :
            exception_type_(exception_type),
            std::runtime_error(magic_enum::enum_name(exception_type).data())
        { }

        TaskCoroRuntimeExceptionType exception_type() const { return exception_type_; }
    };
}
