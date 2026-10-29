#include <gtest/gtest.h>

#include <array>
#include <string>

#include "performance/performance_dump.h"
#include "performance/PerformanceMonitor.h"
#include "performance/PerformanceTraceBackend.h"
#include "performance/performance_stages.h"
#include "performance/GpuQueryBackend.h"

namespace
{
    class LifecycleClock : public ncl_perf::PerfClockInterface
    {
    public:
        uint64_t time{};
        uint64_t reads{};
        uint64_t Now() override
        {
            ++reads;
            return time;
        }
    };

    class LifecycleQueries : public performance::GpuQueryBackendInterface
    {
    public:
        uint32_t allocations{};
        uint32_t deletions{};
        uint32_t writes{};
        uint32_t polls{};
        uint32_t reads{};
        bool ready{};
        bool IsTimestampSupported() const override
        {
            return true;
        }
        int counter_bits() const override
        {
            return 64;
        }
        void Allocate(uint32_t count, uint32_t* names) override
        {
            ++allocations;
            for (uint32_t index = 0; index < count; ++index)
            {
                names[index] = index + 1;
            }
        }
        void Delete(uint32_t, const uint32_t*) override
        {
            ++deletions;
        }
        void Write(uint32_t) override
        {
            ++writes;
        }
        bool IsReady(uint32_t) override
        {
            ++polls;
            return ready;
        }
        uint64_t Read(uint32_t name) override
        {
            ++reads;
            return name * 100;
        }
    };

    class PerformanceLifecycleTest : public testing::Test
    {
    protected:
        LifecycleClock clock;
        LifecycleQueries queries;
        performance::PerformanceMonitor monitor{clock};
        int window{};
        int first_context{};
        int second_context{};

        void SetUp() override
        {
            monitor.SetGpuBackend(&queries);
            monitor.SetMainWindow(&window);
            monitor.SetMode(1);
            monitor.OnContextCreated(&first_context);
        }
        void TearDown() override
        {
            monitor.Shutdown();
        }
        void Present()
        {
            performance::PerfOutputId_t output = monitor.BeginOutput(&window);
            clock.time += 10000000;
            monitor.EndOutput(output);
        }
    };
} // namespace

TEST_F(PerformanceLifecycleTest, LiveShutdownReleasesOnceAndRejectsStaleClosures)
{
    ncl_perf::PerfScopeToken_t old = monitor.BeginGpu(performance::PerfStage::RadarBatch);
    monitor.OnContextDestroyed();
    monitor.Shutdown();
    EXPECT_EQ(queries.deletions, 1u);
    uint32_t writes = queries.writes;
    monitor.EndGpu(old);
    monitor.OnContextDestroyed();
    monitor.Shutdown();
    EXPECT_EQ(queries.deletions, 1u);
    EXPECT_EQ(queries.writes, writes);
    EXPECT_EQ(monitor.BeginCpu(performance::PerfStage::Radar), 0u);
    EXPECT_EQ(monitor.BeginGpu(performance::PerfStage::RadarBatch), 0u);
    EXPECT_EQ(monitor.BeginOutput(&window), 0u);
}

TEST_F(PerformanceLifecycleTest, SameContextResizeKeepsPendingQueriesAndGeneration)
{
    ncl_perf::PerfScopeToken_t token = monitor.BeginGpu(performance::PerfStage::RadarBatch);
    monitor.EndGpu(token);
    Present();
    performance::PerfSnapshot before;
    monitor.GetSnapshot(before);
    monitor.OnContextCreated(&first_context);
    EXPECT_EQ(queries.allocations, 1u);
    EXPECT_EQ(queries.deletions, 0u);
    queries.ready = true;
    monitor.PollGpu();
    performance::PerfSnapshot after;
    monitor.GetSnapshot(after);
    EXPECT_EQ(after.context_generation, before.context_generation);
    ASSERT_EQ(after.gpu_count, 1u);
    EXPECT_EQ(after.gpu_passes[0].state, ncl_perf::PerfSampleState::Measured);
}

