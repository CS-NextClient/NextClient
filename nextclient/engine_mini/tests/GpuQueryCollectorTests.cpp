#include <gtest/gtest.h>

#include <array>
#include <vector>

#include "performance/GpuQueryCollector.h"
#include "performance/PerformanceMonitor.h"
#include "performance/performance_stages.h"

namespace
{
    class QueryBackend : public performance::GpuQueryBackendInterface
    {
    public:
        bool supported = true;
        int bits = 64;
        uint64_t time = 1000;
        uint32_t reads{};
        uint32_t allocated{};
        uint32_t deleted{};
        std::array<uint64_t, 2049> timestamps{};
        std::array<bool, 2049> ready{};
        std::vector<uint32_t> writes;

        bool IsTimestampSupported() const override
        {
            return supported;
        }
        int counter_bits() const override
        {
            return bits;
        }
        void Allocate(uint32_t count, uint32_t* names) override
        {
            allocated += count;
            for (uint32_t index = 0; index < count; ++index)
            {
                names[index] = index + 1;
            }
        }
        void Delete(uint32_t count, const uint32_t*) override
        {
            deleted += count;
        }
        void Write(uint32_t name) override
        {
            timestamps[name] = time;
            ready[name] = false;
            writes.push_back(name);
        }
        bool IsReady(uint32_t name) override
        {
            return ready[name];
        }
        uint64_t Read(uint32_t name) override
        {
            EXPECT_TRUE(ready[name]);
            ++reads;
            return timestamps[name];
        }
    };

    class GpuQueryCollectorTest : public testing::Test
    {
    protected:
        QueryBackend backend;

        void Submit(performance::GpuQueryCollector& collector, ncl_perf::PerfScopeId_t scope, uint32_t count)
        {
            for (uint32_t index = 0; index < count; ++index)
            {
                ncl_perf::PerfScopeToken_t token = collector.Begin(1, 1, scope);
                ASSERT_NE(token, 0u);

                backend.time += 10;
                collector.End(token);
            }
        }

        std::vector<performance::PerfGpuResult> Collect(performance::GpuQueryCollector& collector)
        {
            std::array<performance::PerfGpuResult, 256> results;
            uint32_t count = collector.Poll(results.data(), uint32_t(results.size()));

            return {results.begin(), results.begin() + count};
        }

        uint32_t Count(
            const std::vector<performance::PerfGpuResult>& results,
            ncl_perf::PerfScopeId_t scope,
            ncl_perf::PerfSampleState state,
            performance::PerfSeries series
        )
        {
            uint32_t count = 0;

            for (const performance::PerfGpuResult& result : results)
            {
                if (result.scope_id == scope && result.state == state && result.series == series)
                {
                    ++count;
                }
            }

            return count;
        }
    };
} // namespace

TEST_F(GpuQueryCollectorTest, ReadsOnlyAvailableResults)
{
    performance::GpuQueryCollector collector(backend, 3, 4);
    ncl_perf::PerfScopeToken_t token = collector.Begin(5, 6, performance::PerfStage::StudioShader);
    backend.time = 7000;
    collector.End(token);
    performance::PerfGpuResult result;
    EXPECT_EQ(collector.Poll(&result, 1), 0u);
    EXPECT_EQ(backend.reads, 0u);
    backend.ready.fill(true);
    ASSERT_EQ(collector.Poll(&result, 1), 1u);
    EXPECT_EQ(result.duration_ns, 6000u);
    EXPECT_EQ(result.frame_id, 5u);
    EXPECT_EQ(result.output_id, 6u);
    EXPECT_EQ(result.collector_generation, 3u);
    EXPECT_EQ(result.context_generation, 4u);
    EXPECT_EQ(result.series, performance::PerfSeries::StudioGpu);
    collector.ReleaseGl();
}

TEST_F(GpuQueryCollectorTest, NestedRadarQueriesKeepDetailsWithoutAddingTheChildTwice)
{
    performance::GpuQueryCollector collector(backend, 1, 2);
    ncl_perf::PerfScopeToken_t render = collector.Begin(1, 1, performance::PerfStage::Screen);
    backend.time = 2000;
    ncl_perf::PerfScopeToken_t map = collector.Begin(1, 1, performance::PerfStage::RadarMapDraw);
    backend.time = 3000;
    ncl_perf::PerfScopeToken_t layer = collector.Begin(1, 1, performance::PerfStage::RadarLayer);
    backend.time = 5000;
    collector.End(layer);
    backend.time = 8000;
    collector.End(map);
    backend.time = 10000;
    collector.End(render);
    backend.ready.fill(true);

    std::array<performance::PerfGpuResult, 3> results;
    ASSERT_EQ(collector.Poll(results.data(), uint32_t(results.size())), 3u);
    EXPECT_EQ(results[0].duration_ns, 9000u);
    EXPECT_EQ(results[1].series, performance::PerfSeries::RadarGpu);
    EXPECT_EQ(results[1].duration_ns, 6000u);
    EXPECT_EQ(results[2].scope_id, performance::PerfStage::RadarLayer);
    EXPECT_EQ(results[2].duration_ns, 2000u);
    EXPECT_EQ(results[2].series, performance::PerfSeries::Count);
    collector.ReleaseGl();
}

