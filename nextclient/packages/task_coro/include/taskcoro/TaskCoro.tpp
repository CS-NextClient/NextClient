#pragma once
#include <concurrencpp/concurrencpp.h>

namespace taskcoro
{
    template<class TCallable, class ... TArgs>
    concurrencpp::result<unwrap_result_t<std::invoke_result_t<TCallable&, TArgs...>>>
    TaskCoro::RunInMainThread(TCallable&& callable, TArgs&&... arguments)
    {
        assert(task_impl_ != nullptr);

        return RunTask(TaskType::MainThread, ContinuationContextType::Caller, std::forward<TCallable>(callable), std::forward<TArgs>(arguments)...);
    }

    template<class TCallable, class ... TArgs>
    concurrencpp::result<unwrap_result_t<std::invoke_result_t<TCallable&, TArgs...>>>
    TaskCoro::RunInNewThread(TCallable&& callable, TArgs&&... arguments)
    {
        assert(task_impl_ != nullptr);

        return RunTask(TaskType::NewThread, ContinuationContextType::Caller, std::forward<TCallable>(callable), std::forward<TArgs>(arguments)...);
    }

    template<class TCallable, class ... TArgs>
    concurrencpp::result<unwrap_result_t<std::invoke_result_t<TCallable&, TArgs...>>>
    TaskCoro::RunInThreadPool(TCallable&& callable, TArgs&&... arguments)
    {
        assert(task_impl_ != nullptr);

        return RunTask(TaskType::ThreadPool, ContinuationContextType::Caller, std::forward<TCallable>(callable), std::forward<TArgs>(arguments)...);
    }

    template<class TCallable, class ... TArgs>
    concurrencpp::result<unwrap_result_t<std::invoke_result_t<TCallable&, TArgs...>>>
    TaskCoro::RunIO(TCallable&& callable, TArgs&&... arguments)
    {
        assert(task_impl_ != nullptr);

        return RunTask(TaskType::IO, ContinuationContextType::Caller, std::forward<TCallable>(callable), std::forward<TArgs>(arguments)...);
    }

    template<class TCallable, class... TArgs>
    concurrencpp::result<unwrap_result_t<std::invoke_result_t<TCallable&, TArgs...>>>
    TaskCoro::RunTask(TaskType task_type, ContinuationContextType continuation_context, TCallable&& callable, TArgs&&... arguments)
    {
        assert(task_impl_ != nullptr);

        using TResult    = std::invoke_result_t<TCallable&, TArgs...>;
        using TUnwrapped = unwrap_result_t<TResult>;
        constexpr bool is_coro_result = unwrap_result<TResult>::is;

        auto result_promise = std::make_shared<concurrencpp::result_promise<TResult>>();

        auto bound_call = [task_type,
                           func = std::decay_t<TCallable>(std::forward<TCallable>(callable)),
                           ...xs = std::unwrap_ref_decay_t<TArgs>(std::forward<TArgs>(arguments))]
        () mutable -> TResult
        {
            if (SynchronizationContext::Current() == nullptr && task_type != TaskType::NewThread)
            {
                auto ctx_impl = task_impl_->CreateSynchronizationContext(task_type, std::this_thread::get_id());
                SynchronizationContext::SetCurrent(std::make_shared<SynchronizationContext>(ctx_impl));
            }

            if constexpr (std::is_void_v<TResult>)
            {
                std::invoke(std::move(func), std::forward_like<TArgs>(xs)...);
                return;
            }
            else
            {
                return std::invoke(std::move(func), std::forward_like<TArgs>(xs)...);
            }
        };

        auto task_lambda = [result_promise, bound = std::move(bound_call)]() mutable
        {
            try
            {
                if constexpr (std::is_void_v<TResult>)
                {
                    bound();
                    result_promise->set_result();
                }
                else
                {
                    result_promise->set_result(bound());
                }
            }
            catch (...)
            {
                result_promise->set_exception(std::current_exception());
            }
        };

        auto caller_context = SynchronizationContext::Current();
        auto task = result_promise->get_result();

        task_impl_->RunTask(task_type, std::move(task_lambda));

        if constexpr (std::is_void_v<TResult>)
        {
            if (caller_context && continuation_context == ContinuationContextType::Caller)
            {
                co_await caller_context->SwitchTo();
            }

            co_await task;
            co_return;
        }
        else
        {
            TResult result = co_await task;

            if (caller_context && continuation_context == ContinuationContextType::Caller)
            {
                co_await caller_context->SwitchTo();
            }

            if constexpr (is_coro_result)
            {
                if constexpr (std::is_void_v<TUnwrapped>)
                {
                    co_await std::move(result);
                    co_return;
                }
                else
                {
                    auto value = co_await std::move(result);
                    co_return value;
                }
            }
            else
            {
                co_return result;
            }
        }
    }

    template <typename Range> requires std::ranges::range<Range>
    concurrencpp::result<void> TaskCoro::WhenAll(Range& range, bool supress_tasks_exceptions, std::shared_ptr<CancellationToken> cancellation_token)
    {
        assert(task_impl_ != nullptr);

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
                    if (supress_tasks_exceptions)
                    {
                        continue;
                    }

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

            co_await task_impl_->Yield_();
        }
    }

    template <typename Range> requires std::ranges::range<Range>
    concurrencpp::result<std::ranges::range_difference_t<Range>> TaskCoro::WhenAny(const Range& range,
    bool supress_tasks_exceptions,
    std::shared_ptr<CancellationToken> cancellation_token)
    {
        assert(task_impl_ != nullptr);

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
                    if (supress_tasks_exceptions)
                    {
                        co_return std::ranges::distance(range.begin(), it);
                    }

                    (*it).get();
                }

                if ((*it).status() == concurrencpp::result_status::value)
                {
                    co_return std::ranges::distance(range.begin(), it);
                }
            }

            co_await task_impl_->Yield_();
        }
    }
}