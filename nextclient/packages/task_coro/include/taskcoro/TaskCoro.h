#pragma once
#include <memory>

#include <concurrencpp/concurrencpp.h>

#include "TaskType.h"
#include "TaskCoroImpl.h"

namespace taskcoro
{
    class TaskCoro
    {
        static std::shared_ptr<TaskCoroImpl> task_impl_;

    public:
        static bool IsInitialized() { return task_impl_ != nullptr; }

        static void Initialize(std::shared_ptr<TaskCoroImpl> task_run_impl)
        {
            task_impl_ = task_run_impl;
        }

        static void UnInitialize()
        {
            task_impl_ = nullptr;
        }

        template<class TRet, class TCallable, class... TArgs>
        static concurrencpp::result<TRet> RunInMainThread(TCallable&& callable, TArgs&&... arguments)
        {
            return task_impl_->RunTask<TRet>(TaskType::MainThread, false, std::forward<TCallable>(callable), std::forward<TArgs>(arguments)...);
        }

        template<class TRet, class TCallable, class... TArgs>
        static concurrencpp::result<TRet> RunInNewThread(TCallable&& callable, TArgs&&... arguments)
        {
            return task_impl_->RunTask<TRet>(TaskType::NewThread, true, std::forward<TCallable>(callable), std::forward<TArgs>(arguments)...);
        }

        template<class TRet, class TCallable, class... TArgs>
        static concurrencpp::result<TRet> Run(TCallable&& callable, TArgs&&... arguments)
        {
            return task_impl_->RunTask<TRet>(TaskType::Regular, false, std::forward<TCallable>(callable), std::forward<TArgs>(arguments)...);
        }

        template<class TRet, class TCallable, class... TArgs>
        static concurrencpp::result<TRet> RunIO(TCallable&& callable, TArgs&&... arguments)
        {
            return task_impl_->RunTask<TRet>(TaskType::IO, true, std::forward<TCallable>(callable), std::forward<TArgs>(arguments)...);
        }

        template <typename Range>
            requires std::ranges::range<Range>
        static concurrencpp::result<void> WhenAll(Range& range, std::shared_ptr<CancellationToken> cancellation_token = nullptr)
        {
            if (std::ranges::empty(range))
            {
                co_return;
            }

            while (true)
            {
                if (cancellation_token)
                {
                    cancellation_token->ThrowIfCancelled();
                }

                bool all_done = true;

                for (auto it = range.begin(); it != range.end(); ++it)
                {
                    if ((*it).status() == concurrencpp::result_status::exception)
                    {
                        // An internal exception will be thrown when get() is called
                        (*it).get();
                    }

                    if ((*it).status() == concurrencpp::result_status::idle)
                    {
                        all_done = false;
                        break;
                    }
                }

                if (all_done)
                {
                    co_return;
                }

                co_await Yield_();
            }
        }

        template <typename Range>
            requires std::ranges::range<Range>
        static concurrencpp::result<std::ranges::range_difference_t<Range>> WhenAny(const Range& range, std::shared_ptr<CancellationToken> cancellation_token = nullptr)
        {
            if (std::ranges::empty(range))
            {
                throw std::runtime_error("Range is empty");
            }

            while (true)
            {
                if (cancellation_token)
                {
                    cancellation_token->ThrowIfCancelled();
                }

                for (auto it = range.begin(); it != range.end(); ++it)
                {
                    if ((*it).status() == concurrencpp::result_status::exception)
                    {
                        // An internal exception will be thrown when get() is called
                        (*it).get();
                    }

                    if ((*it).status() == concurrencpp::result_status::value)
                    {
                        co_return std::ranges::distance(range.begin(), it);
                    }
                }

                co_await Yield_();
            }
        }

        static concurrencpp::result<void> TrySwitchToThread(std::thread::id thread_id)
        {
            return task_impl_->TrySwitchToThread(thread_id);
        }

        static concurrencpp::result<void> TrySwitchToMainThread()
        {
            return task_impl_->TrySwitchToMainThread();
        }

        static concurrencpp::result<void> WaitWhile(std::function<bool()> condition, std::shared_ptr<CancellationToken> cancellation_token = nullptr)
        {
            while (condition())
            {
                if (cancellation_token)
                {
                    cancellation_token->ThrowIfCancelled();
                }

                co_await task_impl_->Yield_();
            }
        }

        static concurrencpp::result<void> WaitUntil(std::function<bool()> condition, std::shared_ptr<CancellationToken> cancellation_token = nullptr)
        {
            while (!condition())
            {
                if (cancellation_token)
                {
                    cancellation_token->ThrowIfCancelled();
                }

                co_await task_impl_->Yield_();
            }
        }

        static concurrencpp::result<void> Yield_()
        {
            co_await task_impl_->Yield_();
        }
    };
}

#include "CancellationToken.h"
#include "SynchronizationContext.h"
#include "TaskCoroRuntimeException.h"
#include "OperationCanceledException.h"
#include "io/net.h"
