#include <gtest/gtest.h>

#include <chrono>
#include <deque>
#include <future>
#include <memory>
#include <stdexcept>
#include <thread>
#include <utility>

#include <taskcoro/impl/TaskCoroImpl.h>
#include <taskcoro/TaskCoro.h>

#include "hud/radar/map/layers/RadarLayerAnalysisJob.h"
#include "hud/radar/map/layers/RadarLayerContext.h"
#include "RadarMapLayerFixtures.h"

namespace
{
    class ScheduledRuntime : public taskcoro::TaskCoroImplInterface
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

    class RadarLayerScheduledJobTest : public testing::Test
    {
    protected:
        std::shared_ptr<ScheduledRuntime> runtime = std::make_shared<ScheduledRuntime>();

        void SetUp() override
        {
            taskcoro::TaskCoro::Initialize(runtime);
        }

        void TearDown() override
        {
            taskcoro::SynchronizationContext::SetCurrent(nullptr);
            taskcoro::TaskCoro::UnInitialize();
        }

        RadarMapGeometry Floor(float height)
        {
            RadarMapLayerFixture fixture;
            fixture.AddFloor(height, {{0.0f, 0.0f}, {256.0f, 256.0f}});

            return std::move(fixture.geometry);
        }
    };

    class RadarLayerAnalysisJobTest : public testing::Test
    {
    protected:
        void SetUp() override
        {
            runtime_ = std::make_shared<taskcoro::TaskCoroImpl>(std::this_thread::get_id(), false);
            taskcoro::TaskCoro::Initialize(runtime_);
        }

        void TearDown() override
        {
            taskcoro::TaskCoro::UnInitialize();
            runtime_.reset();
        }

        std::unique_ptr<RadarLayerAnalysisResult> Wait(RadarLayerAnalysisJob& job)
        {
            const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);

            while (std::chrono::steady_clock::now() < deadline)
            {
                if (std::unique_ptr<RadarLayerAnalysisResult> result = job.Poll())
                {
                    return result;
                }
            }

            return nullptr;
        }

        RadarMapGeometry Floor(float height)
        {
            RadarMapLayerFixture fixture;
            fixture.AddFloor(height, {{0.0f, 0.0f}, {256.0f, 256.0f}});

            return std::move(fixture.geometry);
        }

    private:
        std::shared_ptr<taskcoro::TaskCoroImpl> runtime_;
    };
} // namespace

TEST_F(RadarLayerScheduledJobTest, StalledWorkerPollAndResetReturnWithoutWaiting)
{
    RadarLayerAnalysisJob job;
    job.Start(Floor(24.0f));

    ASSERT_EQ(runtime->tasks.size(), 1u);
    EXPECT_FALSE(job.Poll());

    job.Reset();

    EXPECT_FALSE(job.is_pending());
    EXPECT_FALSE(job.Poll());
    ASSERT_EQ(runtime->tasks.size(), 1u);

    runtime->RunNext();

    EXPECT_FALSE(job.Poll());
}

TEST_F(RadarLayerScheduledJobTest, CanceledWorkerCannotPublishAndOnlyLatestPendingInputRuns)
{
    RadarLayerAnalysisJob job;
    job.Start(Floor(24.0f));
    job.Start(Floor(48.0f));
    job.Start(Floor(96.0f));

    ASSERT_EQ(runtime->tasks.size(), 1u);
    EXPECT_FALSE(job.Poll());

    runtime->RunNext();

    EXPECT_FALSE(job.Poll());
    ASSERT_EQ(runtime->tasks.size(), 1u);

    runtime->RunNext();
    std::unique_ptr<RadarLayerAnalysisResult> result = job.Poll();
    ASSERT_TRUE(result);
    ASSERT_EQ(result->map.get_layers().size(), 1u);
    EXPECT_FLOAT_EQ(result->map.get_layers()[0].height, 96.0f);
    EXPECT_FALSE(job.Poll());
}

TEST_F(RadarLayerScheduledJobTest, SchedulerFailurePublishesOnceAndDoesNotRemainPending)
{
    runtime->reject = true;
    RadarLayerAnalysisJob job;
    job.Start(Floor(24.0f));
    std::unique_ptr<RadarLayerAnalysisResult> result = job.Poll();

    ASSERT_TRUE(result);
    EXPECT_EQ(result->status, RadarMapStatus::Failed);
    EXPECT_EQ(result->failure_reason, "scheduled worker unavailable");
    EXPECT_FALSE(job.is_pending());
    EXPECT_FALSE(job.Poll());
}