TEST_F(GpuQueryCollectorTest, PanelGpuUsesAlreadyStartedOutputWithoutScreen)
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
    int window = 0;
    monitor.SetMainWindow(&window);
    monitor.SetGpuBackend(&backend);
    monitor.OnContextCreated();
    monitor.SetMode(1);
    performance::PerfOutputId_t output = monitor.BeginOutput(&window);
    ncl_perf::PerfScopeToken_t token = monitor.BeginGpu(performance::PerfStage::Monitor);
    monitor.EndGpu(token);
    backend.ready.fill(true);
    monitor.EndOutput(output);
    performance::PerfSnapshot snapshot;
    monitor.GetSnapshot(snapshot);
    EXPECT_EQ(snapshot.gpu_output_id, output);
    EXPECT_EQ(snapshot.latest.output_id, output);
    ASSERT_EQ(snapshot.gpu_count, 1u);
    EXPECT_EQ(snapshot.gpu_passes[0].scope_id, performance::PerfStage::Monitor);
    monitor.Shutdown();
}

TEST_F(GpuQueryCollectorTest, ShaderOverflowPreservesScreenRadarAndMonitor)
{
    performance::GpuQueryCollector collector(backend, 1, 2);
    ncl_perf::PerfScopeToken_t screen = collector.Begin(1, 1, performance::PerfStage::Screen);
    ASSERT_NE(screen, 0u);

    uint32_t accepted = 0;

    for (uint32_t index = 0; index < 142; ++index)
    {
        ncl_perf::PerfScopeToken_t shader = collector.Begin(1, 1, performance::PerfStage::StudioShader);
        accepted += shader != 0;
        backend.time += 10;
        collector.End(shader);
    }

    EXPECT_EQ(accepted, 110u);

    Submit(collector, performance::PerfStage::RadarBatch, 16);
    Submit(collector, performance::PerfStage::Monitor, 1);
    collector.End(screen);
    EXPECT_EQ(collector.dropped_scopes(), 32u);
    EXPECT_EQ(backend.allocated, 2048u);
    EXPECT_EQ(backend.writes.size(), 256u);

    backend.ready.fill(true);

    std::vector<performance::PerfGpuResult> results = Collect(collector);

    EXPECT_EQ(results.size(), 129u);
    EXPECT_EQ(Count(results, performance::PerfStage::Screen, ncl_perf::PerfSampleState::Measured, performance::PerfSeries::RenderGpu), 1u);
    EXPECT_EQ(
        Count(results, performance::PerfStage::RadarBatch, ncl_perf::PerfSampleState::Measured, performance::PerfSeries::RadarGpu), 16u
    );
    EXPECT_EQ(Count(results, performance::PerfStage::Monitor, ncl_perf::PerfSampleState::Measured, performance::PerfSeries::Count), 1u);
    EXPECT_EQ(
        Count(results, performance::PerfStage::StudioShader, ncl_perf::PerfSampleState::Incomplete, performance::PerfSeries::StudioGpu), 1u
    );

    collector.ReleaseGl();
}

