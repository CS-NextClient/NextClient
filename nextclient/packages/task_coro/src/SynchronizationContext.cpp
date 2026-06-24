#include <taskcoro/SynchronizationContext.h>

#include <coroutine>
#include <memory>
#include <utility>

#include <taskcoro/TaskCoro.h>

using namespace taskcoro;
using namespace concurrencpp;

thread_local std::shared_ptr<SynchronizationContext> SynchronizationContext::current_;

SynchronizationContext::SynchronizationContext(
    std::shared_ptr<SynchronizationContextImplInterface> sync_ctx_impl
) :
    sync_ctx_impl_(std::move(sync_ctx_impl))
{
}

SwitchToAwaiter SynchronizationContext::SwitchTo()
{
    if (current_ != nullptr && current_->sync_ctx_impl_ == sync_ctx_impl_)
    {
        return SwitchToAwaiter{nullptr};
    }

    return SwitchToAwaiter{sync_ctx_impl_};
}

std::shared_ptr<SynchronizationContext> SynchronizationContext::Current()
{
    return current_;
}

void SynchronizationContext::SetCurrent(std::shared_ptr<SynchronizationContext> sync_ctx)
{
    current_ = std::move(sync_ctx);
}