TEST_F(PerformanceLifecycleTest, ReplacementAbandonsOldNamesAndCannotCloseAgainstReusedNames)
{
    ncl_perf::PerfScopeToken_t old = monitor.BeginGpu(performance::PerfStage::RadarBatch);
    monitor.OnContextCreated(&second_context);
    EXPECT_EQ(queries.allocations, 2u);
    EXPECT_EQ(queries.deletions, 0u);
    ncl_perf::PerfScopeToken_t fresh = monitor.BeginGpu(performance::PerfStage::RadarBatch);
    uint32_t writes = queries.writes;
    monitor.EndGpu(old);
    EXPECT_EQ(queries.writes, writes);
    monitor.EndGpu(fresh);
    EXPECT_EQ(queries.writes, writes + 1);
    performance::PerfSnapshot snapshot;
    monitor.GetSnapshot(snapshot);
    EXPECT_EQ(snapshot.context_generation, 2u);
}

TEST_F(PerformanceLifecycleTest, ContextRecreationRetainsWindowAndTheRealPresentationInterval)
{
    Present();
    Present();
    monitor.OnContextDestroyed();
    EXPECT_TRUE(monitor.is_main_window(&window));
    clock.time += 250000000;
    monitor.OnContextCreated(&second_context);
    Present();
    performance::PerfSnapshot snapshot;
    monitor.GetSnapshot(snapshot);
    EXPECT_EQ(snapshot.latest.values[uint32_t(performance::PerfSeries::FrameInterval)].state, ncl_perf::PerfSampleState::Measured);
    EXPECT_EQ(snapshot.latest.values[uint32_t(performance::PerfSeries::FrameInterval)].duration_ns, 260000000u);
}

TEST_F(PerformanceLifecycleTest, MapTransitionRetiresOldPendingValuesAndClearsCurrentRows)
{
    ncl_perf::PerfScopeToken_t token = monitor.BeginGpu(performance::PerfStage::RadarBatch);
    monitor.EndGpu(token);
    Present();
    monitor.OnMapChanged();
    performance::PerfSnapshot transition;
    monitor.GetSnapshot(transition);
    EXPECT_EQ(transition.latest.values[uint32_t(performance::PerfSeries::RadarGpu)].state, ncl_perf::PerfSampleState::Incomplete);
    queries.ready = true;
    monitor.PollGpu();
    performance::PerfSnapshot snapshot;
    monitor.GetSnapshot(snapshot);
    EXPECT_EQ(snapshot.gpu_count, 0u);
    EXPECT_EQ(snapshot.record_count, 1u);
    EXPECT_NE(snapshot.latest.values[uint32_t(performance::PerfSeries::RadarGpu)].state, ncl_perf::PerfSampleState::Pending);
}

TEST_F(PerformanceLifecycleTest, DroppedCpuScopesAreReportedWithIncompleteHostTiming)
{
    monitor.OnHostEntry();
    monitor.OnFrameAccepted();
    std::array<ncl_perf::PerfScopeToken_t, ncl_perf::kPerfStackCapacity> scopes{};
    for (ncl_perf::PerfScopeToken_t& token : scopes)
    {
        token = monitor.BeginCpu(performance::PerfStage::Radar);
    }
    for (uint32_t index = uint32_t(scopes.size()); index > 0; --index)
    {
        monitor.EndCpu(scopes[index - 1]);
    }
    monitor.OnHostExit();
    performance::PerfSnapshot snapshot;
    monitor.GetSnapshot(snapshot);
    EXPECT_EQ(snapshot.counters[uint32_t(performance::PerfCounterId::CpuDroppedScopes)], 1u);
    EXPECT_EQ(snapshot.cpu_tree[0].state, ncl_perf::PerfSampleState::Incomplete);
}