TEST_F(GpuQueryCollectorTest, NestedDroppedDetailPreservesParentRollup)
{
    performance::GpuQueryCollector collector(backend, 1, 2);
    ncl_perf::PerfScopeToken_t screen = collector.Begin(1, 1, performance::PerfStage::Screen);
    Submit(collector, performance::PerfStage::StudioShader, 110);
    ncl_perf::PerfScopeToken_t radar = collector.Begin(1, 1, performance::PerfStage::RadarMapDraw);
    ASSERT_NE(radar, 0u);

    std::array<ncl_perf::PerfScopeToken_t, 15> children;

    for (ncl_perf::PerfScopeToken_t& child : children)
    {
        child = collector.Begin(1, 1, performance::PerfStage::RadarLayer);
        ASSERT_NE(child, 0u);
    }

    EXPECT_EQ(collector.Begin(1, 1, performance::PerfStage::RadarLayer), 0u);

    for (uint32_t index = uint32_t(children.size()); index > 0; --index)
    {
        collector.End(children[index - 1]);
    }

    backend.time += 40;
    collector.End(radar);
    Submit(collector, performance::PerfStage::Monitor, 1);
    collector.End(screen);
    backend.ready.fill(true);

    std::vector<performance::PerfGpuResult> results = Collect(collector);

    EXPECT_EQ(Count(results, performance::PerfStage::Screen, ncl_perf::PerfSampleState::Measured, performance::PerfSeries::RenderGpu), 1u);
    EXPECT_EQ(
        Count(results, performance::PerfStage::RadarMapDraw, ncl_perf::PerfSampleState::Measured, performance::PerfSeries::RadarGpu), 1u
    );
    EXPECT_EQ(Count(results, performance::PerfStage::RadarLayer, ncl_perf::PerfSampleState::Measured, performance::PerfSeries::Count), 15u);
    EXPECT_EQ(
        Count(results, performance::PerfStage::RadarLayer, ncl_perf::PerfSampleState::Incomplete, performance::PerfSeries::Count), 1u
    );

    collector.ReleaseGl();
}

TEST_F(GpuQueryCollectorTest, RadarOverflowPreservesLateMonitor)
{
    performance::GpuQueryCollector collector(backend, 1, 2);
    ncl_perf::PerfScopeToken_t screen = collector.Begin(1, 1, performance::PerfStage::Screen);
    Submit(collector, performance::PerfStage::StudioShader, 1);
    Submit(collector, performance::PerfStage::RadarBatch, 125);
    EXPECT_EQ(collector.Begin(1, 1, performance::PerfStage::RadarBatch), 0u);

    Submit(collector, performance::PerfStage::Monitor, 1);
    collector.End(screen);
    backend.ready.fill(true);

    std::vector<performance::PerfGpuResult> results = Collect(collector);

    EXPECT_EQ(Count(results, performance::PerfStage::Screen, ncl_perf::PerfSampleState::Measured, performance::PerfSeries::RenderGpu), 1u);
    EXPECT_EQ(Count(results, performance::PerfStage::Monitor, ncl_perf::PerfSampleState::Measured, performance::PerfSeries::Count), 1u);
    EXPECT_EQ(
        Count(results, performance::PerfStage::StudioShader, ncl_perf::PerfSampleState::Measured, performance::PerfSeries::StudioGpu), 1u
    );
    EXPECT_EQ(
        Count(results, performance::PerfStage::RadarBatch, ncl_perf::PerfSampleState::Incomplete, performance::PerfSeries::RadarGpu), 1u
    );

    collector.ReleaseGl();
}

TEST_F(GpuQueryCollectorTest, DroppedUnseenScopeIsReported)
{
    performance::GpuQueryCollector collector(backend, 3, 4);
    ncl_perf::PerfScopeToken_t screen = collector.Begin(1, 1, performance::PerfStage::Screen);
    Submit(collector, performance::PerfStage::RadarBatch, 126);
    uint32_t writes = uint32_t(backend.writes.size());
    EXPECT_EQ(collector.Begin(1, 1, performance::PerfStage::RadarLayer), 0u);
    EXPECT_EQ(backend.writes.size(), writes);

    collector.End(screen);
    backend.ready.fill(true);

    std::vector<performance::PerfGpuResult> results = Collect(collector);

    ASSERT_EQ(results.size(), 128u);

    const performance::PerfGpuResult& marker = results.back();
    EXPECT_EQ(marker.scope_id, performance::PerfStage::RadarLayer);
    EXPECT_EQ(marker.state, ncl_perf::PerfSampleState::Incomplete);
    EXPECT_EQ(marker.series, performance::PerfSeries::RadarGpu);
    EXPECT_EQ(marker.duration_ns, 0u);
    EXPECT_EQ(marker.frame_id, 1u);
    EXPECT_EQ(marker.output_id, 1u);
    EXPECT_EQ(marker.collector_generation, 3u);
    EXPECT_EQ(marker.context_generation, 4u);

    collector.ReleaseGl();
}

