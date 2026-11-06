#pragma once

#include <chrono>
#include <deque>
#include <functional>
#include <memory>
#include <stdexcept>
#include <thread>
#include <utility>

#include <gtest/gtest.h>
#include <taskcoro/TaskCoroImplInterface.h>

class RadarLayerScheduledRuntime : public taskcoro::TaskCoroImplInterface
{
public:
    std::deque<std::function<void()>> tasks;
    bool reject{};

    bool IsMainThreadAvailable() override
    {
        return false;
    }

    bool IsMainThread() override
    {
        return true;
    }

    concurrencpp::result<void> SwitchTo(taskcoro::TaskType) override
    {
        throw std::logic_error("unexpected continuation");
    }

    concurrencpp::result<void> Yield_() override
    {
        throw std::logic_error("unexpected yield");
    }

    concurrencpp::result<void> WaitForMs(std::chrono::milliseconds) override
    {
        throw std::logic_error("unexpected timer");
    }

    concurrencpp::result<void> WaitForNextFrame() override
    {
        throw std::logic_error("unexpected frame wait");
    }

    void RunTask(taskcoro::TaskType type, std::function<void()> task) override
    {
        EXPECT_EQ(type, taskcoro::TaskType::ThreadPool);

        if (reject)
        {
            throw std::runtime_error("scheduled worker unavailable");
        }

        tasks.push_back(std::move(task));
    }

    std::shared_ptr<taskcoro::SynchronizationContextImplInterface> CreateSynchronizationContext(
        taskcoro::TaskType,
        std::thread::id
    ) override
    {
        return nullptr;
    }

    void RunNext()
    {
        ASSERT_FALSE(tasks.empty());

        std::function<void()> task = std::move(tasks.front());
        tasks.pop_front();
        task();
    }
};
