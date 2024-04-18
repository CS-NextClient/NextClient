#pragma once

#include <functional>
#include <queue>
#include <mutex>

class SynchronizationContext
{
    std::queue<std::function<void()>> callbacks_queue_;
    std::mutex mutex_;

public:
    void RunCallbacks()
    {
        std::lock_guard<std::mutex> guard(mutex_);

        while (!callbacks_queue_.empty())
        {
            callbacks_queue_.front()();
            callbacks_queue_.pop();
        }
    }

    void Execute(const std::function<void()>& function)
    {
        std::lock_guard<std::mutex> guard(mutex_);
        callbacks_queue_.emplace(function);
    }
};

