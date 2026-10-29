#include <gtest/gtest.h>

#include <chrono>
#include <functional>
#include <future>
#include <memory>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>

#include "hud/radar/integration/LayerAnalysis.h"
#include "hud/radar/integration/Performance.h"
#include "RadarLayerScheduledRuntime.h"
#include "RadarMapLayerFixtures.h"
#include "RadarRecordingMonitor.h"

namespace
{
    class AnalysisRecordingMonitor : public RadarRecordingMonitor
    {
    public:
        std::vector<std::thread::id> report_threads;
        std::function<void(const performance::PerfWorkerReport&)> on_report;

        void ReportWorker(const performance::PerfWorkerReport& report) override
        {
            RadarRecordingMonitor::ReportWorker(report);
            report_threads.push_back(std::this_thread::get_id());

            if (on_report)
            {
                on_report(report);
            }
        }
    };

    class RadarLayerAnalysisIntegration : public testing::Test
    {
    protected:
        std::shared_ptr<RadarLayerScheduledRuntime> runtime = std::make_shared<RadarLayerScheduledRuntime>();
        AnalysisRecordingMonitor monitor;
        radar::integration::Performance performance{&monitor};

        RadarMapGeometry Floor(float height)
        {
            RadarMapLayerFixture fixture;
            fixture.AddFloor(height, {{0.0f, 0.0f}, {256.0f, 256.0f}});

            return std::move(fixture.geometry);
        }
    };
} // namespace

static_assert(!std::is_copy_constructible_v<radar::integration::LayerAnalysis>);
static_assert(!std::is_copy_assignable_v<radar::integration::LayerAnalysis>);
static_assert(!std::is_move_constructible_v<radar::integration::LayerAnalysis>);
static_assert(!std::is_move_assignable_v<radar::integration::LayerAnalysis>);

TEST_F(RadarLayerAnalysisIntegration, PublishesCompletedOnlyOnOwnerWithOriginalGeneration)
{
    radar::integration::LayerAnalysis analysis(*runtime, performance);
    analysis.Start(Floor(24.0f));

    ASSERT_EQ(monitor.workers.size(), 1u);
    EXPECT_EQ(monitor.workers[0].status, performance::PerfWorkerStatus::Pending);
    EXPECT_EQ(monitor.workers[0].map_generation, 1u);
    monitor.generation = 7;

    std::function<void()> task = std::move(runtime->tasks.front());
    runtime->tasks.pop_front();
    std::future<void> worker = std::async(std::launch::async, std::move(task));
    worker.get();

    EXPECT_EQ(monitor.workers.size(), 1u);
    std::unique_ptr<RadarLayerAnalysisResult> result = analysis.Poll();
    ASSERT_TRUE(result);
    ASSERT_EQ(monitor.workers.size(), 2u);
    EXPECT_EQ(monitor.workers[1].map_generation, 1u);
    EXPECT_EQ(monitor.workers[1].status, performance::PerfWorkerStatus::Completed);
    EXPECT_EQ(monitor.workers[1].duration_ns, result->duration_ns);
    EXPECT_GT(result->duration_ns, 0u);

    for (std::thread::id report_thread : monitor.report_threads)
    {
        EXPECT_EQ(report_thread, std::this_thread::get_id());
    }

    EXPECT_FALSE(analysis.Poll());
    EXPECT_FALSE(analysis.is_pending());
}

TEST_F(RadarLayerAnalysisIntegration, FailedAnalysisKeepsStartGeneration)
{
    radar::integration::LayerAnalysis analysis(*runtime, performance);
    analysis.Start(RadarMapGeometry{});
    monitor.generation = 2;
    runtime->RunNext();

    EXPECT_EQ(monitor.workers.size(), 1u);
    std::unique_ptr<RadarLayerAnalysisResult> result = analysis.Poll();
    ASSERT_TRUE(result);
    EXPECT_EQ(result->status, RadarMapStatus::Failed);
    ASSERT_EQ(monitor.workers.size(), 2u);
    EXPECT_EQ(monitor.workers[1].map_generation, 1u);
    EXPECT_EQ(monitor.workers[1].status, performance::PerfWorkerStatus::Failed);
    EXPECT_EQ(monitor.workers[1].duration_ns, result->duration_ns);
    EXPECT_FALSE(analysis.Poll());
}

