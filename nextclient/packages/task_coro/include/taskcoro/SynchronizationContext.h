#pragma once
#include <functional>
#include <queue>
#include <mutex>

#include <concurrencpp/concurrencpp.h>

namespace taskcoro
{
    class SynchronizationContext
    {
        std::queue<std::function<void()>> callbacks_queue_;
        std::recursive_mutex mutex_;

    public:
        void RunCallbacks()
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

        template <class TReturn>
            requires !std::is_void_v<TReturn>
        concurrencpp::result<TReturn> Run(const std::function<TReturn()>& function)
        {
            std::shared_ptr<concurrencpp::result_promise<TReturn>> promise = std::make_shared<concurrencpp::result_promise<TReturn>>();

            {
                std::lock_guard lock_guard(mutex_);

                callbacks_queue_.emplace([function, promise]
                {
                    promise->set_result(function());
                });
            }

            std::thread::id invoke_thread_id = std::this_thread::get_id();

            auto result = co_await promise->get_result();

            co_await TaskCoro::TrySwitchToThread(invoke_thread_id);

            co_return result;
        }

        template<class TReturn>
            requires std::is_void_v<TReturn>
        concurrencpp::result<TReturn> Run(const std::function<TReturn()>& function)
        {
            std::shared_ptr<concurrencpp::result_promise<void>> promise = std::make_shared<concurrencpp::result_promise<void>>();

            {
                std::lock_guard lock_guard(mutex_);

                callbacks_queue_.emplace([function, promise]
                {
                    function();
                    promise->set_result();
                });
            }

            std::thread::id invoke_thread_id = std::this_thread::get_id();

            co_await promise->get_result();

            co_await TaskCoro::TrySwitchToThread(invoke_thread_id);
        }
    };
}
