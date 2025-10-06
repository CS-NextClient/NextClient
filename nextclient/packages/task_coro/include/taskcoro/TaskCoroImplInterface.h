#pragma once
#include <thread>
#include <chrono>
#include <functional>

#include <concurrencpp/concurrencpp.h>

#include "CancellationToken.h"
#include "TaskType.h"
#include "SynchronizationContextImplInterface.h"

namespace taskcoro
{
    class TaskCoroImplInterface
    {
    public:
        virtual ~TaskCoroImplInterface() = default;

        virtual bool IsMainThread() = 0;
        virtual concurrencpp::result<void> SwitchTo(TaskType task_type) = 0;
        virtual concurrencpp::result<void> Yield_() = 0;
        virtual concurrencpp::result<void> WaitForMs(std::chrono::milliseconds ms) = 0;
        virtual concurrencpp::result<void> WaitForNextFrame() = 0;
        virtual void RunTask(TaskType task_type, std::function<void()> task) = 0;
        virtual std::shared_ptr<SynchronizationContextImplInterface> CreateSynchronizationContext(TaskType task_type, std::thread::id thread_id) = 0;
    };
}