TEST_F(RadarLayerAnalysisJobTest, CompletesWithoutMainThreadPumpAndPublishesOnce)
{
    RadarLayerAnalysisJob job;
    job.Start(Floor(24.0f));

    ASSERT_TRUE(job.is_pending());

    std::unique_ptr<RadarLayerAnalysisResult> result = Wait(job);
    ASSERT_TRUE(result);
    EXPECT_EQ(result->status, RadarMapStatus::Ready);
    ASSERT_EQ(result->map.get_layers().size(), 1u);
    EXPECT_FLOAT_EQ(result->map.get_layers()[0].height, 24.0f);
    EXPECT_FALSE(job.is_pending());
    EXPECT_FALSE(job.Poll());
}

TEST_F(RadarLayerAnalysisJobTest, ResetDiscardsCompletionAndRetainsOnlyLatestRequest)
{
    RadarLayerAnalysisJob job;
    job.Start(Floor(24.0f));
    job.Reset();

    EXPECT_FALSE(job.is_pending());
    EXPECT_FALSE(job.Poll());

    job.Start(Floor(48.0f));
    job.Start(Floor(96.0f));
    std::unique_ptr<RadarLayerAnalysisResult> result = Wait(job);

    ASSERT_TRUE(result);
    ASSERT_EQ(result->map.get_layers().size(), 1u);
    EXPECT_FLOAT_EQ(result->map.get_layers()[0].height, 96.0f);
    EXPECT_FALSE(job.Poll());
}

TEST_F(RadarLayerAnalysisJobTest, NoSupportFailureLatchesOnce)
{
    RadarLayerAnalysisJob job;
    job.Start(RadarMapGeometry{});
    std::unique_ptr<RadarLayerAnalysisResult> result = Wait(job);

    ASSERT_TRUE(result);
    EXPECT_EQ(result->status, RadarMapStatus::Failed);

    RadarLayerLifecycle lifecycle;
    EXPECT_TRUE(lifecycle.SetGeometryStatus(result->status, result->failure_reason));
    EXPECT_TRUE(lifecycle.should_use_fallback());
    EXPECT_FALSE(job.is_pending());
    EXPECT_FALSE(job.Poll());
}

TEST_F(RadarLayerAnalysisJobTest, OwnedOverridesAreAppliedOnWorker)
{
    RadarLayerAnalysisJob job;
    RadarLayerOverrides overrides;
    overrides.layers.push_back({"ground", 0.0f, 64.0f});
    job.Start(Floor(24.0f), overrides);
    overrides.layers[0].name = "changed caller";
    std::unique_ptr<RadarLayerAnalysisResult> result = Wait(job);

    ASSERT_TRUE(result);
    EXPECT_EQ(result->status, RadarMapStatus::Ready);
    ASSERT_EQ(result->map.get_layers().size(), 1u);
    EXPECT_EQ(result->map.get_layers()[0].name, "ground");
}

TEST_F(RadarLayerScheduledJobTest, DestructionJoinsWithoutPumpingFrames)
{
    std::unique_ptr<RadarLayerAnalysisJob> job = std::make_unique<RadarLayerAnalysisJob>();
    job->Start(Floor(24.0f));

    EXPECT_EQ(runtime->tasks.size(), 1u);

    std::promise<void> destruction_entered;
    std::future<void> entered = destruction_entered.get_future();
    std::future<void> destruction = std::async(std::launch::async, [job = std::move(job), &destruction_entered]() mutable {
        destruction_entered.set_value();
        job.reset();
    });

    EXPECT_EQ(entered.wait_for(std::chrono::seconds(5)), std::future_status::ready);
    EXPECT_EQ(destruction.wait_for(std::chrono::milliseconds(100)), std::future_status::timeout);

    runtime->RunNext();

    EXPECT_EQ(destruction.wait_for(std::chrono::seconds(5)), std::future_status::ready);
    destruction.get();
    EXPECT_TRUE(runtime->tasks.empty());
}

TEST(RadarLayerLifecycle, WorkerExceptionReasonIsOwnedAndLatchedOnce)
{
    RadarLayerLifecycle lifecycle;
    std::string reason = "analysis allocation failed";

    EXPECT_TRUE(lifecycle.SetGeometryStatus(RadarMapStatus::Failed, reason));

    reason = "changed caller";
    EXPECT_FALSE(lifecycle.SetGeometryStatus(RadarMapStatus::Failed, "later failure"));
    EXPECT_EQ(lifecycle.get_failure_reason(), "analysis allocation failed");

    lifecycle.Reset();

    EXPECT_TRUE(lifecycle.get_failure_reason().empty());
}
