#pragma once
#include <chrono>

#include <concurrencpp/concurrencpp.h>

#include "../TaskType.h"
#include "../TaskCoroImplInterface.h"

namespace taskcoro
{
    class TaskCoroImpl : public TaskCoroImplInterface, public std::enable_shared_from_this<TaskCoroImpl>
    {
        std::unique_ptr<concurrencpp::runtime> ccruntime_;
        std::shared_ptr<concurrencpp::manual_executor> update_executor_;
        std::shared_ptr<concurrencpp::thread_executor> thread_executor_;
        std::shared_ptr<concurrencpp::thread_pool_executor> thread_pool_executor_;
        std::shared_ptr<concurrencpp::thread_pool_executor> thread_pool_io_executor_;

        std::thread::id main_thread_id_;
        bool is_main_thead_available_;

    public:
        explicit TaskCoroImpl(std::thread::id main_thread_id, bool is_main_thead_available = true);
        explicit TaskCoroImpl();
        ~TaskCoroImpl() override;

        bool IsMainThread() override;
        bool IsMainThreadAvailable() override;
        concurrencpp::result<void> SwitchTo(TaskType task_type) override;
        concurrencpp::result<void> Yield_() override;
        concurrencpp::result<void> WaitForMs(std::chrono::milliseconds ms) override;
        concurrencpp::result<void> WaitForNextFrame() override;
        void RunTask(TaskType task_type, std::function<void()> task) override;
        std::shared_ptr<SynchronizationContextImplInterface> CreateSynchronizationContext(TaskType task_type, std::thread::id thread_id) override;

        void Update() const;

    private:
        std::shared_ptr<concurrencpp::executor> GetExecutorByTaskType(TaskType task_type) const;
    };
}