TEST_F(GpuQueryCollectorTest, ReservedScreenAndMonitorSurviveEarlierDetails)
{
    performance::GpuQueryCollector collector(backend, 1, 2);
    ncl_perf::PerfScopeToken_t outer = collector.Begin(1, 1, performance::PerfStage::StudioShader);
    ASSERT_NE(outer, 0u);

    Submit(collector, performance::PerfStage::StudioShader, 109);
    EXPECT_EQ(collector.Begin(1, 1, performance::PerfStage::StudioShader), 0u);

    ncl_perf::PerfScopeToken_t screen = collector.Begin(1, 1, performance::PerfStage::Screen);
    ASSERT_NE(screen, 0u);

    Submit(collector, performance::PerfStage::Monitor, 1);
    collector.End(screen);
    collector.End(outer);
    backend.ready.fill(true);

    std::vector<performance::PerfGpuResult> results = Collect(collector);

    EXPECT_EQ(Count(results, performance::PerfStage::Screen, ncl_perf::PerfSampleState::Measured, performance::PerfSeries::RenderGpu), 1u);
    EXPECT_EQ(Count(results, performance::PerfStage::Monitor, ncl_perf::PerfSampleState::Measured, performance::PerfSeries::Count), 1u);
    EXPECT_EQ(
        Count(results, performance::PerfStage::StudioShader, ncl_perf::PerfSampleState::Incomplete, performance::PerfSeries::Count), 1u
    );

    collector.ReleaseGl();
}

TEST_F(GpuQueryCollectorTest, PartialPollRetainsIncompleteMarkers)
{
    performance::GpuQueryCollector collector(backend, 1, 2);
    ncl_perf::PerfScopeToken_t screen = collector.Begin(1, 1, performance::PerfStage::Screen);
    Submit(collector, performance::PerfStage::RadarBatch, 126);
    for (uint32_t index = 0; index < 3; ++index)
    {
        EXPECT_EQ(collector.Begin(1, 1, performance::PerfStage::RadarLayer), 0u);
        EXPECT_EQ(collector.Begin(1, 1, performance::PerfStage::RadarMapDraw), 0u);
    }

    collector.End(screen);
    for (uint64_t id = 2; id <= 8; ++id)
    {
        collector.End(collector.Begin(id, id, performance::PerfStage::Screen));
    }

    backend.ready.fill(true);

    performance::PerfGpuResult result;

    for (uint32_t index = 0; index < 127; ++index)
    {
        ASSERT_EQ(collector.Poll(&result, 1), 1u);
        EXPECT_EQ(result.output_id, 1u);
        EXPECT_EQ(result.state, ncl_perf::PerfSampleState::Measured);
    }

    EXPECT_EQ(collector.Begin(9, 9, performance::PerfStage::Screen), 0u);
    ASSERT_EQ(collector.Poll(&result, 1), 1u);
    EXPECT_EQ(result.scope_id, performance::PerfStage::RadarLayer);
    EXPECT_EQ(result.state, ncl_perf::PerfSampleState::Incomplete);
    EXPECT_EQ(collector.Begin(9, 9, performance::PerfStage::Screen), 0u);
    ASSERT_EQ(collector.Poll(&result, 1), 1u);
    EXPECT_EQ(result.scope_id, performance::PerfStage::RadarMapDraw);
    EXPECT_EQ(result.state, ncl_perf::PerfSampleState::Incomplete);

    ncl_perf::PerfScopeToken_t fresh = collector.Begin(9, 9, performance::PerfStage::Screen);
    ASSERT_NE(fresh, 0u);

    for (uint64_t id = 2; id <= 8; ++id)
    {
        ASSERT_EQ(collector.Poll(&result, 1), 1u);
        EXPECT_EQ(result.output_id, id);
    }

    EXPECT_EQ(collector.Poll(&result, 1), 0u);

    collector.End(fresh);
    backend.ready.fill(true);
    EXPECT_EQ(collector.Poll(&result, 1), 1u);
    EXPECT_EQ(collector.Poll(&result, 1), 0u);
    EXPECT_EQ(backend.reads, 270u);

    collector.ReleaseGl();
}

TEST_F(GpuQueryCollectorTest, PendingQueriesNeverReadEvenWithDrops)
{
    performance::GpuQueryCollector collector(backend, 1, 2);
    ncl_perf::PerfScopeToken_t screen = collector.Begin(1, 1, performance::PerfStage::Screen);
    Submit(collector, performance::PerfStage::StudioShader, 110);
    EXPECT_EQ(collector.Begin(1, 1, performance::PerfStage::StudioShader), 0u);

    collector.End(screen);
    backend.ready.fill(true);
    backend.ready[backend.writes.back()] = false;
    EXPECT_TRUE(Collect(collector).empty());
    EXPECT_EQ(backend.reads, 0u);

    backend.ready.fill(true);
    EXPECT_EQ(Collect(collector).size(), 112u);

    collector.ReleaseGl();
}