TEST_F(PerformanceLifecycleTest, CpuFrameIdentityIsSeparateFromScreenOnlyOutputs)
{
    monitor.OnHostEntry();
    monitor.OnFrameAccepted();
    monitor.OnHostExit();
    Present();
    performance::PerfSnapshot snapshot;
    monitor.GetSnapshot(snapshot);
    EXPECT_EQ(snapshot.latest.frame_id, 0u);
    EXPECT_EQ(snapshot.cpu_frame_id, 1u);
}

TEST_F(PerformanceLifecycleTest, AnUnmeasuredShaderBuildIsDifferentFromMeasuredZero)
{
    performance::PerfSnapshot snapshot;
    monitor.GetSnapshot(snapshot);
    EXPECT_EQ(snapshot.shader_compile_state, ncl_perf::PerfSampleState::Unavailable);
    ncl_perf::PerfScopeToken_t token = monitor.BeginCpu(performance::PerfStage::ShaderCompile);
    monitor.EndCpu(token);
    monitor.GetSnapshot(snapshot);
    EXPECT_EQ(snapshot.shader_compile_ns, 0u);
    EXPECT_EQ(snapshot.shader_compile_state, ncl_perf::PerfSampleState::Measured);
    EXPECT_EQ(snapshot.shader_link_state, ncl_perf::PerfSampleState::Unavailable);
}

TEST_F(PerformanceLifecycleTest, RetainedGpuResultsCanCompleteAfterTenSeconds)
{
    ncl_perf::PerfScopeToken_t token = monitor.BeginGpu(performance::PerfStage::RadarBatch);
    monitor.EndGpu(token);
    Present();
    clock.time += 11000000000;
    Present();
    queries.ready = true;
    monitor.PollGpu();
    performance::PerfSnapshot snapshot;
    monitor.GetSnapshot(snapshot);
    EXPECT_EQ(snapshot.gpu_count, 1u);
    EXPECT_EQ(snapshot.counters[uint32_t(performance::PerfCounterId::GpuExpiredResults)], 0u);
}

TEST_F(PerformanceLifecycleTest, OverwrittenGpuResultsDoNotBecomeTheCurrentPassRows)
{
    ncl_perf::PerfScopeToken_t token = monitor.BeginGpu(performance::PerfStage::RadarBatch);
    monitor.EndGpu(token);
    Present();
    for (uint32_t index = 0; index < performance::kPerfHistoryCapacity; ++index)
    {
        monitor.OnHostEntry();
        monitor.OnFrameAccepted();
        ++clock.time;
        monitor.OnHostExit();
    }
    queries.ready = true;
    monitor.PollGpu();
    performance::PerfSnapshot snapshot;
    monitor.GetSnapshot(snapshot);
    EXPECT_EQ(snapshot.gpu_count, 0u);
    EXPECT_EQ(snapshot.counters[uint32_t(performance::PerfCounterId::GpuExpiredResults)], 1u);
}

TEST_F(PerformanceLifecycleTest, DisabledDumpDoesNotAllocateOrWriteGpuQueries)
{
    monitor.SetMode(0);
    uint32_t allocations = queries.allocations;
    uint32_t writes = queries.writes;
    ASSERT_TRUE(monitor.RequestDumpCapture());
    monitor.OnHostEntry();
    monitor.OnFrameAccepted();
    EXPECT_EQ(monitor.BeginGpu(performance::PerfStage::StudioShader), 0u);
    Present();
    monitor.OnHostExit();
    performance::PerfSnapshot captured;
    EXPECT_TRUE(monitor.TakeDumpCapture(captured));
    EXPECT_EQ(queries.allocations, allocations);
    EXPECT_EQ(queries.writes, writes);
}

