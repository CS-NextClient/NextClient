#pragma once
#include <concurrencpp/concurrencpp.h>
#include <magic_enum.hpp>

#include "TaskType.h"
#include "CancellationToken.h"
#include "TaskCoroRuntimeException.h"
#include "OperationCanceledException.h"

namespace taskcoro
{
    class TaskCoroImpl
    {
        std::unique_ptr<concurrencpp::runtime> ccruntime_;
        std::shared_ptr<concurrencpp::manual_executor> update_executor_;
        std::shared_ptr<concurrencpp::thread_executor> thread_executor_;
        std::shared_ptr<concurrencpp::thread_pool_executor> thread_pool_executor_;
        std::shared_ptr<concurrencpp::thread_pool_executor> thread_pool_io_executor_;

        std::thread::id main_thread_id_;

    public:
        explicit TaskCoroImpl(std::thread::id main_thread_id) :
            main_thread_id_(main_thread_id)
        {
            ccruntime_ = std::make_unique<concurrencpp::runtime>();
            update_executor_ = ccruntime_->make_manual_executor();
            thread_executor_ = ccruntime_->thread_executor();
            thread_pool_executor_ = ccruntime_->thread_pool_executor();
            thread_pool_io_executor_ = ccruntime_->background_executor();
        }

        ~TaskCoroImpl()
        {
            if (update_executor_ && !update_executor_->shutdown_requested())
            {
                update_executor_->shutdown();

                while (!update_executor_->empty())
                {
                    update_executor_->loop_once();
                }
            }

            thread_pool_io_executor_->shutdown();
            thread_pool_executor_->shutdown();
            thread_executor_->shutdown();
        }

        void Update()
        {
            if (update_executor_ && !update_executor_->shutdown_requested())
            {
                update_executor_->loop_once();
            }
        }

        concurrencpp::result<void> Yield_()
        {
            if (std::this_thread::get_id() == main_thread_id_)
            {
                co_return co_await update_executor_->submit([]{ });
            }

            std::this_thread::yield();
            co_return co_await concurrencpp::make_ready_result<void>();
        }

        template<class TRet, class TCallable, class... TArgs>
        concurrencpp::result<TRet> RunTask(TaskType task_type, bool try_resume_in_caller_thread, TCallable&& callable, TArgs&&... arguments)
        {
            std::shared_ptr<concurrencpp::executor> executor;

            switch (task_type)
            {
            case TaskType::Regular:
                executor = thread_pool_executor_;
                break;

            case TaskType::NewThread:
                executor = thread_executor_;
                break;

            case TaskType::MainThread:
                executor = update_executor_;
                break;

            case TaskType::IO:
                executor = thread_pool_io_executor_;
                break;

            default:
                throw std::invalid_argument(std::format("Invalid task type {}",  magic_enum::enum_name(task_type)));
            }

            return RunInternal<TRet, TCallable>(
                executor,
                try_resume_in_caller_thread,
                std::forward<TCallable>(callable),
                std::forward<TArgs>(arguments)...
            );
        }

        concurrencpp::result<void> TrySwitchToThread(std::thread::id thread_id)
        {
            if (main_thread_id_ == thread_id)
            {
                co_await resume_on(update_executor_);
            }
        }

        concurrencpp::result<void> TrySwitchToMainThread()
        {
            if (main_thread_id_ != std::this_thread::get_id())
            {
                co_await resume_on(update_executor_);
            }
        }

    private:
        template<class TRet, class TCallable, class... TArgs>
            requires !std::is_void_v<TRet>
        concurrencpp::result<TRet> RunInternal(std::shared_ptr<concurrencpp::executor> executor, bool try_resume_in_caller_thread, TCallable&& callable, TArgs&&... arguments)
        {
            if (executor == nullptr)
            {
                throw TaskCoroRuntimeException(TaskCoroRuntimeExceptionType::ExecutorNotInitialized);
            }

            if (executor->shutdown_requested())
            {
                throw TaskCoroRuntimeException(TaskCoroRuntimeExceptionType::ExecutorShutdown);
            }

            try
            {
                std::thread::id invoke_thread_id = std::this_thread::get_id();

                TRet result = co_await executor->submit(std::forward<TCallable>(callable), std::forward<TArgs>(arguments)...);

                if (try_resume_in_caller_thread)
                {
                    co_await TrySwitchToThread(invoke_thread_id);
                }

                co_return result;
            }
            catch (concurrencpp::errors::interrupted_task& e)
            {
                throw OperationCanceledException(e.what());
            }
        }

        template<class TRet, class TCallable, class... TArgs>
            requires std::is_void_v<TRet>
        concurrencpp::result<void> RunInternal(std::shared_ptr<concurrencpp::executor> executor, bool try_resume_in_caller_thread, TCallable&& callable, TArgs&&... arguments)
        {
            if (executor == nullptr)
            {
                throw TaskCoroRuntimeException(TaskCoroRuntimeExceptionType::ExecutorNotInitialized);
            }

            if (executor->shutdown_requested())
            {
                throw TaskCoroRuntimeException(TaskCoroRuntimeExceptionType::ExecutorShutdown);
            }

            try
            {
                std::thread::id invoke_thread_id = std::this_thread::get_id();

                co_await executor->submit(std::forward<TCallable>(callable), std::forward<TArgs>(arguments)...);

                if (try_resume_in_caller_thread)
                {
                    co_await TrySwitchToThread(invoke_thread_id);
                }
            }
            catch (concurrencpp::errors::interrupted_task& e)
            {
                throw OperationCanceledException(e.what());
            }
        }
    };
}
