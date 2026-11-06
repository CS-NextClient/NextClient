#include <gtest/gtest.h>

#include <array>
#include <vector>

#include <glad/glad.h>
#include <ncl_perf/PerfCpuAccounting.h>

#include "hud/radar/integration/Performance.h"

namespace
{
    class RadarMonitor : public performance::PerformanceMonitorInterface, public ncl_perf::PerfClockInterface
    {
    private:
        ncl_perf::PerfCpuAccounting accounting_{*this};

    public:
        ncl_perf::PerfTimestamp_t time{};
        uint32_t reads{};
        uint32_t map = 1;
        std::vector<std::source_location> cpu_locations;
        std::vector<ncl_perf::PerfScopeId_t> gpu_begins;
        uint32_t gpu_ends{};
        std::array<uint64_t, uint32_t(performance::PerfCounterId::Count)> counters{};
        std::vector<performance::PerfWorkerReport> workers;

        RadarMonitor()
        {
            accounting_.RegisterScope({performance::PerfStage::Radar, "Radar", 0, ncl_perf::PerfScopeCapability::Cpu});
            accounting_.RegisterScope(
                {performance::PerfStage::RadarPrepare, "Prepare", performance::PerfStage::Radar, ncl_perf::PerfScopeCapability::Cpu}
            );
            accounting_.RegisterScope(
                {performance::PerfStage::RadarDraw, "Draw", performance::PerfStage::Radar, ncl_perf::PerfScopeCapability::Cpu}
            );
            accounting_.RegisterScope(
                {performance::PerfStage::RadarBatch, "Batch", performance::PerfStage::Radar, ncl_perf::PerfScopeCapability::Cpu}
            );
            accounting_.SetEnabled(true);
        }
        ncl_perf::PerfTimestamp_t Now() override
        {
            ++reads;
            return time;
        }
        ncl_perf::PerfScopeToken_t BeginCpu(ncl_perf::PerfScopeId_t id, std::source_location location) override
        {
            cpu_locations.push_back(location);
            return accounting_.Begin(id);
        }
        void EndCpu(ncl_perf::PerfScopeToken_t token) override
        {
            accounting_.End(token);
        }
        ncl_perf::PerfScopeToken_t BeginGpu(ncl_perf::PerfScopeId_t id, std::source_location) override
        {
            gpu_begins.push_back(id);
            return 1;
        }
        void EndGpu(ncl_perf::PerfScopeToken_t) override
        {
            ++gpu_ends;
        }
        void AddCounter(performance::PerfCounterId counter, uint64_t value) override
        {
            counters[uint32_t(counter)] += value;
        }
        bool SetGauge(performance::PerfGaugeId, double) override
        {
            return false;
        }
        bool GetSnapshot(performance::PerfSnapshot&) const override
        {
            return false;
        }
        uint32_t map_generation() const override
        {
            return map;
        }
        void ReportWorker(const performance::PerfWorkerReport& report) override
        {
            workers.push_back(report);
        }
        void Copy(ncl_perf::PerfCpuSample& total)
        {
            accounting_.CopySamples(&total, 1);
        }
        void ClearFrame()
        {
            accounting_.ClearFrame();
        }
    };

    class RadarPerformanceTest : public testing::Test
    {
    protected:
        RadarMonitor monitor;
        radar::integration::Performance performance{&monitor};
    };

    uint32_t g_DrawCalls{};
    void APIENTRY DrawBatch(GLenum mode, GLint first, GLsizei count)
    {
        EXPECT_EQ(mode, GL_QUADS);
        EXPECT_EQ(first, 0);
        EXPECT_EQ(count, 12);
        ++g_DrawCalls;
    }
} // namespace

