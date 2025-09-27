#include <taskcoro/impl/SynchronizationContextImpl.h>

#include <utility>
#include <taskcoro/exceptions/TaskCoroRuntimeException.h>

using namespace taskcoro;

SynchronizationContextImpl::SynchronizationContextImpl(
    std::weak_ptr<TaskCoroImpl> task_coro_impl,
    TaskType task_type,
    std::thread::id thread_id
) :
    task_coro_impl_(std::move(task_coro_impl)),
    task_type_(task_type),
    thread_id_(thread_id)
{
}

void SynchronizationContextImpl::RunTask(std::function<void()> task)
{
    if (task_type_ == TaskType::NewThread)
    {
        throw TaskCoroRuntimeException("task_type_ == TaskType::NewThread");
    }

    std::shared_ptr<TaskCoroImpl> impl = task_coro_impl_.lock();
    if (!impl)
    {
        throw TaskCoroRuntimeException("task_coro_impl_.expired()");
    }

    impl->RunTask(task_type_, std::move(task));
}