TEST_F(GpuQueryCollectorTest, ResetAndCancelDiscardDropMetadata)
{
    for (bool reset : {false, true})
    {
        performance::GpuQueryCollector collector(backend, 1, 2);
        ncl_perf::PerfScopeToken_t screen = collector.Begin(1, 1, performance::PerfStage::Screen);
        Submit(collector, performance::PerfStage::StudioShader, 110);
        EXPECT_EQ(collector.Begin(1, 1, performance::PerfStage::StudioShader), 0u);

        if (reset)
        {
            collector.Reset(2);
        }
        else
        {
            collector.CancelOutput(1);
        }

        collector.End(screen);
        ncl_perf::PerfScopeToken_t fresh = collector.Begin(2, 2, performance::PerfStage::Screen);
        collector.End(fresh);
        backend.ready.fill(true);
        std::vector<performance::PerfGpuResult> results = Collect(collector);

        ASSERT_EQ(results.size(), 1u);
        EXPECT_EQ(results[0].output_id, 2u);
        EXPECT_EQ(results[0].collector_generation, reset ? 2u : 1u);
        EXPECT_EQ(results[0].state, ncl_perf::PerfSampleState::Measured);

        collector.ReleaseGl();
    }
}

TEST_F(GpuQueryCollectorTest, ClosedOutputDoesNotAcceptLateBegins)
{
    performance::GpuQueryCollector collector(backend, 1, 2);
    collector.End(collector.Begin(1, 1, performance::PerfStage::Screen));
    EXPECT_EQ(collector.Begin(1, 1, performance::PerfStage::Monitor), 0u);
    EXPECT_EQ(backend.writes.size(), 2u);

    backend.ready.fill(true);

    std::vector<performance::PerfGpuResult> results = Collect(collector);

    EXPECT_EQ(Count(results, performance::PerfStage::Screen, ncl_perf::PerfSampleState::Measured, performance::PerfSeries::RenderGpu), 1u);
    EXPECT_EQ(Count(results, performance::PerfStage::Monitor, ncl_perf::PerfSampleState::Incomplete, performance::PerfSeries::Count), 1u);

    collector.ReleaseGl();
}

TEST_F(GpuQueryCollectorTest, EightPendingSetsAreNeverReused)
{
    performance::GpuQueryCollector collector(backend, 1, 2);

    for (uint64_t id = 1; id <= 8; ++id)
    {
        ncl_perf::PerfScopeToken_t token = collector.Begin(id, id, performance::PerfStage::Screen);
        ASSERT_NE(token, 0u);

        collector.End(token);
    }

    EXPECT_EQ(collector.Begin(9, 9, performance::PerfStage::Screen), 0u);
    EXPECT_EQ(backend.writes.size(), 16u);
    EXPECT_EQ(backend.reads, 0u);

    backend.ready.fill(true);

    std::vector<performance::PerfGpuResult> results = Collect(collector);

    ASSERT_EQ(results.size(), 8u);

    for (uint32_t index = 0; index < results.size(); ++index)
    {
        EXPECT_EQ(results[index].output_id, index + 1);
        EXPECT_EQ(results[index].state, ncl_perf::PerfSampleState::Measured);
    }

    collector.ReleaseGl();
}

TEST_F(GpuQueryCollectorTest, ResetRetainsPendingNamesButRejectsOldResults)
{
    performance::GpuQueryCollector collector(backend, 1, 2);
    ncl_perf::PerfScopeToken_t old = collector.Begin(1, 1, performance::PerfStage::Screen);
    collector.Reset(2);
    uint32_t writes = uint32_t(backend.writes.size());
    collector.End(old);
    EXPECT_EQ(backend.writes.size(), writes);
    ncl_perf::PerfScopeToken_t fresh = collector.Begin(2, 2, performance::PerfStage::Screen);
    collector.End(fresh);
    EXPECT_NE(backend.writes[0], backend.writes[writes]);
    backend.ready.fill(true);
    performance::PerfGpuResult result;
    ASSERT_EQ(collector.Poll(&result, 1), 1u);
    EXPECT_EQ(result.collector_generation, 2u);
    EXPECT_EQ(result.output_id, 2u);
    collector.ReleaseGl();
}

