#include <taskcoro/TaskCoro.h>

#include <memory>

using namespace taskcoro;

std::shared_ptr<TaskCoroImplInterface> TaskCoro::task_impl_;

bool TaskCoro::IsInitialized()
{
    return task_impl_ != nullptr;
}

void TaskCoro::Initialize(std::shared_ptr<TaskCoroImplInterface> task_run_impl)
{
    task_impl_ = std::move(task_run_impl);

    auto ctx_impl = task_impl_->CreateSynchronizationContext(TaskType::MainThread, std::this_thread::get_id());
    SynchronizationContext::SetCurrent(std::make_shared<SynchronizationContext>(ctx_impl));
}

void TaskCoro::UnInitialize()
{
    task_impl_ = nullptr;
}

concurrencpp::result<void> TaskCoro::SwitchTo(TaskType task_type)
{
    assert(task_impl_ != nullptr);

    return task_impl_->SwitchTo(task_type);
}

concurrencpp::result<void> TaskCoro::SwitchToMainThread()
{
    assert(task_impl_ != nullptr);

    return task_impl_->SwitchTo(TaskType::MainThread);
}

concurrencpp::result<void> TaskCoro::SwitchToThreadPool()
{
    assert(task_impl_ != nullptr);

    return task_impl_->SwitchTo(TaskType::ThreadPool);
}

bool TaskCoro::IsMainThread()
{
    assert(task_impl_ != nullptr);

    return task_impl_->IsMainThread();
}

concurrencpp::result<void> TaskCoro::Yield_()
{
    assert(task_impl_ != nullptr);

    auto caller_ctx = SynchronizationContext::Current();

    co_await task_impl_->Yield_();

    if (caller_ctx)
    {
        co_await caller_ctx->SwitchTo();
    }
}

concurrencpp::result<void> TaskCoro::WaitForMs(std::chrono::milliseconds ms, std::shared_ptr<CancellationToken> cancellation_token)
{
    assert(task_impl_ != nullptr);

    auto caller_ctx = SynchronizationContext::Current();

    co_await task_impl_->WaitForMs(ms);

    if (caller_ctx)
    {
        co_await caller_ctx->SwitchTo();
    }

    if (cancellation_token)
    {
        cancellation_token->ThrowIfCancelled();
    }
}

concurrencpp::result<void> TaskCoro::WaitForNextFrame()
{
    assert(task_impl_ != nullptr);

    auto caller_ctx = SynchronizationContext::Current();

    co_await task_impl_->WaitForNextFrame();

    if (caller_ctx)
    {
        co_await caller_ctx->SwitchTo();
    }
}

concurrencpp::result<void> TaskCoro::WaitWhile(std::function<bool()> condition,
                                               std::shared_ptr<CancellationToken> cancellation_token)
{
    assert(task_impl_ != nullptr);

    while (condition())
    {
        if (cancellation_token)
        {
            cancellation_token->ThrowIfCancelled();
        }

        co_await Yield_();
    }
}

concurrencpp::result<void> TaskCoro::WaitUntil(std::function<bool()> condition,
                                               std::shared_ptr<CancellationToken> cancellation_token)
{
    assert(task_impl_ != nullptr);

    while (!condition())
    {
        if (cancellation_token)
        {
            cancellation_token->ThrowIfCancelled();
        }

        co_await Yield_();
    }
}
