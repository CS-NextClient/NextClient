#include <taskcoro/SynchronizationContext.h>

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

result<void> SynchronizationContext::SwitchTo()
{
    if (current_ != nullptr && current_->sync_ctx_impl_ == sync_ctx_impl_)
    {
        co_return;
    }

    result_promise<void> promise;
    result<void> task = promise.get_result();

    sync_ctx_impl_->RunTask([&promise]
    {
        promise.set_result();
    });

    co_await task;
}

std::shared_ptr<SynchronizationContext> SynchronizationContext::Current()
{
    return current_;
}

void SynchronizationContext::SetCurrent(std::shared_ptr<SynchronizationContext> sync_ctx)
{
    current_ = std::move(sync_ctx);
}