TEST_F(RadarPerformanceTest, PreparationAndEarlyReturnAreIncluded)
{
    {
        radar::integration::CpuScope preparation(performance.get_monitor(), performance::PerfStage::RadarPrepare);
        monitor.time = 2000000;
    }
    [&] {
        radar::integration::CpuScope draw(performance.get_monitor(), performance::PerfStage::RadarDraw);
        monitor.time = 2500000;
        return;
    }();
    ncl_perf::PerfCpuSample total;
    monitor.Copy(total);
    EXPECT_EQ(total.inclusive_ns, 2500000u);
    EXPECT_EQ(total.calls, 2u);
    EXPECT_TRUE(monitor.gpu_begins.empty());
}

TEST_F(RadarPerformanceTest, CounterUpdatesDoNotIssueDrawsOrReadClock)
{
    g_DrawCalls = 0;
    PFNGLDRAWARRAYSPROC original = glad_glDrawArrays;
    glad_glDrawArrays = DrawBatch;
    performance.AddCounter(performance::PerfCounterId::RadarBatches, 1);
    performance.AddCounter(performance::PerfCounterId::RadarQuads, 3);
    glad_glDrawArrays = original;
    EXPECT_EQ(g_DrawCalls, 0u);
    EXPECT_TRUE(monitor.gpu_begins.empty());
    EXPECT_EQ(monitor.gpu_ends, 0u);
    EXPECT_EQ(monitor.reads, 0u);
    EXPECT_EQ(monitor.counters[uint32_t(performance::PerfCounterId::RadarBatches)], 1u);
    EXPECT_EQ(monitor.counters[uint32_t(performance::PerfCounterId::RadarQuads)], 3u);
}

TEST_F(RadarPerformanceTest, HiddenRadarReportsInactiveWithoutStaleDuration)
{
    {
        radar::integration::CpuScope draw(performance.get_monitor(), performance::PerfStage::RadarDraw);
        monitor.time = 1000;
    }
    monitor.ClearFrame();
    uint32_t previous_reads = monitor.reads;
    {
        radar::integration::CpuScope hidden(performance.get_monitor(), performance::PerfStage::RadarDraw, false);
    }
    ncl_perf::PerfCpuSample total;
    monitor.Copy(total);
    EXPECT_EQ(total.inclusive_ns, 0u);
    EXPECT_EQ(total.state, ncl_perf::PerfSampleState::Inactive);
    EXPECT_EQ(monitor.reads, previous_reads);
}

TEST_F(RadarPerformanceTest, MissingProviderAllowsNullMonitorCalls)
{
    radar::integration::Performance performance{nullptr};
    {
        radar::integration::CpuScope retained(performance.get_monitor(), performance::PerfStage::RadarPrepare);
    }
    performance.ReportWorker({1, 100, performance::PerfWorkerStatus::Completed});
    EXPECT_EQ(monitor.reads, 0u);
    EXPECT_TRUE(monitor.workers.empty());
    EXPECT_EQ(performance.get_map_generation(), 0u);
}

TEST_F(RadarPerformanceTest, WorkerReportPreservesDispatchGeneration)
{
    uint32_t dispatched_generation = performance.get_map_generation();
    monitor.map = 2;
    performance.ReportWorker({dispatched_generation, 300, performance::PerfWorkerStatus::Completed});
    ASSERT_EQ(monitor.workers.size(), 1u);
    EXPECT_EQ(monitor.workers[0].map_generation, 1u);
    EXPECT_EQ(monitor.workers[0].duration_ns, 300u);
}

TEST_F(RadarPerformanceTest, TotalAndStageScopesPreserveRadarCallerLocation)
{
    const std::source_location caller = std::source_location::current();
    const uint32_t line = __LINE__ + 2;
    {
        radar::integration::CpuScope preparation(performance.get_monitor(), performance::PerfStage::RadarPrepare);
    }
    ASSERT_EQ(monitor.cpu_locations.size(), 2u);
    for (std::source_location location : monitor.cpu_locations)
    {
        EXPECT_STREQ(location.file_name(), __FILE__);
        EXPECT_STREQ(location.function_name(), caller.function_name());
        EXPECT_EQ(location.line(), line);
    }
}
