#pragma once
#include <thread>

#include <taskcoro/SynchronizationContextImplInterface.h>
#include <taskcoro/impl/TaskCoroImpl.h>
#include <taskcoro/TaskType.h>

namespace taskcoro
{
    class SynchronizationContextImpl : public SynchronizationContextImplInterface
    {
        std::weak_ptr<TaskCoroImpl> task_coro_impl_;
        TaskType task_type_{};
        std::thread::id thread_id_{};

    public:
        explicit SynchronizationContextImpl(
            std::weak_ptr<TaskCoroImpl> task_coro_impl,
            TaskType task_type,
            std::thread::id thread_id = {}
        );

        void RunTask(std::function<void()> task) override;
    };
}