TEST_F(GpuQueryCollectorTest, DisableDeletesWithoutWaiting)
{
    performance::GpuQueryCollector collector(backend, 1, 2);
    ncl_perf::PerfScopeToken_t token = collector.Begin(1, 1, performance::PerfStage::Screen);
    collector.ReleaseGl();
    collector.End(token);
    EXPECT_EQ(backend.deleted, 2048u);
    EXPECT_EQ(backend.reads, 0u);
    EXPECT_EQ(backend.writes.size(), 1u);
}

TEST_F(GpuQueryCollectorTest, LostContextAbandonsWithoutDelete)
{
    performance::GpuQueryCollector collector(backend, 1, 2);
    ncl_perf::PerfScopeToken_t token = collector.Begin(1, 1, performance::PerfStage::Screen);
    collector.AbandonGl();
    collector.End(token);
    EXPECT_EQ(backend.deleted, 0u);
    EXPECT_EQ(backend.writes.size(), 1u);
}

TEST_F(GpuQueryCollectorTest, UnsupportedOrZeroBitContextReportsUnavailable)
{
    backend.supported = false;
    performance::GpuQueryCollector unsupported(backend, 1, 2);
    EXPECT_EQ(unsupported.Begin(1, 1, performance::PerfStage::Screen), 0u);
    EXPECT_EQ(backend.allocated, 0u);
    backend.supported = true;
    backend.bits = 0;
    performance::GpuQueryCollector zero_bits(backend, 1, 2);
    EXPECT_EQ(zero_bits.Begin(1, 1, performance::PerfStage::Screen), 0u);
    EXPECT_EQ(backend.allocated, 0u);
}

TEST_F(GpuQueryCollectorTest, CancelUnswappedOutputRetiresWithoutPublishing)
{
    performance::GpuQueryCollector collector(backend, 1, 2);
    ncl_perf::PerfScopeToken_t token = collector.Begin(1, 1, performance::PerfStage::Screen);
    collector.End(token);
    collector.CancelOutput(1);
    backend.ready.fill(true);
    performance::PerfGpuResult result;
    EXPECT_EQ(collector.Poll(&result, 1), 0u);
    EXPECT_NE(collector.Begin(2, 2, performance::PerfStage::Screen), 0u);
    collector.ReleaseGl();
}

TEST_F(GpuQueryCollectorTest, ReadyBeforePublicationIsStaged)
{
    class Clock : public ncl_perf::PerfClockInterface
    {
    public:
        ncl_perf::PerfTimestamp_t time{};
        ncl_perf::PerfTimestamp_t Now() override
        {
            return time;
        }
    } clock;
    performance::PerformanceMonitor monitor(clock);
    int window{};
    monitor.SetGpuBackend(&backend);
    monitor.OnContextCreated();
    monitor.SetMainWindow(&window);
    monitor.SetMode(1);
    ncl_perf::PerfScopeToken_t render = monitor.BeginGpu(performance::PerfStage::Screen);
    performance::PerfOutputId_t id = monitor.BeginOutput(&window);
    backend.time = 7000;
    monitor.CloseRenderGpu();
    backend.ready.fill(true);
    monitor.PollGpu();
    clock.time = 10000;
    monitor.EndOutput(id);
    monitor.EndGpu(render);
    performance::PerfSnapshot snapshot;
    monitor.GetSnapshot(snapshot);
    EXPECT_EQ(snapshot.latest.values[uint32_t(performance::PerfSeries::RenderGpu)].duration_ns, 6000u);
    EXPECT_EQ(snapshot.latest.values[uint32_t(performance::PerfSeries::RenderGpu)].state, ncl_perf::PerfSampleState::Measured);
    EXPECT_EQ(snapshot.counters[uint32_t(performance::PerfCounterId::GpuExpiredResults)], 0u);
    monitor.Shutdown();
}


namespace
{
    class AgeClock : public ncl_perf::PerfClockInterface
    {
    public:
        ncl_perf::PerfTimestamp_t time{};

        ncl_perf::PerfTimestamp_t Now() override
        {
            return time;
        }
    };

    class PerformanceGpuAgeTest : public GpuQueryCollectorTest
    {
    protected:
        AgeClock clock;
        performance::PerformanceMonitor monitor{clock};
        int window{};

        void SetUp() override
        {
            monitor.SetGpuBackend(&backend);
            monitor.SetMainWindow(&window);
            monitor.OnContextCreated();
            monitor.SetMode(1);
        }

        void TearDown() override
        {
            monitor.Shutdown();
        }

        performance::PerfOutputId_t Present()
        {
            performance::PerfOutputId_t output = monitor.BeginOutput(&window);
            clock.time += 10000000;
            monitor.EndOutput(output);
            return output;
        }

