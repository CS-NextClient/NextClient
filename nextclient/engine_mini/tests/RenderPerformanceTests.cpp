#include <gtest/gtest.h>

#include <array>
#include <cstdlib>
#include <string>
#include <vector>

#include <goldsrc/studio_api.h>
#include <next_engine_mini/performance_program_callbacks.h>
#include <next_engine_mini/render_cvars.h>

#include "performance/performance_scopes.h"
#include "performance/PerformanceMonitor.h"
#include "graphics/render/StudioIntercept.h"
#include "graphics/render/StudioSubmit.h"
#include "graphics/render/StudioQueue.h"

namespace
{
    class RecordingMonitor : public performance::PerformanceMonitorInterface
    {
    public:
        std::vector<std::string> events;
        std::array<uint64_t, uint32_t(performance::PerfCounterId::Count)> counters{};
        std::array<performance::PerfScalarValue, uint32_t(performance::PerfGaugeId::Count)> gauges{};
        uint32_t gauge_writes{};
        ncl_perf::PerfScopeToken_t BeginCpu(ncl_perf::PerfScopeId_t, std::source_location) override
        {
            events.emplace_back("cpu.begin");
            return 1;
        }
        void EndCpu(ncl_perf::PerfScopeToken_t) override
        {
            events.emplace_back("cpu.end");
        }
        ncl_perf::PerfScopeToken_t BeginGpu(ncl_perf::PerfScopeId_t, std::source_location) override
        {
            events.emplace_back("gpu.begin");
            return 2;
        }
        void EndGpu(ncl_perf::PerfScopeToken_t) override
        {
            events.emplace_back("gpu.end");
        }
        void AddCounter(performance::PerfCounterId counter, uint64_t value) override
        {
            counters[uint32_t(counter)] += value;
        }
        bool SetGauge(performance::PerfGaugeId gauge, double value) override
        {
            gauges[uint32_t(gauge)] = {value, ncl_perf::PerfSampleState::Measured};
            ++gauge_writes;

            return true;
        }
        bool GetSnapshot(performance::PerfSnapshot&) const override
        {
            return false;
        }
        uint32_t map_generation() const override
        {
            return 1;
        }
        void ReportWorker(const performance::PerfWorkerReport&) override {}
    };

    int g_StockCalls{};

    void StockDraw()
    {
        ++g_StockCalls;
    }
} // namespace

TEST(RenderPerformanceTest, MultiDrawSeparatesApiCallsAndDraws)
{
    RecordingMonitor monitor;
    performance::reporting::ReportDraw(&monitor, 15, 2);

    EXPECT_EQ(monitor.counters[uint32_t(performance::PerfCounterId::StudioDrawCalls)], 1u);
    EXPECT_EQ(monitor.counters[uint32_t(performance::PerfCounterId::StudioConstituentDraws)], 2u);
    EXPECT_EQ(monitor.counters[uint32_t(performance::PerfCounterId::StudioTriangles)], 5u);
    EXPECT_EQ(monitor.counters[uint32_t(performance::PerfCounterId::StudioMultiDrawBatches)], 1u);
    EXPECT_EQ(monitor.counters[uint32_t(performance::PerfCounterId::StudioBatchedMeshes)], 2u);
    EXPECT_EQ(monitor.counters[uint32_t(performance::PerfCounterId::StudioSingleMeshes)], 0u);
    EXPECT_TRUE(monitor.events.empty());
}

TEST(RenderPerformanceTest, SingleMeshDoesNotCountAsAMultiDrawBatch)
{
    RecordingMonitor monitor;
    performance::reporting::ReportDraw(&monitor, 6, 1);

    EXPECT_EQ(monitor.counters[uint32_t(performance::PerfCounterId::StudioDrawCalls)], 1u);
    EXPECT_EQ(monitor.counters[uint32_t(performance::PerfCounterId::StudioConstituentDraws)], 1u);
    EXPECT_EQ(monitor.counters[uint32_t(performance::PerfCounterId::StudioTriangles)], 2u);
    EXPECT_EQ(monitor.counters[uint32_t(performance::PerfCounterId::StudioMultiDrawBatches)], 0u);
    EXPECT_EQ(monitor.counters[uint32_t(performance::PerfCounterId::StudioBatchedMeshes)], 0u);
    EXPECT_EQ(monitor.counters[uint32_t(performance::PerfCounterId::StudioSingleMeshes)], 1u);
    EXPECT_TRUE(monitor.events.empty());
}