TEST_F(RadarLayerAnalysisIntegration, CanceledCompletionIsSuppressedAndLatestPendingKeepsStartGeneration)
{
    radar::integration::LayerAnalysis analysis(*runtime, performance);
    analysis.Start(Floor(24.0f));
    monitor.generation = 2;
    analysis.Start(Floor(48.0f));
    monitor.generation = 3;
    analysis.Start(Floor(96.0f));

    ASSERT_EQ(monitor.workers.size(), 3u);
    EXPECT_EQ(monitor.workers[1].map_generation, 1u);
    EXPECT_EQ(monitor.workers[1].status, performance::PerfWorkerStatus::Canceled);
    EXPECT_EQ(monitor.workers[2].map_generation, 2u);
    EXPECT_EQ(monitor.workers[2].status, performance::PerfWorkerStatus::Canceled);
    monitor.generation = 4;
    runtime->RunNext();

    EXPECT_FALSE(analysis.Poll());
    ASSERT_EQ(monitor.workers.size(), 4u);
    EXPECT_EQ(monitor.workers[3].map_generation, 3u);
    EXPECT_EQ(monitor.workers[3].status, performance::PerfWorkerStatus::Pending);
    runtime->RunNext();

    std::unique_ptr<RadarLayerAnalysisResult> result = analysis.Poll();
    ASSERT_TRUE(result);
    ASSERT_EQ(result->map.get_layers().size(), 1u);
    EXPECT_FLOAT_EQ(result->map.get_layers()[0].height, 96.0f);
    ASSERT_EQ(monitor.workers.size(), 5u);
    EXPECT_EQ(monitor.workers[4].map_generation, 3u);
    EXPECT_EQ(monitor.workers[4].status, performance::PerfWorkerStatus::Completed);
    EXPECT_FALSE(analysis.Poll());
}

TEST_F(RadarLayerAnalysisIntegration, ResetCancelsUndispatchedInputOnce)
{
    radar::integration::LayerAnalysis analysis(*runtime, performance);
    analysis.Start(Floor(24.0f));
    monitor.generation = 2;
    analysis.Start(Floor(48.0f));
    monitor.generation = 3;
    analysis.Reset();
    analysis.Reset();

    ASSERT_EQ(monitor.workers.size(), 3u);
    EXPECT_EQ(monitor.workers[1].map_generation, 1u);
    EXPECT_EQ(monitor.workers[1].status, performance::PerfWorkerStatus::Canceled);
    EXPECT_EQ(monitor.workers[2].map_generation, 2u);
    EXPECT_EQ(monitor.workers[2].status, performance::PerfWorkerStatus::Canceled);
    runtime->RunNext();

    EXPECT_FALSE(analysis.Poll());
    EXPECT_FALSE(analysis.is_pending());
    EXPECT_EQ(monitor.workers.size(), 3u);
    EXPECT_TRUE(runtime->tasks.empty());
}

TEST_F(RadarLayerAnalysisIntegration, SchedulerFailurePublishesOwnedFailureOnce)
{
    runtime->reject = true;
    radar::integration::LayerAnalysis analysis(*runtime, performance);
    analysis.Start(Floor(24.0f));
    std::unique_ptr<RadarLayerAnalysisResult> result = analysis.Poll();

    ASSERT_TRUE(result);
    EXPECT_EQ(result->status, RadarMapStatus::Failed);
    EXPECT_EQ(result->failure_reason, "scheduled worker unavailable");
    ASSERT_EQ(monitor.workers.size(), 2u);
    EXPECT_EQ(monitor.workers[0].status, performance::PerfWorkerStatus::Pending);
    EXPECT_EQ(monitor.workers[1].map_generation, 1u);
    EXPECT_EQ(monitor.workers[1].status, performance::PerfWorkerStatus::Failed);
    EXPECT_FALSE(analysis.is_pending());
    EXPECT_FALSE(analysis.Poll());
}

TEST_F(RadarLayerAnalysisIntegration, NullMonitorAllowsCompletionCancellationAndSchedulerFailure)
{
    radar::integration::Performance performance{nullptr};

    radar::integration::LayerAnalysis analysis(*runtime, performance);
    analysis.Start(Floor(24.0f));
    runtime->RunNext();

    EXPECT_TRUE(analysis.Poll());
    analysis.Start(Floor(48.0f));
    analysis.Reset();
    runtime->RunNext();

    EXPECT_FALSE(analysis.Poll());
    runtime->reject = true;
    analysis.Start(Floor(96.0f));
    std::unique_ptr<RadarLayerAnalysisResult> result = analysis.Poll();

    ASSERT_TRUE(result);
    EXPECT_EQ(result->failure_reason, "scheduled worker unavailable");
    EXPECT_FALSE(analysis.is_pending());
    EXPECT_TRUE(monitor.workers.empty());
}