TEST_F(PerformanceLifecycleTest, EnablingMonitoringDuringDisabledDumpStartsGpuCollection)
{
    monitor.SetMode(0);
    uint32_t allocations = queries.allocations;
    ASSERT_TRUE(monitor.RequestDumpCapture());
    monitor.OnHostEntry();
    monitor.OnFrameAccepted();
    EXPECT_EQ(queries.allocations, allocations);
    monitor.SetMode(1);
    monitor.OnHostExit();
    EXPECT_EQ(monitor.get_mode(), 1);
    EXPECT_GT(queries.allocations, allocations);
    ncl_perf::PerfScopeToken_t gpu = monitor.BeginGpu(performance::PerfStage::StudioShader);
    EXPECT_NE(gpu, 0u);
    monitor.EndGpu(gpu);
}
TEST_F(PerformanceLifecycleTest, DetailedIndependentBatchingDoesNotEnableGpuCollection)
{
    monitor.SetMode(0);
    uint32_t allocations = queries.allocations;
    uint32_t writes = queries.writes;
    monitor.SetBatchLevel(2);
    EXPECT_NE(monitor.get_scope_monitor(), nullptr);
    EXPECT_NE(monitor.get_counter_monitor(), nullptr);
    monitor.OnHostEntry();
    monitor.OnFrameAccepted();
    uint64_t reads = clock.reads;
    ncl_perf::PerfScopeToken_t studio = monitor.BeginCpu(performance::PerfStage::Studio);
    ncl_perf::PerfScopeToken_t live_detail = monitor.BeginCpu(performance::PerfStage::StudioDataPrepare);
    EXPECT_NE(studio, 0u);
    EXPECT_NE(live_detail, 0u);
    monitor.EndCpu(live_detail);
    monitor.EndCpu(studio);
    EXPECT_EQ(monitor.BeginGpu(performance::PerfStage::StudioShader), 0u);
    EXPECT_GT(clock.reads, reads);
    monitor.AddCounter(performance::PerfCounterId::StudioPackets, 5);
    Present();
    monitor.OnHostExit();
    performance::PerfSnapshot snapshot;
    monitor.GetSnapshot(snapshot);
    EXPECT_EQ(snapshot.mode, 0);
    EXPECT_EQ(snapshot.cpu_frame_count, 1u);
    EXPECT_EQ(monitor.get_batch_snapshot()->cpu_frame_count, 1u);
    EXPECT_EQ(snapshot.counters[uint32_t(performance::PerfCounterId::StudioPackets)], 5u);
    EXPECT_EQ(queries.allocations, allocations);
    EXPECT_EQ(queries.writes, writes);
    monitor.Reset();
    monitor.GetSnapshot(snapshot);
    EXPECT_EQ(snapshot.counters[uint32_t(performance::PerfCounterId::StudioPackets)], 0u);
    ASSERT_TRUE(monitor.RequestDumpCapture());
    monitor.OnHostEntry();
    monitor.OnFrameAccepted();
    ncl_perf::PerfScopeToken_t detail = monitor.BeginCpu(performance::PerfStage::StudioDataPrepare);
    EXPECT_NE(detail, 0u);
    monitor.EndCpu(detail);
    monitor.OnHostExit();
    ASSERT_TRUE(monitor.TakeDumpCapture(snapshot));
    EXPECT_EQ(snapshot.cpu_frame_count, 1u);
    EXPECT_EQ(monitor.get_mode(), 0);
    EXPECT_EQ(monitor.get_batch_level(), 2);
    EXPECT_EQ(queries.allocations, allocations);
    monitor.SetBatchLevel(0);
    EXPECT_EQ(monitor.get_counter_monitor(), nullptr);
    monitor.Shutdown();
    uint64_t shutdown_reads = clock.reads;
    monitor.OnHostEntry();
    monitor.OnFrameAccepted();
    monitor.OnHostExit();
    monitor.GetSnapshot(snapshot);
    EXPECT_EQ(clock.reads, shutdown_reads);
}


namespace
{
    class LifecycleTrace : public performance::PerformanceTraceBackendInterface
    {
    public:
        uint32_t gpu_begins{};
        uint32_t gpu_ends{};
        std::array<bool, 32> open_gpu{};
        uint32_t outputs{};

