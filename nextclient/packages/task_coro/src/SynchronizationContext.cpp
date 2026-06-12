#include <taskcoro/SynchronizationContext.h>

#include <coroutine>
#include <memory>
#include <utility>

#include <taskcoro/TaskCoro.h>

using namespace taskcoro;
using namespace concurrencpp;

thread_local std::shared_ptr<SynchronizationContext> SynchronizationContext::current_;

namespace
{
    // Suspends unconditionally before posting the handle, so the target thread cannot
    // complete the switch before this coroutine is suspended; otherwise the continuation
    // would keep running on the source thread (lost switch).
    struct SwitchToAwaiter
    {
        std::shared_ptr<SynchronizationContextImplInterface> sync_ctx_impl;

        bool await_ready() const noexcept
        {
            return false;
        }

        void await_suspend(std::coroutine_handle<> handle) const
        {
            sync_ctx_impl->RunTask([handle] { handle.resume(); });
        }

        void await_resume() const noexcept {}
    };
} // namespace

SynchronizationContext::SynchronizationContext(
    std::shared_ptr<SynchronizationContextImplInterface> sync_ctx_impl
) :
    sync_ctx_impl_(std::move(sync_ctx_impl))
{
}

result<void> SynchronizationContext::SwitchTo()
{
    if (current_ != nullptr && current_->sync_ctx_impl_ == sync_ctx_impl_)
    {
        co_return;
    }

    co_await SwitchToAwaiter{sync_ctx_impl_};
}

std::shared_ptr<SynchronizationContext> SynchronizationContext::Current()
{
    return current_;
}

void SynchronizationContext::SetCurrent(std::shared_ptr<SynchronizationContext> sync_ctx)
{
    current_ = std::move(sync_ctx);
}