TEST(RenderPerformanceTest, StudioQueueCountersDistinguishScopeSharingFromImmediateCauses)
{
    RecordingMonitor monitor;
    performance::reporting::StudioFlush(&monitor, performance::PerfCounterId::StudioFlushBrush, 3);
    performance::reporting::StudioFlush(&monitor, performance::PerfCounterId::StudioFlushImmediate, 1);
    performance::reporting::StudioFlush(&monitor, performance::PerfCounterId::StudioFlushMutableSkin, 1);
    performance::reporting::StudioImmediate(&monitor, performance::PerfCounterId::StudioImmediateOutsideOpaque);
    performance::reporting::StudioImmediate(&monitor, performance::PerfCounterId::StudioImmediateQueueDisabled);
    performance::reporting::StudioImmediate(&monitor, performance::PerfCounterId::StudioImmediateCallback);
    performance::reporting::StudioImmediate(&monitor, performance::PerfCounterId::StudioImmediateMutableSkin);
    EXPECT_EQ(monitor.counters[uint32_t(performance::PerfCounterId::StudioPackets)], 5u);
    EXPECT_EQ(monitor.counters[uint32_t(performance::PerfCounterId::StudioSharedRuns)], 1u);
    EXPECT_EQ(monitor.counters[uint32_t(performance::PerfCounterId::StudioSharedPackets)], 3u);
    EXPECT_EQ(monitor.counters[uint32_t(performance::PerfCounterId::StudioSinglePackets)], 2u);
    EXPECT_EQ(monitor.counters[uint32_t(performance::PerfCounterId::StudioFlushBrush)], 1u);
    EXPECT_EQ(monitor.counters[uint32_t(performance::PerfCounterId::StudioFlushImmediate)], 1u);
    EXPECT_EQ(monitor.counters[uint32_t(performance::PerfCounterId::StudioFlushMutableSkin)], 1u);
    EXPECT_EQ(monitor.counters[uint32_t(performance::PerfCounterId::StudioImmediateOutsideOpaque)], 1u);
    EXPECT_EQ(monitor.counters[uint32_t(performance::PerfCounterId::StudioImmediateQueueDisabled)], 1u);
    EXPECT_EQ(monitor.counters[uint32_t(performance::PerfCounterId::StudioImmediateCallback)], 1u);
    EXPECT_EQ(monitor.counters[uint32_t(performance::PerfCounterId::StudioImmediateMutableSkin)], 1u);
}

TEST(RenderPerformanceTest, ScopesCloseOnEarlyReturn)
{
    RecordingMonitor monitor;
    [&] {
        performance::PerformanceCpuScope cpu(&monitor, performance::PerfStage::Studio);
        performance::PerformanceGpuScope gpu(&monitor, performance::PerfStage::StudioShader);
        return;
    }();
    EXPECT_EQ(monitor.events, (std::vector<std::string>{"cpu.begin", "gpu.begin", "gpu.end", "cpu.end"}));
}

TEST(RenderPerformanceTest, StudioPreparationAndDeferredSubmissionCountWithoutNestedDuplication)
{
    class Clock final : public ncl_perf::PerfClockInterface
    {
    public:
        ncl_perf::PerfTimestamp_t time{};
        ncl_perf::PerfTimestamp_t Now() override
        {
            return time;
        }
    } clock;
    performance::PerformanceMonitor monitor(clock);
    unsigned int depth = 0;
    monitor.SetMode(1);
    monitor.OnHostEntry();
    monitor.OnFrameAccepted();
    {
        render::StudioCpuScope capture(monitor, depth);
        clock.time = 10;
        render::StudioCpuScope immediate_submission(monitor, depth);
        clock.time = 20;
    }

    {
        render::StudioCpuScope deferred_submission(monitor, depth);
        clock.time = 30;
    }

    monitor.OnHostExit();
    performance::PerfSnapshot snapshot;
    monitor.GetSnapshot(snapshot);
    uint32_t studio_entries = 0;
    for (uint32_t index = 0; index < snapshot.cpu_count; ++index)
    {
        const ncl_perf::PerfCpuSample& sample = snapshot.cpu_tree[index];
        if (sample.scope_id == performance::PerfStage::Studio)
        {
            ++studio_entries;
            EXPECT_EQ(sample.calls, 2u);
            EXPECT_EQ(sample.inclusive_ns, 30u);
        }
    }
    EXPECT_EQ(studio_entries, 1u);
    EXPECT_EQ(depth, 0u);
}

