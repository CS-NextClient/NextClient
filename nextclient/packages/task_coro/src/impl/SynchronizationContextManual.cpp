#include <taskcoro/impl/SynchronizationContextManual.h>

using namespace taskcoro;

void SynchronizationContextManual::RunTask(std::function<void()> task)
{
    std::scoped_lock lock(mutex_);

    callbacks_queue_.emplace(task);
}

void SynchronizationContextManual::Update()
{
    std::lock_guard lock_guard(mutex_);

    std::queue<std::function<void()>> callbacks_queue;
    callbacks_queue_.swap(callbacks_queue);

    while (!callbacks_queue.empty())
    {
        auto& callback = callbacks_queue.front();
        callback();

        callbacks_queue.pop();
    }
}
