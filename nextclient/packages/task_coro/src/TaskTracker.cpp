#include <taskcoro/TaskTracker.h>
#include <atomic>
#include <chrono>
#include <mutex>
#include <taskcoro/TaskCoro.h>

using namespace taskcoro;
using namespace concurrencpp;
using namespace std::chrono_literals;

TaskTracker::Token::Token(TaskTracker* parent) :
    parent_(parent)
{
}

TaskTracker::Token::Token(Token&& other) noexcept :
    parent_(other.parent_)
{
    other.parent_ = nullptr;
}

TaskTracker::Token::~Token()
{
    if (parent_)
    {
        parent_->OnTaskFinished();
    }
}

TaskTracker::TaskTracker()
{
}

TaskTracker::Token TaskTracker::MakeToken()
{
    active_.fetch_add(1, std::memory_order_acq_rel);
    return Token(this);
}

result<void> TaskTracker::WaitAsync()
{
    std::shared_ptr<SynchronizationContext> caller_ctx = SynchronizationContext::Current();

    awaiters_.fetch_add(1, std::memory_order_acq_rel);

    bool should_set_result_now = false;
    {
        std::unique_lock lock(mutex_);

        if (!initialized_)
        {
            promise_ = result_promise<void>();
            shared_result_ = promise_.get_result();
            initialized_ = true;
        }

        waiter_registered_ = true;

        if (!result_set_ && (active_.load(std::memory_order_acquire) == 0 || pending_set_))
        {
            pending_set_ = false;
            result_set_ = true;
            should_set_result_now = true;
        }
    }

    if (should_set_result_now)
    {
        promise_.set_result();
    }

    auto my_result = shared_result_;

    try
    {
        co_await my_result;

        if (caller_ctx)
        {
            co_await caller_ctx->SwitchTo();
        }
    }
    catch (...)
    {
        awaiters_.fetch_sub(1, std::memory_order_acq_rel);
        throw;
    }

    awaiters_.fetch_sub(1, std::memory_order_acq_rel);
}

void TaskTracker::OnTaskFinished()
{
    size_t prev_active = active_.fetch_sub(1, std::memory_order_acq_rel);
    size_t active = prev_active - 1;

    if (active != 0)
    {
        return;
    }

    bool do_set_result = false;

    {
        std::unique_lock lock(mutex_);

        if (result_set_)
        {
            return;
        }

        if (!initialized_)
        {
            pending_set_ = true;
            return;
        }

        if (waiter_registered_)
        {
            result_set_ = true;
            do_set_result = true;
        }
        else
        {
            pending_set_ = true;
            return;
        }
    }

    if (do_set_result)
    {
        promise_.set_result();
    }
}
