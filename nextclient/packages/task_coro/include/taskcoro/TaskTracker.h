#pragma once
#include <memory>
#include <concurrencpp/concurrencpp.h>

namespace taskcoro
{
    class TaskTracker
    {
    public:
        class Token
        {
        public:
            Token(TaskTracker* parent);

            Token(Token&& other) noexcept;

            Token(const Token&) = delete;
            Token& operator=(const Token&) = delete;

            ~Token();

        private:
            TaskTracker* parent_;
        };

    private:
        std::atomic<size_t> awaiters_{0};
        std::atomic<size_t> active_{0};
        std::mutex mutex_;
        concurrencpp::result_promise<void> promise_;
        concurrencpp::shared_result<void> shared_result_;

        bool initialized_{};
        bool waiter_registered_{};
        bool pending_set_{};
        bool result_set_{};

    public:
        explicit TaskTracker();
        ~TaskTracker() = default;

        Token MakeToken();
        concurrencpp::result<void> WaitAsync();

    private:
        void OnTaskFinished();
    };
}