        void CancelScreens()
        {
            for (uint32_t index = 0; index < 3; ++index)
            {
                ncl_perf::PerfScopeToken_t screen = monitor.BeginGpu(performance::PerfStage::Screen);
                monitor.EndGpu(screen);
            }
        }
    };
} // namespace

TEST_F(PerformanceGpuAgeTest, CanceledScreensDoNotAgeCompletedGpuOutput)
{
    ncl_perf::PerfScopeToken_t studio = monitor.BeginGpu(performance::PerfStage::StudioShader);
    monitor.EndGpu(studio);
    backend.ready.fill(true);
    performance::PerfOutputId_t first = Present();
    performance::PerfSnapshot snapshot;
    monitor.GetSnapshot(snapshot);
    ASSERT_EQ(snapshot.gpu_output_id, first);
    EXPECT_EQ(snapshot.gpu_age, 0u);

    CancelScreens();
    monitor.GetSnapshot(snapshot);
    EXPECT_EQ(snapshot.gpu_age, 0u);
    performance::PerfOutputId_t later = monitor.BeginOutput(&window);
    monitor.GetSnapshot(snapshot);
    EXPECT_EQ(snapshot.gpu_age, 0u);
    monitor.EndOutput(later);
    monitor.GetSnapshot(snapshot);
    EXPECT_EQ(snapshot.gpu_age, 1u);
    EXPECT_GT(later, first + 1);
    EXPECT_EQ(snapshot.gpu_output_id, first);
}

TEST_F(PerformanceGpuAgeTest, LateResultCountsCompletedPresentationsAcrossCanceledIdentifiers)
{
    ncl_perf::PerfScopeToken_t studio = monitor.BeginGpu(performance::PerfStage::StudioShader);
    monitor.EndGpu(studio);
    performance::PerfOutputId_t first = Present();
    CancelScreens();
    Present();
    performance::PerfOutputId_t draft = monitor.BeginOutput(&window);
    backend.ready.fill(true);
    monitor.PollGpu();
    performance::PerfSnapshot snapshot;
    monitor.GetSnapshot(snapshot);
    EXPECT_EQ(snapshot.gpu_output_id, first);
    EXPECT_EQ(snapshot.gpu_age, 1u);
    monitor.EndOutput(draft);
    monitor.GetSnapshot(snapshot);
    EXPECT_EQ(snapshot.gpu_age, 2u);
    monitor.EndOutput(draft);
    monitor.GetSnapshot(snapshot);
    EXPECT_EQ(snapshot.gpu_age, 2u);
}

TEST_F(PerformanceGpuAgeTest, ResultPolledAtSwapCountsTheCurrentPresentationOnce)
{
    ncl_perf::PerfScopeToken_t studio = monitor.BeginGpu(performance::PerfStage::StudioShader);
    monitor.EndGpu(studio);
    performance::PerfOutputId_t first = Present();
    CancelScreens();
    performance::PerfOutputId_t draft = monitor.BeginOutput(&window);
    backend.ready.fill(true);
    monitor.EndOutput(draft);
    performance::PerfSnapshot snapshot;
    monitor.GetSnapshot(snapshot);
    EXPECT_EQ(snapshot.gpu_output_id, first);
    EXPECT_EQ(snapshot.gpu_age, 1u);
}

TEST_F(PerformanceGpuAgeTest, CurrentDraftResultHasZeroAgeOnBothSidesOfSwap)
{
    CancelScreens();
    ncl_perf::PerfScopeToken_t studio = monitor.BeginGpu(performance::PerfStage::StudioShader);
    monitor.EndGpu(studio);
    performance::PerfOutputId_t output = monitor.BeginOutput(&window);
    backend.ready.fill(true);
    monitor.PollGpu();
    performance::PerfSnapshot snapshot;
    monitor.GetSnapshot(snapshot);
    EXPECT_EQ(snapshot.gpu_output_id, output);
    EXPECT_EQ(snapshot.gpu_age, 0u);
    monitor.EndOutput(output);
    monitor.GetSnapshot(snapshot);
    EXPECT_EQ(snapshot.gpu_age, 0u);
    Present();
    monitor.GetSnapshot(snapshot);
    EXPECT_EQ(snapshot.gpu_age, 1u);
}