        bool IsConnected() const override
        {
            return true;
        }
        ncl_perf::PerfScopeToken_t BeginCpu(ncl_perf::PerfScopeId_t, std::source_location) override
        {
            return 0;
        }
        void EndCpu(ncl_perf::PerfScopeToken_t) override {}
        ncl_perf::PerfScopeToken_t BeginGpu(ncl_perf::PerfScopeId_t, std::source_location) override
        {
            open_gpu[++gpu_begins] = true;
            return gpu_begins;
        }
        void EndGpu(ncl_perf::PerfScopeToken_t token) override
        {
            EXPECT_TRUE(open_gpu[token]);
            open_gpu[token] = false;
            ++gpu_ends;
        }
        void Collect() override {}
        void RecordCounter(performance::PerfCounterId, uint64_t) override {}
        void RecordGauge(performance::PerfGaugeId, double) override {}
        void MarkFrame(bool output, uint64_t) override
        {
            outputs += output;
        }
    };

    class PerformanceGpuReleaseTest : public PerformanceLifecycleTest, public testing::WithParamInterface<int>
    {
    protected:
        LifecycleQueries replacement;
        LifecycleTrace trace;

        void ReleaseAndResume()
        {
            switch (GetParam())
            {
                case 0:
                    monitor.SetGpuEnabled(false);
                    monitor.SetGpuEnabled(true);
                    break;
                case 1:
                    monitor.SetMode(0);
                    monitor.SetMode(1);
                    break;
                case 2:
                    monitor.SetGpuBackend(&replacement);
                    break;
            }
        }

        void TearDown() override
        {
            monitor.Shutdown();
            monitor.SetTraceBackend(nullptr);
        }
    };

    std::string FormatLifecycleDump(const performance::PerfSnapshot& snapshot)
    {
        std::array<char, 32768> text{};
        performance::dump::Format(snapshot, text.data(), uint32_t(text.size()));
        return text.data();
    }
} // namespace