TEST(RenderPerformanceTest, LocalScopesUseTheRealMonitorWithoutClockReadsWhenDisabled)
{
    class Clock final : public ncl_perf::PerfClockInterface
    {
    public:
        ncl_perf::PerfTimestamp_t time{};
        uint32_t reads{};
        ncl_perf::PerfTimestamp_t Now() override
        {
            ++reads;
            return time;
        }
    } clock;
    performance::PerformanceMonitor monitor(clock);
    {
        performance::LocalCpuScope cpu(&monitor, performance::PerfStage::Studio);
        performance::LocalGpuScope gpu(&monitor, performance::PerfStage::StudioShader);
    }
    EXPECT_EQ(clock.reads, 0u);
    monitor.SetMode(1);
    monitor.OnHostEntry();
    monitor.OnFrameAccepted();
    {
        performance::LocalCpuScope cpu(monitor.get_scope_monitor(), performance::PerfStage::Studio);
        clock.time = 50;
    }
    monitor.OnHostExit();
    performance::PerfSnapshot snapshot;
    monitor.GetSnapshot(snapshot);
    uint32_t measured = 0;
    for (uint32_t index = 0; index < snapshot.cpu_count; ++index)
    {
        const ncl_perf::PerfCpuSample& sample = snapshot.cpu_tree[index];
        if (sample.scope_id == performance::PerfStage::Studio)
        {
            ++measured;
            EXPECT_EQ(sample.calls, 1u);
            EXPECT_EQ(sample.inclusive_ns, 50u);
        }
    }
    EXPECT_EQ(measured, 1u);
}

TEST(RenderPerformanceTest, ShaderBuildFailureKeepsCpuEventAndCounter)
{
    RecordingMonitor monitor;
    ncl_gl::ProgramBuildCallbacks callbacks = performance::MakeProgramBuildCallbacks(&monitor);
    uint64_t token = callbacks.Begin(callbacks.user, ncl_gl::ProgramBuildStage::Compile);
    callbacks.End(callbacks.user, token, false);
    EXPECT_EQ(monitor.counters[uint32_t(performance::PerfCounterId::ShaderCompiles)], 1u);
    EXPECT_EQ(monitor.counters[uint32_t(performance::PerfCounterId::ShaderBuildFailures)], 1u);
    EXPECT_EQ(monitor.events, (std::vector<std::string>{"cpu.begin", "cpu.end"}));
}

TEST(RenderPerformanceTest, StockFallbackCountsActualPath)
{
    EXPECT_EXIT(
        {
            goldsrc::engine_studio_api_t api{};
            api.StudioDrawPoints = StockDraw;
            g_StockCalls = 0;
            performance::PerformanceMonitor& monitor = performance::GetMonitor();
            monitor.Reset();
            monitor.SetMode(1);
            monitor.OnHostEntry();
            monitor.OnFrameAccepted();
            render::Studio_PatchApi(&api);
            api.StudioDrawPoints();
            monitor.OnHostExit();
            performance::PerfSnapshot snapshot;
            monitor.GetSnapshot(snapshot);
            EXPECT_EQ(g_StockCalls, 1);
            EXPECT_EQ(snapshot.counters[uint32_t(performance::PerfCounterId::StudioEngineSubmissions)], 1u);
            EXPECT_EQ(snapshot.counters[uint32_t(performance::PerfCounterId::StudioShaderSubmissions)], 0u);
            render::Studio_Shutdown();
            monitor.SetMode(0);
            monitor.Reset();
            std::exit(testing::Test::HasFailure() ? 1 : 0);
        },
        testing::ExitedWithCode(0),
        ""
    );
}

TEST(RenderPerformanceTest, UnresolvedWholeStagesStayUnavailable)
{
    class Clock : public ncl_perf::PerfClockInterface
    {
    public:
        ncl_perf::PerfTimestamp_t Now() override
        {
            return 100;
        }
    } clock;
    performance::PerformanceMonitor monitor(clock);
    monitor.SetMode(1);
    monitor.OnHostEntry();
    monitor.OnFrameAccepted();
    EXPECT_EQ(monitor.BeginCpu(performance::PerfStage::Commands), 0u);
    EXPECT_EQ(monitor.BeginCpu(performance::PerfStage::Prediction), 0u);
    monitor.OnHostExit();
    performance::PerfSnapshot snapshot;
    monitor.GetSnapshot(snapshot);
    uint32_t found = 0;
    for (uint32_t index = 0; index < snapshot.cpu_count; ++index)
    {
        if (snapshot.cpu_tree[index].scope_id == performance::PerfStage::Commands ||
            snapshot.cpu_tree[index].scope_id == performance::PerfStage::Prediction)
        {
            ++found;
            EXPECT_EQ(snapshot.cpu_tree[index].state, ncl_perf::PerfSampleState::Unavailable);
        }
    }
    EXPECT_EQ(found, 2u);
}

