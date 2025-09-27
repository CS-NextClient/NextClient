#include <taskcoro/impl/TaskCoroImpl.h>

#include <functional>

#include <magic_enum/magic_enum.hpp>

#include <taskcoro/impl/SynchronizationContextImpl.h>
#include <taskcoro/exceptions/OperationCanceledException.h>
#include <taskcoro/exceptions/TaskCoroRuntimeException.h>

using namespace taskcoro;

TaskCoroImpl::TaskCoroImpl(std::thread::id main_thread_id):
    main_thread_id_(main_thread_id)
{
    ccruntime_ = std::make_unique<concurrencpp::runtime>();
    update_executor_ = ccruntime_->make_manual_executor();
    thread_executor_ = ccruntime_->thread_executor();
    thread_pool_executor_ = ccruntime_->thread_pool_executor();
    thread_pool_io_executor_ = ccruntime_->background_executor();
}

TaskCoroImpl::~TaskCoroImpl()
{
    thread_pool_io_executor_->shutdown();
    thread_pool_executor_->shutdown();
    thread_executor_->shutdown();
    update_executor_->shutdown();
}

bool TaskCoroImpl::IsMainThread()
{
    return std::this_thread::get_id() == main_thread_id_;
}

concurrencpp::result<void> TaskCoroImpl::SwitchTo(TaskType task_type)
{
    if (task_type == TaskType::MainThread && IsMainThread())
    {
        co_return;
    }

    std::shared_ptr<concurrencpp::executor> executor = GetExecutorByTaskType(task_type);

    co_await resume_on(executor);
}

concurrencpp::result<void> TaskCoroImpl::Yield_()
{
    using namespace std::chrono_literals;

    if (IsMainThread())
    {
        co_await update_executor_->submit([]{ });
        co_return;
    }

    co_await WaitForMs(1ms);
}

concurrencpp::result<void> TaskCoroImpl::WaitForMs(std::chrono::milliseconds ms)
{
    std::shared_ptr<concurrencpp::executor> executor;

    if (IsMainThread())
    {
        executor = update_executor_;
    }
    else
    {
        executor = thread_pool_executor_;
    }

    co_await ccruntime_->timer_queue()->make_delay_object(ms, executor);
}

concurrencpp::result<void> TaskCoroImpl::WaitForNextFrame()
{
    bool invoked_from_main_thread = IsMainThread();

    co_await update_executor_->submit([]{ });

    if (!invoked_from_main_thread)
    {
        co_await SwitchTo(TaskType::ThreadPool);
    }
}

std::shared_ptr<concurrencpp::executor> TaskCoroImpl::GetExecutorByTaskType(TaskType task_type) const
{
    std::shared_ptr<concurrencpp::executor> executor;

    switch (task_type)
    {
    case TaskType::ThreadPool:
        executor = thread_pool_executor_;
        break;

    case TaskType::NewThread:
        executor = thread_executor_;
        break;

    case TaskType::MainThread:
        executor = update_executor_;
        break;

    case TaskType::IO:
        executor = thread_pool_io_executor_;
        break;

    default:
        throw std::invalid_argument(std::format("Invalid task type {}",  magic_enum::enum_name(task_type)));
    }

    return executor;
}

void TaskCoroImpl::RunTask(TaskType task_type, std::function<void()> task)
{
    std::shared_ptr<concurrencpp::executor> executor = GetExecutorByTaskType(task_type);

    if (executor == nullptr)
    {
        throw TaskCoroRuntimeException(TaskCoroRuntimeExceptionType::ExecutorNotInitialized);
    }

    if (executor->shutdown_requested())
    {
        throw TaskCoroRuntimeException(TaskCoroRuntimeExceptionType::ExecutorShutdown);
    }

    try
    {
        executor->submit(std::move(task));
    }
    catch (concurrencpp::errors::interrupted_task& e)
    {
        throw OperationCanceledException(e.what());
    }
}

std::shared_ptr<SynchronizationContextImplInterface> TaskCoroImpl::CreateSynchronizationContext(
    TaskType task_type,
    std::thread::id thread_id
)
{
    return std::make_shared<SynchronizationContextImpl>(weak_from_this(), task_type, thread_id);
}

void TaskCoroImpl::Update() const
{
    if (update_executor_ && !update_executor_->shutdown_requested())
    {
        update_executor_->loop_once();
    }
}