TEST_P(PerformanceGpuReleaseTest, RetiresPendingHistoryAndPreservesCompletedPassesAndSavedDump)
{
    queries.ready = true;
    ncl_perf::PerfScopeToken_t complete = monitor.BeginGpu(performance::PerfStage::StudioShader);
    monitor.EndGpu(complete);
    Present();
    queries.ready = false;
    ncl_perf::PerfScopeToken_t pending = monitor.BeginGpu(performance::PerfStage::RadarBatch);
    monitor.EndGpu(pending);
    Present();

    ASSERT_TRUE(monitor.RequestDumpCapture());
    monitor.OnHostEntry();
    monitor.OnFrameAccepted();
    monitor.AddCounter(performance::PerfCounterId::StudioPackets, 7);
    clock.time += 123;
    monitor.OnHostExit();
    ASSERT_NE(monitor.get_dump_snapshot(), nullptr);
    std::string saved_dump = FormatLifecycleDump(*monitor.get_dump_snapshot());
    clock.time = 260000000;
    monitor.AdvanceSnapshot();
    performance::PerfSnapshot before;
    monitor.GetSnapshot(before);
    ASSERT_EQ(before.latest.values[uint32_t(performance::PerfSeries::RadarGpu)].state, ncl_perf::PerfSampleState::Pending);
    ASSERT_EQ(before.gpu_count, 1u);

    ReleaseAndResume();

    performance::PerfSnapshot after;
    monitor.GetSnapshot(after);
    EXPECT_EQ(after.latest.values[uint32_t(performance::PerfSeries::RadarGpu)].state, ncl_perf::PerfSampleState::Incomplete);
    EXPECT_EQ(after.latest.values[uint32_t(performance::PerfSeries::RadarGpu)].duration_ns, 0u);
    EXPECT_EQ(after.summaries[uint32_t(performance::PerfSeries::RadarGpu)].state, ncl_perf::PerfSampleState::Incomplete);
    EXPECT_EQ(after.summaries[uint32_t(performance::PerfSeries::StudioGpu)].count, 1u);
    EXPECT_EQ(
        after.summaries[uint32_t(performance::PerfSeries::StudioGpu)].mean_ns,
        before.summaries[uint32_t(performance::PerfSeries::StudioGpu)].mean_ns
    );
    EXPECT_EQ(
        after.summaries[uint32_t(performance::PerfSeries::HostCpu)].mean_ns,
        before.summaries[uint32_t(performance::PerfSeries::HostCpu)].mean_ns
    );
    EXPECT_EQ(after.record_count, before.record_count);
    EXPECT_EQ(after.counters, before.counters);
    EXPECT_EQ(after.collector_generation, before.collector_generation);
    EXPECT_EQ(after.context_generation, before.context_generation);
    EXPECT_EQ(after.map_generation, before.map_generation);
    EXPECT_EQ(after.gpu_output_id, before.gpu_output_id);
    ASSERT_EQ(after.gpu_count, before.gpu_count);
    EXPECT_EQ(after.gpu_passes[0].scope_id, before.gpu_passes[0].scope_id);
    EXPECT_EQ(after.gpu_passes[0].duration_ns, before.gpu_passes[0].duration_ns);
    EXPECT_EQ(after.gpu_passes[0].calls, before.gpu_passes[0].calls);
    EXPECT_EQ(after.gpu_passes[0].state, ncl_perf::PerfSampleState::Measured);
    EXPECT_EQ(FormatLifecycleDump(*monitor.get_dump_snapshot()), saved_dump);
    ASSERT_TRUE(monitor.TakeDumpCapture(after));

    ASSERT_TRUE(monitor.RequestDumpCapture());
    monitor.OnHostEntry();
    monitor.OnFrameAccepted();
    monitor.OnHostExit();
    ASSERT_NE(monitor.get_dump_snapshot(), nullptr);
    EXPECT_EQ(
        monitor.get_dump_snapshot()->summaries[uint32_t(performance::PerfSeries::RadarGpu)].state, ncl_perf::PerfSampleState::Incomplete
    );
    EXPECT_EQ(FormatLifecycleDump(*monitor.get_dump_snapshot()).find("Radar GPU: Pending"), std::string::npos);
}

TEST_P(PerformanceGpuReleaseTest, RetiresOpenDraftWithoutClosingTraceOrWritingThroughOldToken)
{
    monitor.SetTraceBackend(&trace);
    ncl_perf::PerfScopeToken_t screen = monitor.BeginGpu(performance::PerfStage::Screen);
    ncl_perf::PerfScopeToken_t studio = monitor.BeginGpu(performance::PerfStage::StudioShader);
    monitor.EndGpu(studio);
    performance::PerfOutputId_t output = monitor.BeginOutput(&window);

    ReleaseAndResume();
    EXPECT_EQ(trace.gpu_ends, 1u);
    ncl_perf::PerfScopeToken_t fresh_studio = monitor.BeginGpu(performance::PerfStage::RadarBatch);
    ASSERT_NE(fresh_studio, 0u);
    uint32_t writes = queries.writes;
    uint32_t replacement_writes = replacement.writes;
    monitor.EndGpu(screen);
    monitor.EndGpu(screen);
    EXPECT_EQ(queries.writes, writes);
    EXPECT_EQ(replacement.writes, replacement_writes);
    EXPECT_EQ(trace.gpu_ends + 1, trace.gpu_begins);
    monitor.EndGpu(fresh_studio);
    EXPECT_EQ(trace.gpu_ends, trace.gpu_begins);

    monitor.EndOutput(output);
    performance::PerfSnapshot snapshot;
    monitor.GetSnapshot(snapshot);
    if (GetParam() != 1)
    {
        EXPECT_EQ(snapshot.latest.output_id, output);
        EXPECT_EQ(snapshot.latest.values[uint32_t(performance::PerfSeries::RenderGpu)].state, ncl_perf::PerfSampleState::Incomplete);
        EXPECT_EQ(snapshot.latest.values[uint32_t(performance::PerfSeries::StudioGpu)].state, ncl_perf::PerfSampleState::Incomplete);
        EXPECT_EQ(snapshot.latest.values[uint32_t(performance::PerfSeries::StudioGpu)].duration_ns, 0u);
    }
    ncl_perf::PerfScopeToken_t fresh = monitor.BeginGpu(performance::PerfStage::Screen);
    ASSERT_NE(fresh, 0u);
    monitor.EndGpu(screen);
    EXPECT_EQ(trace.gpu_ends + 1, trace.gpu_begins);
    Present();
    monitor.EndGpu(fresh);
    EXPECT_EQ(trace.gpu_ends, trace.gpu_begins);
    EXPECT_EQ(trace.outputs, 2u);
}

