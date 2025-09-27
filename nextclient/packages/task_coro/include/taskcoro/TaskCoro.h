#pragma once
#include <memory>
#include <chrono>

#include <concurrencpp/concurrencpp.h>

#include "TaskType.h"
#include "TaskCoroImplInterface.h"
#include "CancellationToken.h"
#include "ContinuationContextType.h"
#include "SynchronizationContext.h"
#include "traits.h"
#include "io/net.h"
#include "exceptions/TaskCoroRuntimeException.h"
#include "exceptions/OperationCanceledException.h"

namespace taskcoro
{
    class TaskCoro
    {
        static std::shared_ptr<TaskCoroImplInterface> task_impl_;

    public:
        static bool IsInitialized();

        static void Initialize(std::shared_ptr<TaskCoroImplInterface> task_run_impl);

        static void UnInitialize();

        template<class TCallable, class... TArgs>
        static concurrencpp::result<unwrap_result_t<std::invoke_result_t<TCallable&, TArgs...>>>
        RunInMainThread(TCallable&& callable, TArgs&&... arguments);

        template<class TCallable, class... TArgs>
        static concurrencpp::result<unwrap_result_t<std::invoke_result_t<TCallable&, TArgs...>>>
        RunInNewThread(TCallable&& callable, TArgs&&... arguments);

        template< class TCallable, class... TArgs>
        static concurrencpp::result<unwrap_result_t<std::invoke_result_t<TCallable&, TArgs...>>>
        RunInThreadPool(TCallable&& callable, TArgs&&... arguments);

        template<class TCallable, class... TArgs>
        static concurrencpp::result<unwrap_result_t<std::invoke_result_t<TCallable&, TArgs...>>>
        RunIO(TCallable&& callable, TArgs&&... arguments);

        template<class TCallable, class... TArgs>
        static concurrencpp::result<unwrap_result_t<std::invoke_result_t<TCallable&, TArgs...>>>
        RunTask(TaskType task_type, ContinuationContextType continuation_context, TCallable&& callable, TArgs&&... arguments);

        static concurrencpp::result<void> SwitchTo(TaskType task_type);

        static concurrencpp::result<void> SwitchToMainThread();

        static concurrencpp::result<void> SwitchToThreadPool();

        static bool IsMainThread();

        /**
         * Yields execution to allow other operations or tasks to proceed.
         *
         * When called from the main thread, it waits for the next Update, and execution continues in the main thread.
         * When called from another thread, it waits for 1 ms, and execution continues in the ThreadPool.
         *
         * This method never blocks the calling thread.
         */
        static concurrencpp::result<void> Yield_();

        /**
         * Suspends the execution for the specified amount of time in milliseconds.
         *
         * When called from the main thread, execution continues in the main thread.
         * When called from another thread, execution continues in the ThreadPool.
         *
         * This method never blocks the calling thread.
         */
        static concurrencpp::result<void> WaitForMs(std::chrono::milliseconds ms);

        /**
         * Suspends the execution of the current task until the next frame update.
         *
         * When called from the main thread, execution continues in the main thread.
         * When called from another thread, execution continues in the ThreadPool.
         *
         * This method never blocks the calling thread.
         */
        static concurrencpp::result<void> WaitForNextFrame();

        template <typename Range>
            requires std::ranges::range<Range>
        static concurrencpp::result<void> WhenAll(Range& range, bool supress_tasks_exceptions = false, std::shared_ptr<CancellationToken> cancellation_token = nullptr);

        template <typename Range>
            requires std::ranges::range<Range>
        static concurrencpp::result<std::ranges::range_difference_t<Range>>WhenAny(const Range& range, bool supress_tasks_exceptions = false, std::shared_ptr<CancellationToken> cancellation_token = nullptr);

        static concurrencpp::result<void> WaitWhile(std::function<bool()> condition, std::shared_ptr<CancellationToken> cancellation_token = nullptr);

        static concurrencpp::result<void> WaitUntil(std::function<bool()> condition, std::shared_ptr<CancellationToken> cancellation_token = nullptr);
    };
}

#include "TaskCoro.tpp"