TEST_F(RadarLayerAnalysisIntegration, ShutdownPublishesCancellationBeforeJoinAndMonitorClear)
{
    std::promise<void> canceled;
    std::future<void> cancellation = canceled.get_future();
    monitor.on_report = [&canceled](const performance::PerfWorkerReport& report) {
        if (report.status == performance::PerfWorkerStatus::Canceled)
        {
            canceled.set_value();
        }
    };

    {
        radar::integration::LayerAnalysis analysis(*runtime, performance);
        analysis.Start(Floor(24.0f));
        std::function<void()> task = std::move(runtime->tasks.front());
        runtime->tasks.pop_front();
        std::future<void> worker = std::async(std::launch::async, [task = std::move(task), &cancellation]() mutable {
            EXPECT_EQ(cancellation.wait_for(std::chrono::seconds(5)), std::future_status::ready);
            task();
        });

        analysis.Shutdown();

        EXPECT_EQ(worker.wait_for(std::chrono::seconds(5)), std::future_status::ready);
        worker.get();
        ASSERT_EQ(monitor.workers.size(), 2u);
        EXPECT_EQ(monitor.workers[1].map_generation, 1u);
        EXPECT_EQ(monitor.workers[1].status, performance::PerfWorkerStatus::Canceled);
        EXPECT_FALSE(analysis.is_pending());
        analysis.Shutdown();
    }

    EXPECT_EQ(monitor.workers.size(), 2u);
    EXPECT_TRUE(runtime->tasks.empty());
}

TEST_F(RadarLayerAnalysisIntegration, DestructionCancelsRunningAndPendingBeforeMonitorClear)
{
    std::promise<void> pending_canceled;
    std::future<void> cancellation = pending_canceled.get_future();
    monitor.on_report = [&pending_canceled](const performance::PerfWorkerReport& report) {
        if (report.map_generation == 2 && report.status == performance::PerfWorkerStatus::Canceled)
        {
            pending_canceled.set_value();
        }
    };

    std::unique_ptr<radar::integration::LayerAnalysis> analysis =
        std::make_unique<radar::integration::LayerAnalysis>(*runtime, performance);
    analysis->Start(Floor(24.0f));
    monitor.generation = 2;
    analysis->Start(Floor(48.0f));
    std::function<void()> task = std::move(runtime->tasks.front());
    runtime->tasks.pop_front();
    std::future<void> worker = std::async(std::launch::async, [task = std::move(task), &cancellation]() mutable {
        EXPECT_EQ(cancellation.wait_for(std::chrono::seconds(5)), std::future_status::ready);
        task();
    });

    analysis.reset();

    EXPECT_EQ(worker.wait_for(std::chrono::seconds(5)), std::future_status::ready);
    worker.get();
    ASSERT_EQ(monitor.workers.size(), 3u);
    EXPECT_EQ(monitor.workers[0].map_generation, 1u);
    EXPECT_EQ(monitor.workers[0].status, performance::PerfWorkerStatus::Pending);
    EXPECT_EQ(monitor.workers[1].map_generation, 1u);
    EXPECT_EQ(monitor.workers[1].status, performance::PerfWorkerStatus::Canceled);
    EXPECT_EQ(monitor.workers[2].map_generation, 2u);
    EXPECT_EQ(monitor.workers[2].status, performance::PerfWorkerStatus::Canceled);
    EXPECT_TRUE(runtime->tasks.empty());
}

TEST_F(RadarLayerAnalysisIntegration, IndependentOwnersKeepSchedulersAndAttribution)
{
    RadarLayerScheduledRuntime other_runtime;
    AnalysisRecordingMonitor other_monitor;
    other_monitor.generation = 9;
    radar::integration::Performance other_performance{&other_monitor};
    radar::integration::LayerAnalysis first(*runtime, performance);
    radar::integration::LayerAnalysis second(other_runtime, other_performance);
    first.Start(Floor(24.0f));
    second.Start(Floor(48.0f));

    EXPECT_EQ(runtime->tasks.size(), 1u);
    EXPECT_EQ(other_runtime.tasks.size(), 1u);
    other_runtime.RunNext();
    ASSERT_TRUE(second.Poll());
    EXPECT_EQ(monitor.workers.size(), 1u);
    ASSERT_EQ(other_monitor.workers.size(), 2u);
    EXPECT_EQ(other_monitor.workers.back().map_generation, 9u);

    runtime->RunNext();
    ASSERT_TRUE(first.Poll());
    ASSERT_EQ(monitor.workers.size(), 2u);
    EXPECT_EQ(monitor.workers.back().map_generation, 1u);
}
