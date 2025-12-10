#pragma once
#include <memory>
#include <chrono>
#include <variant>

#include <concurrencpp/concurrencpp.h>

#include "TaskType.h"
#include "TaskCoroImplInterface.h"
#include "CancellationToken.h"
#include "LinkedCancellationToken.h"
#include "ContinuationContextType.h"
#include "SynchronizationContext.h"
#include "TaskTracker.h"
#include "traits.h"
#include "concepts.h"
#include "io/net.h"
#include "exceptions/TaskCoroRuntimeException.h"
#include "exceptions/OperationCanceledException.h"
#include "exceptions/OperationTimeoutException.h"

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
         * @brief Yields execution to allow other tasks to proceed.
         *
         * Resumes in the caller synchronization context.
         *
         * From the main thread, resumption occurs on the next Update.
         * From a worker thread, the coroutine pauses for ≈1 ms and then resumes
         * in the same worker context.
         *
         * The calling thread is not blocked; it is returned to the scheduler
         * for the duration of the suspension.
         *
         * @return concurrencpp::result<void>
         */
        static concurrencpp::result<void> Yield_();

        /**
         * @brief Suspends the coroutine for the specified duration.
         *
         * Resumes in the caller synchronization context.
         *
         * The calling thread is not blocked; it is returned to the scheduler
         * for the duration of the suspension.
         *
         * @param ms Suspension duration.
         * @param cancellation_token Optional cancellation token.
         *
         * @return concurrencpp::result<void>
         */
        static concurrencpp::result<void> WaitForMs(
            std::chrono::milliseconds ms,
            std::shared_ptr<CancellationToken> cancellation_token = nullptr);


        /**
         * @brief Suspends execution until the next frame update.
         *
         * Resumes in the original synchronization context.
         *
         * @return concurrencpp::result<void>
         */
        static concurrencpp::result<void> WaitForNextFrame();

        template <typename Range>
            requires std::ranges::range<Range>
        static concurrencpp::result<void> WhenAll(
            Range& range,
            bool supress_tasks_exceptions = false,
            std::shared_ptr<CancellationToken> cancellation_token = nullptr
        );

        template <typename Range>
            requires std::ranges::range<Range>
        static concurrencpp::result<std::ranges::range_difference_t<Range>>WhenAny(
            Range& range,
            bool supress_tasks_exceptions = false,
            std::shared_ptr<CancellationToken> cancellation_token = nullptr,
            std::chrono::milliseconds timeout = std::chrono::milliseconds{0}
        );

        static concurrencpp::result<void> WaitWhile(
            std::function<bool()> condition,
            std::shared_ptr<CancellationToken> cancellation_token = nullptr
        );

        static concurrencpp::result<void> WaitUntil(
            std::function<bool()> condition,
            std::shared_ptr<CancellationToken> cancellation_token = nullptr
        );

        /**
         * @brief Wraps an existing task with external cancellation monitoring.
         *
         * The function returns a coroutine that completes with an
         * `OperationCanceledException` if the provided `cancellation_token`
         * is signaled during the awaiting period.
         *
         * It must be noted that the underlying `task` is not canceled.
         * The cancellation affects only the awaiting wrapper. The original
         * `concurrencpp::shared_result` continues its execution regardless of the
         * cancellation request, as `concurrencpp::result` does not support
         * cooperative or forced task termination.
         *
         * @tparam TResult The result type produced by the underlying task.
         * @param task The original `concurrencpp::shared_result` to be awaited.
         * @param cancellation_token The token used to signal cancellation
         *        of the await operation.
         *
         * @return `concurrencpp::result<TResult>` — the wrapped result.
         *         If cancellation is requested, the coroutine throws
         *         `OperationCanceledException`.
         */
        template<ResultLike TTask>
        static auto WithCancellation(
            TTask task,
            std::shared_ptr<CancellationToken> cancellation_token
        ) -> concurrencpp::result<std::decay_t<decltype(task.get())>>;

        template<ResultLike TTask>
        static auto WithTimeout(
            TTask task,
            std::chrono::milliseconds timeout,
            std::shared_ptr<CancellationToken> cancellation_token = nullptr
        ) -> concurrencpp::result<std::decay_t<decltype(task.get())>>;

        template<ResultLike TTask>
        static auto SupressException(
            TTask task
        ) -> concurrencpp::result<std::variant<std::decay_t<decltype(task.get())>, std::exception>>;
    };
}

#include "TaskCoro.tpp"