TEST_F(PerformanceGpuAgeTest, AgeContinuesAcrossLongGapsWithRetainedFrames)
{
    ncl_perf::PerfScopeToken_t studio = monitor.BeginGpu(performance::PerfStage::StudioShader);
    monitor.EndGpu(studio);
    backend.ready.fill(true);
    performance::PerfOutputId_t first = Present();
    CancelScreens();
    clock.time += 11000000000;
    Present();
    Present();
    performance::PerfSnapshot snapshot;
    monitor.GetSnapshot(snapshot);
    EXPECT_EQ(snapshot.gpu_output_id, first);
    EXPECT_EQ(snapshot.gpu_age, 2u);
    EXPECT_EQ(snapshot.gpu_count, 1u);
}

TEST_F(GpuQueryCollectorTest, ReleasedCollectorTokenCannotCloseAReplacementSet)
{
    performance::GpuQueryCollector retired(backend, 1, 2);
    ncl_perf::PerfScopeToken_t old = retired.Begin(1, 1, performance::PerfStage::Screen);
    retired.ReleaseGl();
    performance::GpuQueryCollector collector(backend, 1, 2);
    ncl_perf::PerfScopeToken_t fresh = collector.Begin(1, 5, performance::PerfStage::Screen);
    uint32_t writes = uint32_t(backend.writes.size());
    collector.End(old);
    EXPECT_EQ(backend.writes.size(), writes);
    collector.End(fresh);
    backend.ready.fill(true);
    performance::PerfGpuResult result;
    ASSERT_EQ(collector.Poll(&result, 1), 1u);
    EXPECT_EQ(result.output_id, 5u);
    EXPECT_EQ(result.collector_generation, 1u);
    EXPECT_EQ(result.context_generation, 2u);
    EXPECT_EQ(collector.Poll(&result, 1), 0u);
    collector.ReleaseGl();
}

TEST_F(GpuQueryCollectorTest, CpuOnlyRadarStageDoesNotEnrollInGpuRollup)
{
    performance::GpuQueryCollector collector(backend, 1, 2);
    ncl_perf::PerfScopeToken_t token = collector.Begin(1, 1, performance::PerfStage::RadarPrepare);
    backend.time += 100;
    collector.End(token);
    backend.ready.fill(true);
    performance::PerfGpuResult result;
    ASSERT_EQ(collector.Poll(&result, 1), 1u);
    EXPECT_EQ(result.scope_id, performance::PerfStage::RadarPrepare);
    EXPECT_EQ(result.duration_ns, 100u);
    EXPECT_EQ(result.series, performance::PerfSeries::Count);
    collector.ReleaseGl();
}

TEST_F(GpuQueryCollectorTest, DroppedScopeKeepsDistinctEffectiveSeriesKeys)
{
    performance::GpuQueryCollector collector(backend, 1, 2);
    ncl_perf::PerfScopeToken_t screen = collector.Begin(1, 1, performance::PerfStage::Screen);
    ncl_perf::PerfScopeToken_t shader = collector.Begin(1, 1, performance::PerfStage::StudioShader);
    Submit(collector, performance::PerfStage::StudioShader, 109);
    EXPECT_EQ(collector.Begin(1, 1, performance::PerfStage::StudioShader), 0u);

    collector.End(shader);
    EXPECT_EQ(collector.Begin(1, 1, performance::PerfStage::StudioShader), 0u);

    collector.End(screen);
    backend.ready.fill(true);

    std::vector<performance::PerfGpuResult> results = Collect(collector);

    EXPECT_EQ(results.size(), 113u);
    EXPECT_EQ(
        Count(results, performance::PerfStage::StudioShader, ncl_perf::PerfSampleState::Incomplete, performance::PerfSeries::Count), 1u
    );
    EXPECT_EQ(
        Count(results, performance::PerfStage::StudioShader, ncl_perf::PerfSampleState::Incomplete, performance::PerfSeries::StudioGpu), 1u
    );
    EXPECT_EQ(collector.dropped_scopes(), 2u);

    collector.ReleaseGl();
}

TEST_F(GpuQueryCollectorTest, TimestampWrapUsesTheAdvertisedCounterWidth)
{
    backend.bits = 8;
    backend.time = 250;
    performance::GpuQueryCollector collector(backend, 1, 2);
    ncl_perf::PerfScopeToken_t token = collector.Begin(1, 1, performance::PerfStage::Screen);
    backend.time = 4;
    collector.End(token);
    backend.ready.fill(true);

    performance::PerfGpuResult result;

    ASSERT_EQ(collector.Poll(&result, 1), 1u);
    EXPECT_EQ(result.duration_ns, 10u);
    EXPECT_EQ(result.state, ncl_perf::PerfSampleState::Measured);

    collector.ReleaseGl();
}