TEST_P(PerformanceGpuReleaseTest, RetiresStagedValuesBeforeOutputDraftExists)
{
    ncl_perf::PerfScopeToken_t screen = monitor.BeginGpu(performance::PerfStage::Screen);
    ncl_perf::PerfScopeToken_t studio = monitor.BeginGpu(performance::PerfStage::StudioShader);
    monitor.EndGpu(studio);
    ReleaseAndResume();
    Present();
    monitor.EndGpu(screen);
    performance::PerfSnapshot snapshot;
    monitor.GetSnapshot(snapshot);
    EXPECT_NE(snapshot.latest.values[uint32_t(performance::PerfSeries::StudioGpu)].state, ncl_perf::PerfSampleState::Pending);
    EXPECT_EQ(snapshot.latest.values[uint32_t(performance::PerfSeries::StudioGpu)].duration_ns, 0u);
}

INSTANTIATE_TEST_SUITE_P(GpuMonitorAndBackend, PerformanceGpuReleaseTest, testing::Values(0, 1, 2));

TEST_F(PerformanceLifecycleTest, RepeatedHostAbortsRetireCpuAndGpuScopes)
{
    for (uint32_t index = 0; index < 2 * ncl_perf::kPerfScopeCapacity; ++index)
    {
        monitor.OnHostEntry();
        monitor.OnFrameAccepted();
        ncl_perf::PerfScopeToken_t cpu = monitor.BeginCpu(performance::PerfStage::Packets);
        ncl_perf::PerfScopeToken_t gpu = monitor.BeginGpu(performance::PerfStage::RadarBatch);
        ASSERT_NE(cpu, 0u);
        ASSERT_NE(gpu, 0u);
        monitor.OnHostExit(false);
        uint32_t writes = queries.writes;
        monitor.EndCpu(cpu);
        monitor.EndGpu(gpu);
        EXPECT_EQ(queries.writes, writes);
        queries.ready = true;
        monitor.PollGpu();
    }
    monitor.OnHostEntry();
    monitor.OnFrameAccepted();
    ncl_perf::PerfScopeToken_t studio = monitor.BeginCpu(performance::PerfStage::Studio);
    ncl_perf::PerfScopeToken_t radar = monitor.BeginGpu(performance::PerfStage::RadarBatch);
    ASSERT_NE(studio, 0u);
    ASSERT_NE(radar, 0u);
    clock.time += 100;
    monitor.EndCpu(studio);
    monitor.EndGpu(radar);
    Present();
    monitor.PollGpu();
    monitor.OnHostExit();
    performance::PerfSnapshot complete;
    monitor.GetSnapshot(complete);
    EXPECT_EQ(complete.gpu_count, 1u);
    EXPECT_EQ(complete.record_count, 2u);
    EXPECT_EQ(complete.latest.values[uint32_t(performance::PerfSeries::RadarGpu)].state, ncl_perf::PerfSampleState::Measured);
}

