#include <gtest/gtest.h>

#include <chrono>
#include <future>
#include <memory>
#include <thread>
#include <utility>
#include <vector>

#include <taskcoro/impl/TaskCoroImpl.h>

#include "hud/radar/map/layers/RadarLayerAnalysisJob.h"
#include "hud/radar/map/layers/RadarLayerContext.h"
#include "RadarMapLayerFixtures.h"
#include "RadarLayerScheduledRuntime.h"

namespace
{
    class RadarLayerScheduledJobTest : public testing::Test
    {
    protected:
        std::shared_ptr<RadarLayerScheduledRuntime> runtime = std::make_shared<RadarLayerScheduledRuntime>();
        std::vector<RadarLayerAnalysisEvent> events;

        RadarLayerAnalysisSink_t Sink()
        {
            return [this](const RadarLayerAnalysisEvent& event) { events.push_back(event); };
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
        }

        void TearDown() override
        {
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

    protected:
        std::shared_ptr<taskcoro::TaskCoroImpl> runtime_;
    };
} // namespace

TEST_F(RadarLayerScheduledJobTest, StalledWorkerPollAndResetReturnWithoutWaiting)
{
    RadarLayerAnalysisJob job(*runtime);
    job.Start(1, Floor(24.0f));

    ASSERT_EQ(runtime->tasks.size(), 1u);
    EXPECT_FALSE(job.Poll());

    job.Reset();

    EXPECT_FALSE(job.is_pending());
    EXPECT_FALSE(job.Poll());
    ASSERT_EQ(runtime->tasks.size(), 1u);

    runtime->RunNext();

    EXPECT_FALSE(job.Poll());
}

TEST_F(RadarLayerScheduledJobTest, WorkerPublishesNativeOutcomeOnlyOnOwnerAndKeepsRequest)
{
    RadarLayerAnalysisJob job(*runtime, Sink());
    job.Start(42, Floor(24.0f));

    ASSERT_EQ(events.size(), 1u);
    EXPECT_EQ(events[0].request, 42u);
    EXPECT_EQ(events[0].status, RadarLayerAnalysisStatus::Pending);

    runtime->RunNext();

    EXPECT_EQ(events.size(), 1u);
    std::unique_ptr<RadarLayerAnalysisResult> result = job.Poll();
    ASSERT_TRUE(result);
    EXPECT_EQ(result->request, 42u);
    EXPECT_GT(result->duration_ns, 0u);
    ASSERT_EQ(events.size(), 2u);
    EXPECT_EQ(events[1].request, result->request);
    EXPECT_EQ(events[1].duration_ns, result->duration_ns);
    EXPECT_EQ(events[1].status, RadarLayerAnalysisStatus::Completed);
}

TEST_F(RadarLayerScheduledJobTest, CanceledCompletionCannotPublishNativeOutcome)
{
    RadarLayerAnalysisJob job(*runtime, Sink());
    job.Start(1, Floor(24.0f));
    job.Reset();

    ASSERT_EQ(events.size(), 2u);
    EXPECT_EQ(events.back().request, 1u);
    EXPECT_EQ(events.back().status, RadarLayerAnalysisStatus::Canceled);

    job.Start(2, Floor(48.0f));
    runtime->RunNext();

    EXPECT_FALSE(job.Poll());
    ASSERT_EQ(events.size(), 3u);
    EXPECT_EQ(events.back().request, 2u);
    EXPECT_EQ(events.back().status, RadarLayerAnalysisStatus::Pending);

    runtime->RunNext();

    EXPECT_TRUE(job.Poll());
    ASSERT_EQ(events.size(), 4u);
    EXPECT_EQ(events.back().request, 2u);
    EXPECT_EQ(events.back().status, RadarLayerAnalysisStatus::Completed);
}

TEST_F(RadarLayerScheduledJobTest, ResetCancelsRunningAndUndispatchedRequestsOnce)
{
    RadarLayerAnalysisJob job(*runtime, Sink());
    job.Start(1, Floor(24.0f));
    job.Start(2, Floor(48.0f));
    job.Start(3, Floor(96.0f));
    job.Reset();
    job.Reset();

    ASSERT_EQ(events.size(), 4u);
    EXPECT_EQ(events[0].request, 1u);
    EXPECT_EQ(events[0].status, RadarLayerAnalysisStatus::Pending);
    EXPECT_EQ(events[1].request, 1u);
    EXPECT_EQ(events[1].status, RadarLayerAnalysisStatus::Canceled);
    EXPECT_EQ(events[2].request, 2u);
    EXPECT_EQ(events[2].status, RadarLayerAnalysisStatus::Canceled);
    EXPECT_EQ(events[3].request, 3u);
    EXPECT_EQ(events[3].status, RadarLayerAnalysisStatus::Canceled);

    runtime->RunNext();

    EXPECT_FALSE(job.Poll());
    EXPECT_EQ(events.size(), 4u);
    EXPECT_FALSE(job.is_pending());
}

TEST_F(RadarLayerScheduledJobTest, CanceledWorkerCannotPublishAndOnlyLatestPendingInputRuns)
{
    RadarLayerAnalysisJob job(*runtime);
    job.Start(1, Floor(24.0f));
    job.Start(2, Floor(48.0f));
    job.Start(3, Floor(96.0f));

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
    RadarLayerAnalysisJob job(*runtime, Sink());
    job.Start(1, Floor(24.0f));
    std::unique_ptr<RadarLayerAnalysisResult> result = job.Poll();

    ASSERT_TRUE(result);
    EXPECT_EQ(result->status, RadarMapStatus::Failed);
    EXPECT_EQ(result->failure_reason, "scheduled worker unavailable");
    EXPECT_EQ(result->request, 1u);
    ASSERT_EQ(events.size(), 2u);
    EXPECT_EQ(events[0].status, RadarLayerAnalysisStatus::Pending);
    EXPECT_EQ(events[1].request, 1u);
    EXPECT_EQ(events[1].status, RadarLayerAnalysisStatus::Failed);
    EXPECT_FALSE(job.is_pending());
    EXPECT_FALSE(job.Poll());
}

TEST_F(RadarLayerAnalysisJobTest, CompletesWithoutMainThreadPumpAndPublishesOnce)
{
    RadarLayerAnalysisJob job(*runtime_);
    job.Start(1, Floor(24.0f));

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
    RadarLayerAnalysisJob job(*runtime_);
    job.Start(1, Floor(24.0f));
    job.Reset();

    EXPECT_FALSE(job.is_pending());
    EXPECT_FALSE(job.Poll());

    job.Start(2, Floor(48.0f));
    job.Start(3, Floor(96.0f));
    std::unique_ptr<RadarLayerAnalysisResult> result = Wait(job);

    ASSERT_TRUE(result);
    ASSERT_EQ(result->map.get_layers().size(), 1u);
    EXPECT_FLOAT_EQ(result->map.get_layers()[0].height, 96.0f);
    EXPECT_FALSE(job.Poll());
}

TEST_F(RadarLayerAnalysisJobTest, NoSupportFailureLatchesOnce)
{
    RadarLayerAnalysisJob job(*runtime_);
    job.Start(1, RadarMapGeometry{});
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
    RadarLayerAnalysisJob job(*runtime_);
    RadarLayerOverrides overrides;
    overrides.layers.push_back({"ground", 0.0f, 64.0f});
    job.Start(1, Floor(24.0f), overrides);
    overrides.layers[0].name = "changed caller";
    std::unique_ptr<RadarLayerAnalysisResult> result = Wait(job);

    ASSERT_TRUE(result);
    EXPECT_EQ(result->status, RadarMapStatus::Ready);
    ASSERT_EQ(result->map.get_layers().size(), 1u);
    EXPECT_EQ(result->map.get_layers()[0].name, "ground");
}

TEST_F(RadarLayerScheduledJobTest, DestructionJoinsWithoutPumpingFrames)
{
    std::unique_ptr<RadarLayerAnalysisJob> job = std::make_unique<RadarLayerAnalysisJob>(*runtime);
    job->Start(1, Floor(24.0f));

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
