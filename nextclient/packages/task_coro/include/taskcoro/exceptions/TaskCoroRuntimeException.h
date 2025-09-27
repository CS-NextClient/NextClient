#pragma once
#include <stdexcept>

#include <magic_enum/magic_enum.hpp>

namespace taskcoro
{
    enum class TaskCoroRuntimeExceptionType
    {
        ExecutorNotInitialized,
        ExecutorShutdown,
        NotMainThread,
        Other
    };

    class TaskCoroRuntimeException : public std::runtime_error
    {
        TaskCoroRuntimeExceptionType exception_type_;

    public:
        TaskCoroRuntimeException(TaskCoroRuntimeExceptionType exception_type) :
            exception_type_(exception_type),
            std::runtime_error(magic_enum::enum_name(exception_type).data())
        { }

        TaskCoroRuntimeException(const std::string& text) :
            exception_type_(TaskCoroRuntimeExceptionType::Other),
            std::runtime_error(text)
        { }

        TaskCoroRuntimeExceptionType exception_type() const { return exception_type_; }
    };
}