TEST_F(PerformanceLifecycleTest, CpuShutdownAbandonsGpuWithoutDeletingAndRejectsLateClosures)
{
    ncl_perf::PerfScopeToken_t pending = monitor.BeginGpu(performance::PerfStage::RadarBatch);
    ASSERT_NE(pending, 0u);
    monitor.Shutdown();
    EXPECT_EQ(queries.deletions, 0u);
    uint32_t writes = queries.writes;
    uint32_t polls = queries.polls;
    uint32_t reads = queries.reads;
    queries.ready = true;
    monitor.EndGpu(pending);
    monitor.PollGpu();
    monitor.Shutdown();
    EXPECT_EQ(queries.writes, writes);
    EXPECT_EQ(queries.deletions, 0u);
    EXPECT_EQ(queries.polls, polls);
    EXPECT_EQ(queries.reads, reads);
    performance::PerfSnapshot snapshot;
    monitor.GetSnapshot(snapshot);
    EXPECT_EQ(snapshot.gpu_count, 0u);
}

TEST_F(PerformanceLifecycleTest, AbandonmentAndCpuShutdownNeverDeleteTheRetiredGeneration)
{
    ncl_perf::PerfScopeToken_t pending = monitor.BeginGpu(performance::PerfStage::StudioShader);
    ASSERT_NE(pending, 0u);
    monitor.AbandonGpu();
    monitor.AbandonGpu();
    uint32_t writes = queries.writes;
    monitor.EndGpu(pending);
    monitor.Shutdown();
    EXPECT_EQ(queries.deletions, 0u);
    EXPECT_EQ(queries.writes, writes);
}

TEST_F(PerformanceLifecycleTest, GpuAbandonmentPreservesCompletedCpuHistoryAndDoesNotRestartTheWindow)
{
    monitor.OnHostEntry();
    monitor.OnFrameAccepted();
    ncl_perf::PerfScopeToken_t cpu = monitor.BeginCpu(performance::PerfStage::Radar);
    ASSERT_NE(cpu, 0u);
    clock.time = 12345;
    monitor.EndCpu(cpu);
    Present();
    monitor.OnHostExit();
    clock.time = 200000000;
    monitor.AdvanceSnapshot();

    performance::PerfSnapshot before;
    ASSERT_TRUE(monitor.GetSnapshot(before));
    ASSERT_EQ(before.record_count, 2u);
    ASSERT_EQ(before.latest.kind, performance::PerfRecordKind::Output);
    ASSERT_NE(before.cpu_frame_id, 0u);
    ASSERT_EQ(before.latest.frame_id, before.cpu_frame_id);
    ASSERT_GT(before.cpu_count, 0u);
    ASSERT_EQ(before.cpu_frame_count, 1u);
    ASSERT_GT(before.cpu_window_ns, 0u);
    uint32_t cpu_series = static_cast<uint32_t>(performance::PerfSeries::RadarCpu);
    performance::PerfSummary sample = before.summaries[cpu_series];
    ASSERT_EQ(sample.state, ncl_perf::PerfSampleState::Measured);
    ASSERT_EQ(sample.count, 1u);
    ASSERT_EQ(sample.mean_ns, 12345u);

    monitor.AbandonGpu();

    performance::PerfSnapshot after;
    ASSERT_TRUE(monitor.GetSnapshot(after));
    EXPECT_EQ(after.record_count, before.record_count);
    EXPECT_EQ(after.latest.frame_id, before.latest.frame_id);
    EXPECT_EQ(after.summaries[cpu_series].state, sample.state);
    EXPECT_EQ(after.summaries[cpu_series].count, sample.count);
    EXPECT_EQ(after.summaries[cpu_series].mean_ns, sample.mean_ns);
    EXPECT_EQ(after.cpu_count, before.cpu_count);
    EXPECT_EQ(after.cpu_frame_id, before.cpu_frame_id);
    EXPECT_EQ(after.cpu_frame_count, before.cpu_frame_count);
    EXPECT_EQ(after.cpu_window_ns, before.cpu_window_ns);
}
