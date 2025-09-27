#pragma once
#include <functional>

namespace taskcoro
{
    class SynchronizationContextImplInterface
    {
    public:
        virtual ~SynchronizationContextImplInterface() = default;

        virtual void RunTask(std::function<void()> task) = 0;
    };
}