TEST(RenderPerformanceTest, MissingResolvedStageIsUnavailable)
{
    class Clock : public ncl_perf::PerfClockInterface
    {
    public:
        ncl_perf::PerfTimestamp_t Now() override
        {
            return 100;
        }
    } clock;
    performance::PerformanceMonitor monitor(clock);
    monitor.SetStageAvailable(performance::PerfStage::World, false);
    monitor.SetMode(1);
    monitor.OnHostEntry();
    monitor.OnFrameAccepted();
    EXPECT_EQ(monitor.BeginCpu(performance::PerfStage::World), 0u);
    monitor.OnHostExit();
    performance::PerfSnapshot snapshot;
    monitor.GetSnapshot(snapshot);
    uint32_t found = 0;
    for (uint32_t index = 0; index < snapshot.cpu_count; ++index)
    {
        if (snapshot.cpu_tree[index].scope_id == performance::PerfStage::World)
        {
            ++found;
            EXPECT_EQ(snapshot.cpu_tree[index].state, ncl_perf::PerfSampleState::Unavailable);
        }
    }
    EXPECT_EQ(found, 1u);
}

TEST(RenderPerformanceTest, StudioShaderIsEnabledByDefault)
{
    constexpr cvars::CvarDefinition definition = render::FindCvar("r_studio_shader");
    ASSERT_NE(definition.name, nullptr);
    EXPECT_STREQ(definition.default_value, "1");
    EXPECT_EQ(definition.flags, goldsrc::FCVAR_ARCHIVE);
}

namespace
{
    class GaugeQueueSink : public render::StudioSubmissionSink
    {
    public:
        uint32_t draws{};

        void Begin() override {}
        void Draw(const render::StudioDrawPacket&) override
        {
            ++draws;
        }
        void End() override {}
    };

    render::StudioDrawPacket GaugePacket()
    {
        render::StudioDrawPacket packet;
        packet.meshes.emplace_back();
        packet.meshes.back().index_count = 3;

        return packet;
    }
} // namespace

TEST(RenderPerformanceTest, GaugeAdapterPublishesRetainedPeakAfterQueueDrainsAndEmptyTraversalZero)
{
    RecordingMonitor monitor;
    GaugeQueueSink sink;
    render::StudioDrawQueue queue(sink);
    queue.BeginOpaquePass(true);
    for (uint32_t index = 0; index < 3; ++index)
    {
        queue.Submit(GaugePacket());
    }
    queue.Flush(render::StudioFlushReason::Brush);
    queue.Submit(GaugePacket());
    queue.EndOpaquePass();
    ASSERT_EQ(sink.draws, 4u);
    EXPECT_EQ(queue.stats().max_run, 3u);
    performance::reporting::StudioQueuePeak(&monitor, queue.stats().max_run);
    EXPECT_EQ(monitor.gauge_writes, 1u);
    EXPECT_EQ(monitor.gauges[0].state, ncl_perf::PerfSampleState::Measured);
    EXPECT_DOUBLE_EQ(monitor.gauges[0].value, 3);

    queue.BeginOpaquePass(true);
    queue.EndOpaquePass();
    performance::reporting::StudioQueuePeak(&monitor, queue.stats().max_run);
    EXPECT_EQ(monitor.gauge_writes, 2u);
    EXPECT_DOUBLE_EQ(monitor.gauges[0].value, 0);
    performance::reporting::StudioQueuePeak(nullptr, queue.stats().max_run);
}

TEST(RenderPerformanceTest, GaugeLastCompletedTraversalReplacesEarlierPeakWithinAcceptedHost)
{
    class Clock : public ncl_perf::PerfClockInterface
    {
    public:
        ncl_perf::PerfTimestamp_t Now() override
        {
            return 100;
        }
    } clock;
    performance::PerformanceMonitor monitor(clock);
    monitor.SetMode(1);
    monitor.OnHostEntry();
    monitor.OnFrameAccepted();
    GaugeQueueSink sink;
    render::StudioDrawQueue queue(sink);
    for (uint32_t packets : {3u, 2u})
    {
        queue.BeginOpaquePass(true);
        for (uint32_t index = 0; index < packets; ++index)
        {
            queue.Submit(GaugePacket());
        }
        queue.EndOpaquePass();
        performance::reporting::StudioQueuePeak(&monitor, queue.stats().max_run);
    }
    monitor.OnHostExit();
    performance::PerfSnapshot snapshot;
    monitor.GetSnapshot(snapshot);
    EXPECT_EQ(sink.draws, 5u);
    EXPECT_EQ(snapshot.gauges[0].state, ncl_perf::PerfSampleState::Measured);
    EXPECT_DOUBLE_EQ(snapshot.gauges[0].value, 2);
    EXPECT_DOUBLE_EQ(snapshot.gauge_summaries[0].maximum, 2);
}
