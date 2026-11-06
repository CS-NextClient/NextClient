#include <algorithm>
#include <iterator>
#include <memory>

#include <gtest/gtest.h>

#include "performance/PerformanceFrameHistory.h"
#include "performance/PerformanceMetrics.h"

namespace
{
    performance::PerfFrameRecord Output(uint64_t id, uint64_t end, uint64_t interval)
    {
        performance::PerfFrameRecord record;
        record.kind = performance::PerfRecordKind::Output;
        record.output_id = id;
        record.completed_at = end;
        record.collector_generation = 1;
        record.context_generation = 2;
        record.values[uint32_t(performance::PerfSeries::FrameInterval)] = {interval, ncl_perf::PerfSampleState::Measured};
        return record;
    }
} // namespace

TEST(FrameHistoryTest, FpsUsesTotalIntervalTime)
{
    performance::PerfFrameHistory history;
    history.Append(Output(1, 10000000, 10000000));
    history.Append(Output(2, 30000000, 20000000));
    performance::PerfFrameRecord host;
    host.kind = performance::PerfRecordKind::Host;
    host.completed_at = 30000000;
    host.collector_generation = 1;
    host.values[uint32_t(performance::PerfSeries::FrameInterval)] = {5000000, ncl_perf::PerfSampleState::Measured};
    history.Append(host);
    performance::PerfSnapshot snapshot;
    history.BuildSnapshot(30000000, snapshot);
    const performance::PerfSummary& summary = snapshot.summaries[uint32_t(performance::PerfSeries::FrameInterval)];
    EXPECT_EQ(summary.count, 2u);
    EXPECT_NEAR(summary.fps, 66.6666667, 0.00001);
    EXPECT_EQ(summary.mean_ns, 15000000u);
}

TEST(FrameHistoryTest, WindowFpsUsesOnlyMeasuredOutputsCompletedInsideTheWindow)
{
    performance::PerfFrameHistory history;
    history.Append(Output(1, 1490000000, 100000000));
    history.Append(Output(2, 1500000000, 10000000));
    performance::PerfFrameRecord host = Output(3, 1700000000, 1000000);
    host.kind = performance::PerfRecordKind::Host;
    history.Append(host);
    history.Append(Output(4, 1800000000, 20000000));
    history.Append(Output(5, 1850000000, 0));
    performance::PerfFrameRecord pending = Output(6, 1900000000, 100000000);
    pending.values[uint32_t(performance::PerfSeries::FrameInterval)].state = ncl_perf::PerfSampleState::Pending;
    history.Append(pending);
    history.Append(Output(7, 2000000000, 30000000));
    history.Append(Output(8, 2001000000, 100000000));

    EXPECT_DOUBLE_EQ(history.CalculateFps(2000000000, 500000000), 40.0);
    performance::PerfSnapshot snapshot;
    history.BuildSnapshot(2001000000, snapshot);
    EXPECT_EQ(snapshot.summaries[0].count, 5u);
    EXPECT_NEAR(snapshot.summaries[0].fps, 5000000000.0 / 260000000.0, 0.00001);
}

TEST(FrameHistoryTest, WindowFpsSupportsWarmupExpirationAndReset)
{
    performance::PerfFrameHistory history;
    EXPECT_DOUBLE_EQ(history.CalculateFps(0, 500000000), 0.0);
    history.Append(Output(1, 10000000, 10000000));
    history.Append(Output(2, 30000000, 20000000));
    EXPECT_NEAR(history.CalculateFps(30000000, 500000000), 66.6666667, 0.00001);
    EXPECT_DOUBLE_EQ(history.CalculateFps(30000000, 0), 0.0);
    EXPECT_DOUBLE_EQ(history.CalculateFps(530000000, 500000000), 0.0);
    history.Reset(2);
    EXPECT_DOUBLE_EQ(history.CalculateFps(30000000, 500000000), 0.0);
}

TEST(FrameHistoryTest, NearestRankPercentilesNeedEnoughSamples)
{
    performance::PerfFrameHistory history;
    for (uint64_t id = 1; id <= 99; ++id)
    {
        history.Append(Output(id, id * 1000000, id * 1000000));
    }
    performance::PerfSnapshot snapshot;
    history.BuildSnapshot(99000000, snapshot);
    EXPECT_FALSE(snapshot.summaries[0].percentiles_available);
    EXPECT_EQ(snapshot.summaries[0].median_ns, 50000000u);
    history.Append(Output(100, 100000000, 100000000));
    history.BuildSnapshot(100000000, snapshot);
    EXPECT_TRUE(snapshot.summaries[0].percentiles_available);
    EXPECT_EQ(snapshot.summaries[0].median_ns, 50500000u);
    EXPECT_EQ(snapshot.summaries[0].p95_ns, 95000000u);
    EXPECT_EQ(snapshot.summaries[0].p99_ns, 99000000u);
}

TEST(FrameHistoryTest, CpuAndGpuPercentilesExcludeMissingSamples)
{
    performance::PerfFrameHistory history;
    for (uint64_t id = 1; id <= 101; ++id)
    {
        performance::PerfFrameRecord output = Output(id, id * 1000000, 1000000);
        output.values[uint32_t(performance::PerfSeries::RenderGpu)] = {id * 1000, ncl_perf::PerfSampleState::Measured};
        if (id == 101)
        {
            output.values[uint32_t(performance::PerfSeries::RenderGpu)].state = ncl_perf::PerfSampleState::Pending;
        }
        history.Append(output);

        performance::PerfFrameRecord host = output;
        host.kind = performance::PerfRecordKind::Host;
        host.values[uint32_t(performance::PerfSeries::HostCpu)] = {id * 2000, ncl_perf::PerfSampleState::Measured};
        history.Append(host);
    }
    performance::PerfSnapshot snapshot;
    history.BuildSnapshot(101000000, snapshot);
    const performance::PerfSummary& cpu = snapshot.summaries[uint32_t(performance::PerfSeries::HostCpu)];
    const performance::PerfSummary& gpu = snapshot.summaries[uint32_t(performance::PerfSeries::RenderGpu)];
    EXPECT_TRUE(cpu.percentiles_available);
    EXPECT_EQ(cpu.p99_ns, 200000u);
    EXPECT_TRUE(gpu.percentiles_available);
    EXPECT_EQ(gpu.count, 100u);
    EXPECT_EQ(gpu.p99_ns, 99000u);
}

TEST(FrameHistoryTest, GroupedPeaksUsePartsFromTheSameHostAndOutput)
{
    performance::PerfFrameHistory history;
    performance::PerfFrameRecord output = Output(1, 10000000, 10000000);
    output.values[uint32_t(performance::PerfSeries::RenderGpu)] = {6000000, ncl_perf::PerfSampleState::Measured};
    output.values[uint32_t(performance::PerfSeries::StudioGpu)] = {1000000, ncl_perf::PerfSampleState::Measured};
    output.values[uint32_t(performance::PerfSeries::RadarGpu)] = {2000000, ncl_perf::PerfSampleState::Measured};
    history.Append(output);
    performance::PerfFrameRecord host = output;
    host.kind = performance::PerfRecordKind::Host;
    host.values[uint32_t(performance::PerfSeries::HostCpu)] = {9000000, ncl_perf::PerfSampleState::Measured};
    host.values[uint32_t(performance::PerfSeries::StudioCpu)] = {1000000, ncl_perf::PerfSampleState::Measured};
    host.values[uint32_t(performance::PerfSeries::RadarCpu)] = {2000000, ncl_perf::PerfSampleState::Measured};
    host.values[uint32_t(performance::PerfSeries::SwapCpu)] = {3000000, ncl_perf::PerfSampleState::Measured};
    history.Append(host);
    output.output_id = 2;
    output.completed_at += 1;
    output.values[uint32_t(performance::PerfSeries::RenderGpu)].duration_ns = 5000000;
    output.values[uint32_t(performance::PerfSeries::StudioGpu)].duration_ns = 4000000;
    history.Append(output);
    host.completed_at += 1;
    host.values[uint32_t(performance::PerfSeries::HostCpu)].duration_ns = 8000000;
    host.values[uint32_t(performance::PerfSeries::StudioCpu)].duration_ns = 4000000;
    history.Append(host);

    performance::PerfSnapshot snapshot;
    performance::PerfGroupedGraphs groups;
    history.BuildSnapshot(10000001, snapshot, &groups);
    const performance::PerfGroupedGraphBin& cpu = groups.cpu.back();
    const performance::PerfGroupedGraphBin& gpu = groups.gpu.back();
    EXPECT_EQ(cpu.total.maximum_ns, 9000000u);
    EXPECT_EQ(cpu.total.mean_ns, 8500000u);
    EXPECT_TRUE(cpu.parts_available);
    EXPECT_EQ(cpu.parts, (std::array<ncl_perf::PerfDuration_t, 3>{1000000, 2000000, 3000000}));
    EXPECT_EQ(gpu.total.maximum_ns, 6000000u);
    EXPECT_TRUE(gpu.parts_available);
    EXPECT_EQ(gpu.parts, (std::array<ncl_perf::PerfDuration_t, 3>{1000000, 2000000, 0}));
    EXPECT_EQ(snapshot.summaries[uint32_t(performance::PerfSeries::SwapCpu)].count, 2u);
}

TEST(FrameHistoryTest, DelayedGpuPartsFillTheirOriginalBinAndMissingPartsStayGaps)
{
    performance::PerfFrameHistory history;
    performance::PerfFrameRecord output = Output(1, 1000000000, 1000000000);
    output.frame_id = 7;
    output.values[uint32_t(performance::PerfSeries::RenderGpu)] = {6000000, ncl_perf::PerfSampleState::Measured};
    output.values[uint32_t(performance::PerfSeries::StudioGpu)].state = ncl_perf::PerfSampleState::Pending;
    output.values[uint32_t(performance::PerfSeries::RadarGpu)].state = ncl_perf::PerfSampleState::Inactive;
    history.Append(output);
    history.Append(Output(2, 2000000000, 1000000000));
    performance::PerfSnapshot snapshot;
    performance::PerfGroupedGraphs groups;
    history.BuildSnapshot(2000000000, snapshot, &groups);
    auto delayed = std::find_if(groups.gpu.begin(), groups.gpu.end(), [](const performance::PerfGroupedGraphBin& bin) {
        return bin.total.count != 0;
    });
    ASSERT_NE(delayed, groups.gpu.end());
    uint32_t delayed_bin = uint32_t(std::distance(groups.gpu.begin(), delayed));
    EXPECT_FALSE(groups.gpu[delayed_bin].parts_available);
    EXPECT_EQ(groups.gpu[delayed_bin].total.maximum_ns, 6000000u);
    performance::PerfGpuResult result{7, 1, 0, 1, 2, 2000000, performance::PerfSeries::StudioGpu, ncl_perf::PerfSampleState::Measured};
    ASSERT_TRUE(history.ApplyGpu(result));
    history.BuildSnapshot(2000000000, snapshot, &groups);
    EXPECT_TRUE(groups.gpu[delayed_bin].parts_available);
    EXPECT_EQ(groups.gpu[delayed_bin].parts[0], 2000000u);
    EXPECT_EQ(groups.gpu.back().total.count, 0u);
    history.Reset(2);
    history.BuildSnapshot(2000000000, snapshot, &groups);
    EXPECT_EQ(groups.gpu[delayed_bin].total.count, 0u);
    EXPECT_FALSE(groups.gpu[delayed_bin].parts_available);
}

TEST(FrameHistoryTest, OverlappingOrUnavailablePartsCannotBecomeAStack)
{
    for (ncl_perf::PerfSampleState state :
         {ncl_perf::PerfSampleState::Measured, ncl_perf::PerfSampleState::Unavailable, ncl_perf::PerfSampleState::Incomplete})
    {
        performance::PerfFrameHistory history;
        performance::PerfFrameRecord output = Output(1, 10000000, 10000000);
        output.values[uint32_t(performance::PerfSeries::RenderGpu)] = {1000, ncl_perf::PerfSampleState::Measured};
        output.values[uint32_t(performance::PerfSeries::StudioGpu)] = {900, ncl_perf::PerfSampleState::Measured};
        output.values[uint32_t(performance::PerfSeries::RadarGpu)] = {200, state};
        history.Append(output);
        performance::PerfSnapshot snapshot;
        performance::PerfGroupedGraphs groups;
        history.BuildSnapshot(10000000, snapshot, &groups);
        EXPECT_FALSE(groups.gpu.back().parts_available);
    }
}

TEST(FrameHistoryTest, GroupedGraphsKeepNeighboringOutputAndHostSamplesInSeparateBins)
{
    performance::PerfFrameHistory history;
    performance::PerfFrameRecord first = Output(1, 1000000000, 10000000);
    first.values[uint32_t(performance::PerfSeries::RenderGpu)] = {6000000, ncl_perf::PerfSampleState::Measured};
    history.Append(first);
    performance::PerfFrameRecord host = first;
    host.kind = performance::PerfRecordKind::Host;
    host.values[uint32_t(performance::PerfSeries::HostCpu)] = {9000000, ncl_perf::PerfSampleState::Measured};
    host.values[uint32_t(performance::PerfSeries::StudioCpu)] = {1000000, ncl_perf::PerfSampleState::Measured};
    host.values[uint32_t(performance::PerfSeries::RadarCpu)] = {2000000, ncl_perf::PerfSampleState::Measured};
    host.values[uint32_t(performance::PerfSeries::SwapCpu)] = {3000000, ncl_perf::PerfSampleState::Measured};
    history.Append(host);
    performance::PerfFrameRecord second = Output(2, 1050000000, 11000000);
    second.values[uint32_t(performance::PerfSeries::RenderGpu)] = {5000000, ncl_perf::PerfSampleState::Measured};
    history.Append(second);
    host.completed_at = second.completed_at;
    host.values[uint32_t(performance::PerfSeries::HostCpu)].duration_ns = 8000000;
    host.values[uint32_t(performance::PerfSeries::StudioCpu)].duration_ns = 2000000;
    history.Append(host);

    performance::PerfSnapshot snapshot;
    performance::PerfGroupedGraphs groups;
    history.BuildSnapshot(2000000000, snapshot, &groups);
    uint32_t measured_cpu = 0;
    uint32_t measured_gpu = 0;
    for (const performance::PerfGroupedGraphBin& bin : groups.cpu)
    {
        if (bin.total.count)
        {
            ++measured_cpu;
            EXPECT_EQ(bin.total.count, 1u);
            EXPECT_EQ(bin.interval.count, 1u);
            EXPECT_TRUE(bin.parts_available);
            EXPECT_EQ(bin.parts[0], measured_cpu == 1 ? 1000000u : 2000000u);
        }
    }
    for (const performance::PerfGroupedGraphBin& bin : groups.gpu)
    {
        if (bin.total.count)
        {
            ++measured_gpu;
            EXPECT_EQ(bin.total.count, 1u);
            EXPECT_EQ(bin.total.maximum_ns, measured_gpu == 1 ? 6000000u : 5000000u);
        }
    }
    EXPECT_EQ(measured_cpu, 2u);
    EXPECT_EQ(measured_gpu, 2u);
}

TEST(FrameHistoryTest, TimeBinsRetainSingleSpike)
{
    performance::PerfFrameHistory history;
    history.Append(Output(1, 1000000, 1000000));
    history.Append(Output(2, 5010000000, 50000000));
    history.Append(Output(3, 5011000000, 1000000));
    history.Append(Output(4, 10000000000, 1000000));
    performance::PerfSnapshot snapshot;
    history.BuildSnapshot(10000000000, snapshot);
    const performance::PerfGraphBin& bin = snapshot.graphs[0][128];
    EXPECT_EQ(bin.count, 2u);
    EXPECT_EQ(bin.maximum_ns, 50000000u);
    EXPECT_EQ(bin.mean_ns, 25500000u);
    EXPECT_EQ(snapshot.graphs[0][120].count, 0u);
}

TEST(FrameHistoryTest, ShortHistoryStartsAtTheRightAndMovesLeftAsTimePasses)
{
    performance::PerfFrameHistory history;
    performance::PerfFrameRecord output = Output(1, 100000000000, 16000000);
    output.values[uint32_t(performance::PerfSeries::RenderGpu)] = {6000000, ncl_perf::PerfSampleState::Measured};
    history.Append(output);
    performance::PerfFrameRecord host = output;
    host.kind = performance::PerfRecordKind::Host;
    host.values[uint32_t(performance::PerfSeries::HostCpu)] = {9000000, ncl_perf::PerfSampleState::Measured};
    history.Append(host);
    performance::PerfSnapshot snapshot;
    performance::PerfGroupedGraphs groups;
    history.BuildSnapshot(100000000000, snapshot, &groups);
    EXPECT_EQ(snapshot.coverage_ns, 16000000u);
    EXPECT_EQ(snapshot.graphs[0].front().count, 0u);
    EXPECT_EQ(snapshot.graphs[0].back().count, 1u);
    EXPECT_EQ(groups.cpu.back().interval.count, 1u);

    history.BuildSnapshot(101000000000, snapshot, &groups);
    EXPECT_EQ(snapshot.graphs[0][230].count, 1u);
    EXPECT_EQ(snapshot.graphs[0].back().count, 0u);
    auto cpu_bin = std::find_if(groups.cpu.begin(), groups.cpu.end(), [](const performance::PerfGroupedGraphBin& bin) {
        return bin.total.count != 0;
    });
    auto gpu_bin = std::find_if(groups.gpu.begin(), groups.gpu.end(), [](const performance::PerfGroupedGraphBin& bin) {
        return bin.total.count != 0;
    });
    ASSERT_NE(cpu_bin, groups.cpu.end());
    ASSERT_NE(gpu_bin, groups.gpu.end());
    EXPECT_EQ(std::distance(groups.cpu.begin(), cpu_bin), 230);
    EXPECT_EQ(std::distance(groups.gpu.begin(), gpu_bin), 230);
    EXPECT_EQ(cpu_bin->total.maximum_ns, 9000000u);
    EXPECT_EQ(gpu_bin->total.maximum_ns, 6000000u);
    EXPECT_EQ(groups.cpu.back().interval.count, 0u);
    history.Append(Output(2, 101000000000, 1000000000));
    history.BuildSnapshot(101000000000, snapshot, &groups);
    EXPECT_EQ(snapshot.graphs[0][230].count, 1u);
    EXPECT_EQ(snapshot.graphs[0].back().count, 1u);
    EXPECT_EQ(cpu_bin->interval.count, 1u);
    EXPECT_EQ(groups.cpu.back().interval.count, 1u);
    EXPECT_EQ(snapshot.summaries[0].count, 2u);
    EXPECT_EQ(snapshot.coverage_ns, 1016000000u);
}

TEST(FrameHistoryTest, MedianUsesMeasurementsRatherThanBinMeans)
{
    performance::PerfFrameHistory history;
    history.Append(Output(1, 1000000, 1000000));
    history.Append(Output(2, 5010000000, 12000000));
    history.Append(Output(3, 5011000000, 3000000));
    history.Append(Output(4, 5012000000, 2500000));
    history.Append(Output(5, 10000000000, 2000000));

    performance::PerfSnapshot snapshot;
    history.BuildSnapshot(10000000000, snapshot);
    const performance::PerfSummary& summary = snapshot.summaries[0];
    EXPECT_EQ(summary.count, 5u);
    EXPECT_EQ(summary.median_ns, 2500000u);
    EXPECT_EQ(summary.mean_ns, 4100000u);
    EXPECT_EQ(summary.maximum_ns, 12000000u);
}

TEST(FrameHistoryTest, EvenMedianAveragesCentralMeasurements)
{
    performance::PerfFrameHistory history;
    history.Append(Output(1, 1000000, 12000000));
    history.Append(Output(2, 2000000, 2000000));
    history.Append(Output(3, 3000000, 3000001));
    history.Append(Output(4, 4000000, 2500000));

    performance::PerfSnapshot snapshot;
    history.BuildSnapshot(4000000, snapshot);
    EXPECT_EQ(snapshot.summaries[0].median_ns, 2750000u);
}

TEST(FrameHistoryTest, MedianIncludesMeasuredZeroAndExcludesExpiredOrInvalidSamples)
{
    performance::PerfFrameHistory history;
    performance::PerfFrameRecord expired = Output(1, 1000000, 1000000);
    expired.values[uint32_t(performance::PerfSeries::RenderGpu)] = {9000000, ncl_perf::PerfSampleState::Measured};
    history.Append(expired);

    uint64_t id = 2;
    for (ncl_perf::PerfSampleState state :
         {ncl_perf::PerfSampleState::Pending,
          ncl_perf::PerfSampleState::Incomplete,
          ncl_perf::PerfSampleState::Dropped,
          ncl_perf::PerfSampleState::Inactive,
          ncl_perf::PerfSampleState::Unavailable,
          ncl_perf::PerfSampleState::Measured})
    {
        performance::PerfFrameRecord record = Output(id, 11000000000 + id * 1000000, 1000000);
        record.values[uint32_t(performance::PerfSeries::RenderGpu)] = {state == ncl_perf::PerfSampleState::Measured ? 0u : 9000000u, state};
        history.Append(record);
        ++id;
    }
    performance::PerfFrameRecord last = Output(id, 11000000000 + id * 1000000, 1000000);
    last.values[uint32_t(performance::PerfSeries::RenderGpu)] = {6000000, ncl_perf::PerfSampleState::Measured};
    history.Append(last);

    performance::PerfSnapshot snapshot;
    history.BuildSnapshot(last.completed_at, snapshot);
    const performance::PerfSummary& summary = snapshot.summaries[uint32_t(performance::PerfSeries::RenderGpu)];
    EXPECT_EQ(summary.count, 2u);
    EXPECT_EQ(summary.median_ns, 3000000u);
    EXPECT_EQ(summary.mean_ns, 3000000u);
}

TEST(FrameHistoryTest, CapacityReportsActualCoverage)
{
    performance::PerfFrameHistory history;
    for (uint64_t id = 1; id <= 16385; ++id)
    {
        history.Append(Output(id, id * 250000, 250000));
    }
    performance::PerfSnapshot snapshot;
    history.BuildSnapshot(4096250000, snapshot);
    EXPECT_EQ(snapshot.record_count, 16384u);
    EXPECT_EQ(snapshot.coverage_ns, 4096000000u);
    EXPECT_TRUE(snapshot.capacity_limited);
}

TEST(FrameHistoryTest, LateGpuResultUpdatesOriginalOutput)
{
    performance::PerfFrameHistory history;
    history.Append(Output(1, 10000000, 10000000));
    history.Append(Output(2, 30000000, 20000000));
    performance::PerfGpuResult result;
    result.output_id = 1;
    result.collector_generation = 1;
    result.context_generation = 2;
    result.duration_ns = 7000000;
    result.series = performance::PerfSeries::RenderGpu;
    EXPECT_TRUE(history.ApplyGpu(result));
    performance::PerfSnapshot snapshot;
    history.BuildSnapshot(30000000, snapshot);
    const performance::PerfSummary& summary = snapshot.summaries[uint32_t(performance::PerfSeries::RenderGpu)];
    EXPECT_EQ(summary.count, 1u);
    EXPECT_EQ(summary.mean_ns, 7000000u);
    EXPECT_EQ(snapshot.latest.output_id, 2u);
    EXPECT_EQ(snapshot.latest.values[uint32_t(performance::PerfSeries::RenderGpu)].state, ncl_perf::PerfSampleState::Unavailable);
}

TEST(FrameHistoryTest, ResetRejectsGpuFromPreviousGeneration)
{
    performance::PerfFrameHistory history;
    history.Append(Output(1, 10000000, 10000000));
    history.Reset(2);
    performance::PerfGpuResult result;
    result.output_id = 1;
    result.collector_generation = 1;
    result.context_generation = 2;
    EXPECT_FALSE(history.ApplyGpu(result));
    performance::PerfSnapshot snapshot;
    history.BuildSnapshot(10000000, snapshot);
    EXPECT_EQ(snapshot.record_count, 0u);
}

TEST(FrameHistoryTest, OverwrittenRecordRejectsGpu)
{
    performance::PerfFrameHistory history;
    history.Append(Output(1, 10000000, 10000000));
    for (uint64_t id = 2; id <= performance::kPerfHistoryCapacity + 1; ++id)
    {
        history.Append(Output(id, 11000000000 + id, 20000000));
    }
    performance::PerfGpuResult result;
    result.output_id = 1;
    result.collector_generation = 1;
    result.context_generation = 2;
    EXPECT_FALSE(history.ApplyGpu(result));
}

TEST(FrameHistoryTest, IncompleteSamplesAreExcludedAndZeroGpuRemainsMeasured)
{
    performance::PerfFrameHistory history;
    performance::PerfFrameRecord incomplete = Output(1, 1000000, 1000000);
    incomplete.values[uint32_t(performance::PerfSeries::RenderGpu)] = {500, ncl_perf::PerfSampleState::Incomplete};
    history.Append(incomplete);
    performance::PerfFrameRecord measured = Output(2, 2000000, 1000000);
    measured.values[uint32_t(performance::PerfSeries::RenderGpu)] = {0, ncl_perf::PerfSampleState::Measured};
    history.Append(measured);
    performance::PerfSnapshot snapshot;
    history.BuildSnapshot(2000000, snapshot);
    EXPECT_EQ(snapshot.summaries[uint32_t(performance::PerfSeries::RenderGpu)].count, 1u);
    EXPECT_EQ(snapshot.summaries[uint32_t(performance::PerfSeries::RenderGpu)].mean_ns, 0u);
}

TEST(FrameHistoryTest, RepeatedGpuUpdatesKeepTheFirstMatchingRecordAfterRingOverwrite)
{
    performance::PerfFrameHistory history;
    history.Append(Output(7, 1, 1));
    history.Append(Output(7, 2, 1));
    for (uint64_t index = 3; index <= performance::kPerfHistoryCapacity; ++index)
    {
        history.Append(Output(1000 + index, index, 1));
    }
    performance::PerfGpuResult result;
    result.output_id = 7;
    result.collector_generation = 1;
    result.context_generation = 2;
    result.series = performance::PerfSeries::RenderGpu;
    result.state = ncl_perf::PerfSampleState::Measured;
    result.duration_ns = 100;
    ASSERT_TRUE(history.ApplyGpu(result));
    history.Append(Output(7, performance::kPerfHistoryCapacity + 1, 1));
    ASSERT_TRUE(history.ApplyGpu(result));
    ASSERT_TRUE(history.ApplyGpu(result));
    performance::PerfSnapshot snapshot;
    history.BuildSnapshot(performance::kPerfHistoryCapacity + 1, snapshot);
    EXPECT_EQ(snapshot.latest.output_id, 7u);
    EXPECT_EQ(snapshot.latest.values[uint32_t(performance::PerfSeries::RenderGpu)].state, ncl_perf::PerfSampleState::Unavailable);
    EXPECT_EQ(snapshot.summaries[uint32_t(performance::PerfSeries::RenderGpu)].mean_ns, 200u);
}

TEST(FrameHistoryTest, RepeatedGpuUpdatesValidateFrameContextAndRetention)
{
    performance::PerfFrameHistory history;
    performance::PerfFrameRecord first = Output(7, 1, 1);
    first.frame_id = 10;
    history.Append(first);
    performance::PerfGpuResult result;
    result.output_id = 7;
    result.frame_id = 10;
    result.collector_generation = 1;
    result.context_generation = 2;
    result.series = performance::PerfSeries::RenderGpu;
    result.state = ncl_perf::PerfSampleState::Measured;
    result.duration_ns = 100;
    ASSERT_TRUE(history.ApplyGpu(result));
    performance::PerfFrameRecord second = Output(7, 2, 1);
    second.frame_id = 20;
    history.Append(second);
    result.frame_id = 20;
    ASSERT_TRUE(history.ApplyGpu(result));
    second.context_generation = 3;
    second.completed_at = 3;
    history.Append(second);
    result.context_generation = 3;
    ASSERT_TRUE(history.ApplyGpu(result));
    performance::PerfSnapshot snapshot;
    history.BuildSnapshot(3, snapshot);
    EXPECT_EQ(snapshot.latest.values[uint32_t(performance::PerfSeries::RenderGpu)].duration_ns, 100u);
    history.Append(Output(9, 12000000000, 1));
    EXPECT_TRUE(history.ApplyGpu(result));
    history.Reset(1);
    EXPECT_FALSE(history.ApplyGpu(result));
    history.Append(second);
    ASSERT_TRUE(history.ApplyGpu(result));
    history.BuildSnapshot(3, snapshot);
    EXPECT_EQ(snapshot.latest.values[uint32_t(performance::PerfSeries::RenderGpu)].duration_ns, 100u);
}

TEST(FrameHistoryTest, CountsRetainedOutputsAfterIdentifierGapsAndIgnoresHostRecords)
{
    performance::PerfFrameHistory history;
    EXPECT_EQ(history.CountOutputsAfter(0), 0u);
    history.Append(Output(1, 1, 1));
    performance::PerfFrameRecord host = Output(100, 2, 1);
    host.kind = performance::PerfRecordKind::Host;
    history.Append(host);
    history.Append(Output(5, 3, 1));
    history.Append(Output(9, 4, 1));
    EXPECT_EQ(history.CountOutputsAfter(0), 3u);
    EXPECT_EQ(history.CountOutputsAfter(1), 2u);
    EXPECT_EQ(history.CountOutputsAfter(5), 1u);
    EXPECT_EQ(history.CountOutputsAfter(9), 0u);
    EXPECT_EQ(history.CountOutputsAfter(10), 0u);
    history.Reset(2);
    EXPECT_EQ(history.CountOutputsAfter(0), 0u);
}

TEST(FrameHistoryTest, CountsOutputsAcrossRingWrapAndLongTimestampGap)
{
    performance::PerfFrameHistory history;
    for (uint64_t index = 1; index <= performance::kPerfHistoryCapacity + 2; ++index)
    {
        history.Append(Output(index * 3, index, 1));
    }
    EXPECT_EQ(history.CountOutputsAfter(0), performance::kPerfHistoryCapacity);
    EXPECT_EQ(history.CountOutputsAfter((performance::kPerfHistoryCapacity - 2) * 3), 4u);
    history.Append(Output((performance::kPerfHistoryCapacity + 3) * 3, performance::kPerfHistoryDuration + 100000, 1));
    EXPECT_EQ(history.CountOutputsAfter(0), performance::kPerfHistoryCapacity);
    EXPECT_EQ(history.CountOutputsAfter((performance::kPerfHistoryCapacity - 2) * 3), 5u);
}

TEST(FrameHistoryTest, FrameGraphsKeepNeighboringFramesInIndividualRightAlignedColumns)
{
    performance::PerfFrameHistory history;
    for (uint64_t id = 1; id <= 3; ++id)
    {
        performance::PerfFrameRecord output = Output(id, 1000000 + id, id * 1000);
        output.frame_id = id;
        output.values[uint32_t(performance::PerfSeries::RenderGpu)] = {id * 200, ncl_perf::PerfSampleState::Measured};
        history.Append(output);

        performance::PerfFrameRecord host;
        host.kind = performance::PerfRecordKind::Host;
        host.frame_id = id;
        host.completed_at = output.completed_at;
        host.collector_generation = 1;
        host.values[uint32_t(performance::PerfSeries::HostCpu)] = {id * 300, ncl_perf::PerfSampleState::Measured};
        history.Append(host);
    }

    performance::PerfFrameGraphs graphs;
    history.BuildFrameGraphs(1000003, 5, graphs);
    ASSERT_EQ(graphs.frames.size(), 5u);
    ASSERT_EQ(graphs.count, 3u);
    EXPECT_EQ(graphs.captured_at, 1000003u);
    EXPECT_EQ(graphs.collector_generation, 1u);
    EXPECT_EQ(graphs.frames[0].output_id, 0u);
    EXPECT_EQ(graphs.frames[1].values[uint32_t(performance::PerfSeries::HostCpu)].state, ncl_perf::PerfSampleState::Unavailable);
    EXPECT_EQ(graphs.frames[2].frame_id, 1u);
    EXPECT_EQ(graphs.frames[2].output_id, 1u);
    EXPECT_EQ(graphs.frames[2].values[uint32_t(performance::PerfSeries::FrameInterval)].duration_ns, 1000u);
    EXPECT_EQ(graphs.frames[2].values[uint32_t(performance::PerfSeries::HostCpu)].duration_ns, 300u);
    EXPECT_EQ(graphs.frames[2].values[uint32_t(performance::PerfSeries::RenderGpu)].duration_ns, 200u);
    EXPECT_EQ(graphs.frames[3].frame_id, 2u);
    EXPECT_EQ(graphs.frames[3].output_id, 2u);
    EXPECT_EQ(graphs.frames[3].values[uint32_t(performance::PerfSeries::FrameInterval)].duration_ns, 2000u);
    EXPECT_EQ(graphs.frames[3].values[uint32_t(performance::PerfSeries::HostCpu)].duration_ns, 600u);
    EXPECT_EQ(graphs.frames[3].values[uint32_t(performance::PerfSeries::RenderGpu)].duration_ns, 400u);
    EXPECT_EQ(graphs.frames[4].frame_id, 3u);
    EXPECT_EQ(graphs.frames[4].output_id, 3u);
    EXPECT_EQ(graphs.frames[4].values[uint32_t(performance::PerfSeries::FrameInterval)].duration_ns, 3000u);
}

TEST(FrameHistoryTest, FrameGraphsKeepMissingGpuSlotsAndApplyLateResultsToTheirOriginalOutput)
{
    performance::PerfFrameHistory history;
    for (uint64_t id = 1; id <= 3; ++id)
    {
        performance::PerfFrameRecord output = Output(id, id * 1000, 1000);
        output.frame_id = id * 10;
        output.values[uint32_t(performance::PerfSeries::RenderGpu)] = {id * 100, ncl_perf::PerfSampleState::Measured};
        if (id == 2)
        {
            output.values[uint32_t(performance::PerfSeries::RenderGpu)] = {0, ncl_perf::PerfSampleState::Pending};
            output.values[uint32_t(performance::PerfSeries::StudioGpu)] = {0, ncl_perf::PerfSampleState::Pending};
        }
        history.Append(output);
    }

    performance::PerfFrameGraphs graphs;
    history.BuildFrameGraphs(3000, 3, graphs);
    ASSERT_EQ(graphs.count, 3u);
    ASSERT_EQ(graphs.frames.size(), 3u);
    EXPECT_EQ(graphs.frames[0].output_id, 1u);
    EXPECT_EQ(graphs.frames[1].output_id, 2u);
    EXPECT_EQ(graphs.frames[1].values[uint32_t(performance::PerfSeries::RenderGpu)].state, ncl_perf::PerfSampleState::Pending);
    EXPECT_EQ(graphs.frames[2].values[uint32_t(performance::PerfSeries::RenderGpu)].duration_ns, 300u);

    performance::PerfGpuResult result{20, 2, 0, 1, 2, 0, performance::PerfSeries::RenderGpu, ncl_perf::PerfSampleState::Measured};
    ASSERT_TRUE(history.ApplyGpu(result));
    result.series = performance::PerfSeries::StudioGpu;
    result.duration_ns = 50;
    ASSERT_TRUE(history.ApplyGpu(result));
    history.BuildFrameGraphs(3000, 3, graphs);
    ASSERT_EQ(graphs.count, 3u);
    EXPECT_EQ(graphs.frames[1].frame_id, 20u);
    EXPECT_EQ(graphs.frames[1].output_id, 2u);
    EXPECT_EQ(graphs.frames[1].values[uint32_t(performance::PerfSeries::RenderGpu)].duration_ns, 0u);
    EXPECT_EQ(graphs.frames[1].values[uint32_t(performance::PerfSeries::RenderGpu)].state, ncl_perf::PerfSampleState::Measured);
    EXPECT_EQ(graphs.frames[1].values[uint32_t(performance::PerfSeries::StudioGpu)].duration_ns, 50u);
    EXPECT_EQ(graphs.frames[2].output_id, 3u);
    EXPECT_EQ(graphs.frames[2].values[uint32_t(performance::PerfSeries::StudioGpu)].state, ncl_perf::PerfSampleState::Unavailable);
}

TEST(FrameHistoryTest, FrameGraphsAttachHostOnceToItsLastOutputAndKeepHostsWithoutOutputs)
{
    performance::PerfFrameHistory history;
    performance::PerfFrameRecord host;
    host.kind = performance::PerfRecordKind::Host;
    host.frame_id = 1;
    host.completed_at = 100;
    host.collector_generation = 1;
    host.values[uint32_t(performance::PerfSeries::HostCpu)] = {1000, ncl_perf::PerfSampleState::Measured};
    history.Append(host);

    performance::PerfFrameRecord output = Output(10, 200, 100);
    output.frame_id = 2;
    output.values[uint32_t(performance::PerfSeries::HostCpu)] = {9999, ncl_perf::PerfSampleState::Measured};
    output.values[uint32_t(performance::PerfSeries::SwapCpu)] = {9999, ncl_perf::PerfSampleState::Measured};
    output.values[uint32_t(performance::PerfSeries::MonitorCpu)] = {10, ncl_perf::PerfSampleState::Measured};
    output.values[uint32_t(performance::PerfSeries::RenderGpu)] = {500, ncl_perf::PerfSampleState::Measured};
    output.values[uint32_t(performance::PerfSeries::StudioGpu)] = {200, ncl_perf::PerfSampleState::Measured};
    output.values[uint32_t(performance::PerfSeries::RadarGpu)] = {50, ncl_perf::PerfSampleState::Inactive};
    history.Append(output);
    output.output_id = 11;
    output.completed_at = 300;
    output.values[uint32_t(performance::PerfSeries::MonitorCpu)].duration_ns = 20;
    history.Append(output);

    host.frame_id = 2;
    host.completed_at = 400;
    host.values[uint32_t(performance::PerfSeries::HostCpu)].duration_ns = 2000;
    host.values[uint32_t(performance::PerfSeries::StudioCpu)] = {200, ncl_perf::PerfSampleState::Measured};
    host.values[uint32_t(performance::PerfSeries::RadarCpu)] = {0, ncl_perf::PerfSampleState::Inactive};
    host.values[uint32_t(performance::PerfSeries::SwapCpu)] = {300, ncl_perf::PerfSampleState::Measured};
    host.values[uint32_t(performance::PerfSeries::FrameInterval)] = {9999, ncl_perf::PerfSampleState::Measured};
    host.values[uint32_t(performance::PerfSeries::RenderGpu)] = {9999, ncl_perf::PerfSampleState::Measured};
    host.values[uint32_t(performance::PerfSeries::MonitorCpu)] = {9999, ncl_perf::PerfSampleState::Measured};
    history.Append(host);
    history.Append(Output(12, 500, 100));
    host.frame_id = 3;
    host.completed_at = 600;
    host.values[uint32_t(performance::PerfSeries::HostCpu)].duration_ns = 3000;
    history.Append(host);

    performance::PerfFrameGraphs graphs;
    history.BuildFrameGraphs(600, 6, graphs);
    ASSERT_EQ(graphs.count, 5u);
    ASSERT_EQ(graphs.frames.size(), 6u);
    EXPECT_EQ(graphs.frames[1].frame_id, 1u);
    EXPECT_EQ(graphs.frames[1].output_id, 0u);
    EXPECT_EQ(graphs.frames[1].values[uint32_t(performance::PerfSeries::HostCpu)].duration_ns, 1000u);
    EXPECT_EQ(graphs.frames[1].values[uint32_t(performance::PerfSeries::FrameInterval)].state, ncl_perf::PerfSampleState::Unavailable);
    EXPECT_EQ(graphs.frames[2].output_id, 10u);
    EXPECT_EQ(graphs.frames[2].values[uint32_t(performance::PerfSeries::HostCpu)].state, ncl_perf::PerfSampleState::Unavailable);
    EXPECT_EQ(graphs.frames[2].values[uint32_t(performance::PerfSeries::SwapCpu)].state, ncl_perf::PerfSampleState::Unavailable);
    EXPECT_EQ(graphs.frames[2].values[uint32_t(performance::PerfSeries::MonitorCpu)].duration_ns, 10u);
    EXPECT_EQ(graphs.frames[3].frame_id, 2u);
    EXPECT_EQ(graphs.frames[3].output_id, 11u);
    EXPECT_EQ(graphs.frames[3].values[uint32_t(performance::PerfSeries::HostCpu)].duration_ns, 2000u);
    EXPECT_EQ(graphs.frames[3].values[uint32_t(performance::PerfSeries::StudioCpu)].duration_ns, 200u);
    EXPECT_EQ(graphs.frames[3].values[uint32_t(performance::PerfSeries::RadarCpu)].state, ncl_perf::PerfSampleState::Inactive);
    EXPECT_EQ(graphs.frames[3].values[uint32_t(performance::PerfSeries::SwapCpu)].duration_ns, 300u);
    EXPECT_EQ(graphs.frames[3].values[uint32_t(performance::PerfSeries::FrameInterval)].duration_ns, 100u);
    EXPECT_EQ(graphs.frames[3].values[uint32_t(performance::PerfSeries::RenderGpu)].duration_ns, 500u);
    EXPECT_EQ(graphs.frames[3].values[uint32_t(performance::PerfSeries::StudioGpu)].duration_ns, 200u);
    EXPECT_EQ(graphs.frames[3].values[uint32_t(performance::PerfSeries::RadarGpu)].state, ncl_perf::PerfSampleState::Inactive);
    EXPECT_EQ(graphs.frames[3].values[uint32_t(performance::PerfSeries::MonitorCpu)].duration_ns, 20u);
    EXPECT_EQ(graphs.frames[4].output_id, 12u);
    EXPECT_EQ(graphs.frames[4].values[uint32_t(performance::PerfSeries::HostCpu)].state, ncl_perf::PerfSampleState::Unavailable);
    EXPECT_EQ(graphs.frames[5].frame_id, 3u);
    EXPECT_EQ(graphs.frames[5].output_id, 0u);
    EXPECT_EQ(graphs.frames[5].values[uint32_t(performance::PerfSeries::HostCpu)].duration_ns, 3000u);
    EXPECT_EQ(graphs.frames[5].values[uint32_t(performance::PerfSeries::RenderGpu)].state, ncl_perf::PerfSampleState::Unavailable);
    EXPECT_EQ(graphs.frames[5].values[uint32_t(performance::PerfSeries::MonitorCpu)].state, ncl_perf::PerfSampleState::Unavailable);
}

TEST(FrameHistoryTest, FrameGraphsResizeWithoutLosingRecentFramesOrReallocatingAvailableStorage)
{
    performance::PerfFrameHistory history;
    history.Append(Output(1, 100, 100));
    history.Append(Output(2, 200, 100));
    history.Append(Output(3, 300, 100));

    performance::PerfFrameGraphs graphs;
    history.BuildFrameGraphs(300, 8, graphs);
    ASSERT_EQ(graphs.count, 3u);
    ASSERT_EQ(graphs.frames.size(), 8u);
    const performance::PerfFrameGraph* storage = graphs.frames.data();
    EXPECT_EQ(graphs.frames[5].output_id, 1u);
    EXPECT_EQ(graphs.frames[7].output_id, 3u);

    history.BuildFrameGraphs(300, 2, graphs);
    ASSERT_EQ(graphs.count, 2u);
    ASSERT_EQ(graphs.frames.size(), 2u);
    EXPECT_EQ(graphs.frames.data(), storage);
    EXPECT_EQ(graphs.frames[0].output_id, 2u);
    EXPECT_EQ(graphs.frames[1].output_id, 3u);

    history.BuildFrameGraphs(300, 6, graphs);
    ASSERT_EQ(graphs.count, 3u);
    ASSERT_EQ(graphs.frames.size(), 6u);
    EXPECT_EQ(graphs.frames.data(), storage);
    EXPECT_EQ(graphs.frames[0].output_id, 0u);
    EXPECT_EQ(graphs.frames[1].values[uint32_t(performance::PerfSeries::FrameInterval)].state, ncl_perf::PerfSampleState::Unavailable);
    EXPECT_EQ(graphs.frames[2].output_id, 0u);
    EXPECT_EQ(graphs.frames[3].output_id, 1u);
    EXPECT_EQ(graphs.frames[5].output_id, 3u);

    history.BuildFrameGraphs(300, 0, graphs);
    EXPECT_TRUE(graphs.frames.empty());
    EXPECT_EQ(graphs.count, 0u);
    history.BuildFrameGraphs(300, 6, graphs);
    ASSERT_EQ(graphs.count, 3u);
    EXPECT_EQ(graphs.frames.data(), storage);
    EXPECT_EQ(graphs.frames[3].output_id, 1u);
}

TEST(FrameHistoryTest, FrameGraphsFilterFutureRecordsAndClearAfterReset)
{
    performance::PerfFrameHistory history;
    history.Append(Output(1, 2000000000, 100));
    history.Append(Output(2, 3000000000, 100));
    history.Append(Output(3, 4000000000, 100));

    performance::PerfFrameGraphs graphs;
    history.BuildFrameGraphs(3000000000, 3, graphs);
    ASSERT_EQ(graphs.count, 2u);
    EXPECT_EQ(graphs.frames[1].output_id, 1u);
    EXPECT_EQ(graphs.frames[2].output_id, 2u);

    history.BuildFrameGraphs(15000000000, 3, graphs);
    ASSERT_EQ(graphs.count, 3u);
    EXPECT_EQ(graphs.frames[0].output_id, 1u);
    EXPECT_EQ(graphs.frames[1].output_id, 2u);
    EXPECT_EQ(graphs.frames[2].output_id, 3u);
    history.Reset(2);
    history.Append(Output(4, 100, 100));
    history.BuildFrameGraphs(100, 3, graphs);
    EXPECT_EQ(graphs.count, 0u);
    EXPECT_EQ(graphs.collector_generation, 2u);

    performance::PerfFrameRecord output = Output(5, 200, 100);
    output.collector_generation = 2;
    history.Append(output);
    history.BuildFrameGraphs(200, 3, graphs);
    ASSERT_EQ(graphs.count, 1u);
    EXPECT_EQ(graphs.frames[1].output_id, 0u);
    EXPECT_EQ(graphs.frames[2].output_id, 5u);
    EXPECT_EQ(graphs.captured_at, 200u);
}

TEST(FrameHistoryTest, FrameGraphsClampColumnsAndKeepNewestOutputsAcrossRingWrap)
{
    performance::PerfFrameHistory history;
    for (uint64_t id = 1; id <= performance::kPerfHistoryCapacity + 2; ++id)
    {
        history.Append(Output(id, id, 1));
    }

    performance::PerfFrameGraphs graphs;
    history.BuildFrameGraphs(performance::kPerfHistoryCapacity + 2, performance::kPerfHistoryCapacity + 100, graphs);
    ASSERT_EQ(graphs.frames.size(), performance::kPerfHistoryCapacity);
    ASSERT_EQ(graphs.count, performance::kPerfHistoryCapacity);
    EXPECT_EQ(graphs.frames.front().output_id, 3u);
    EXPECT_EQ(graphs.frames.back().output_id, performance::kPerfHistoryCapacity + 2u);

    history.BuildFrameGraphs(performance::kPerfHistoryCapacity + 2, 2, graphs);
    ASSERT_EQ(graphs.count, 2u);
    EXPECT_EQ(graphs.frames[0].output_id, performance::kPerfHistoryCapacity + 1u);
    EXPECT_EQ(graphs.frames[1].output_id, performance::kPerfHistoryCapacity + 2u);
}

TEST(FrameHistoryTest, FrameGraphSummariesUseOnlyVisibleMeasuredSlotsAndIncludeMeasuredGpuZero)
{
    performance::PerfFrameHistory history;
    performance::PerfFrameRecord output = Output(1, 100, 100);
    output.frame_id = 1;
    output.values[uint32_t(performance::PerfSeries::RenderGpu)] = {10000, ncl_perf::PerfSampleState::Measured};
    history.Append(output);
    performance::PerfFrameRecord host = output;
    host.kind = performance::PerfRecordKind::Host;
    host.values[uint32_t(performance::PerfSeries::HostCpu)] = {50000, ncl_perf::PerfSampleState::Measured};
    history.Append(host);

    const std::array<ncl_perf::PerfSampleState, 7> states = {
        ncl_perf::PerfSampleState::Measured,
        ncl_perf::PerfSampleState::Pending,
        ncl_perf::PerfSampleState::Incomplete,
        ncl_perf::PerfSampleState::Dropped,
        ncl_perf::PerfSampleState::Inactive,
        ncl_perf::PerfSampleState::Unavailable,
        ncl_perf::PerfSampleState::Measured
    };
    for (uint32_t index = 0; index < states.size(); ++index)
    {
        output = Output(index + 2, (index + 2) * 100, index == 6 ? 0 : 100);
        output.frame_id = index + 2;
        output.values[uint32_t(performance::PerfSeries::RenderGpu)] = {index == 0 ? 0u : index == 6 ? 60u : 9000u, states[index]};
        history.Append(output);
        host = output;
        host.kind = performance::PerfRecordKind::Host;
        host.values[uint32_t(performance::PerfSeries::HostCpu)] = {index == 0 ? 200u : index == 6 ? 600u : 9000u, states[index]};
        history.Append(host);
    }
    history.Append(Output(9, 900, 9000));

    performance::PerfFrameGraphs graphs;
    history.BuildFrameGraphs(800, 7, graphs);
    ASSERT_EQ(graphs.count, 7u);
    EXPECT_EQ(graphs.frames.front().output_id, 2u);
    EXPECT_EQ(graphs.frames.back().output_id, 8u);
    const performance::PerfSummary& cpu = graphs.summaries[uint32_t(performance::PerfSeries::HostCpu)];
    EXPECT_EQ(cpu.count, 2u);
    EXPECT_EQ(cpu.mean_ns, 400u);
    EXPECT_EQ(cpu.median_ns, 400u);
    EXPECT_EQ(cpu.maximum_ns, 600u);
    EXPECT_EQ(cpu.state, ncl_perf::PerfSampleState::Measured);
    const performance::PerfSummary& gpu = graphs.summaries[uint32_t(performance::PerfSeries::RenderGpu)];
    EXPECT_EQ(gpu.count, 2u);
    EXPECT_EQ(gpu.mean_ns, 30u);
    EXPECT_EQ(gpu.median_ns, 30u);
    EXPECT_EQ(gpu.maximum_ns, 60u);
    EXPECT_EQ(gpu.state, ncl_perf::PerfSampleState::Measured);
    EXPECT_FALSE(gpu.percentiles_available);
    const performance::PerfSummary& interval = graphs.summaries[uint32_t(performance::PerfSeries::FrameInterval)];
    EXPECT_EQ(interval.count, 6u);
    EXPECT_EQ(interval.mean_ns, 100u);
    EXPECT_DOUBLE_EQ(interval.fps, 10000000.0);
    EXPECT_EQ(graphs.summaries[uint32_t(performance::PerfSeries::StudioGpu)].count, 0u);
}

TEST(FrameHistoryTest, FrameGraphSummariesRecalculatePercentileAvailabilityWhenTheColumnWindowChanges)
{
    performance::PerfFrameHistory history;
    for (uint64_t id = 1; id <= 101; ++id)
    {
        performance::PerfFrameRecord output = Output(id, id * 1000, 1000);
        output.values[uint32_t(performance::PerfSeries::RenderGpu)] = {id * 100, ncl_perf::PerfSampleState::Measured};
        if (id == 101)
        {
            output.values[uint32_t(performance::PerfSeries::RenderGpu)].state = ncl_perf::PerfSampleState::Pending;
        }
        history.Append(output);
    }

    performance::PerfFrameGraphs graphs;
    history.BuildFrameGraphs(101000, 101, graphs);
    const performance::PerfSummary& gpu = graphs.summaries[uint32_t(performance::PerfSeries::RenderGpu)];
    EXPECT_EQ(gpu.count, 100u);
    EXPECT_EQ(gpu.mean_ns, 5050u);
    EXPECT_EQ(gpu.median_ns, 5050u);
    EXPECT_EQ(gpu.maximum_ns, 10000u);
    EXPECT_TRUE(gpu.percentiles_available);
    EXPECT_EQ(gpu.p95_ns, 9500u);
    EXPECT_EQ(gpu.p99_ns, 9900u);

    history.BuildFrameGraphs(101000, 100, graphs);
    EXPECT_EQ(gpu.count, 99u);
    EXPECT_EQ(gpu.mean_ns, 5100u);
    EXPECT_EQ(gpu.median_ns, 5100u);
    EXPECT_FALSE(gpu.percentiles_available);
    EXPECT_EQ(gpu.p99_ns, 0u);
    history.BuildFrameGraphs(101000, 2, graphs);
    EXPECT_EQ(gpu.count, 1u);
    EXPECT_EQ(gpu.mean_ns, 10000u);
    EXPECT_EQ(gpu.maximum_ns, 10000u);
    history.BuildFrameGraphs(101000, 0, graphs);
    EXPECT_EQ(gpu.count, 0u);
    EXPECT_EQ(gpu.mean_ns, 0u);
    EXPECT_EQ(gpu.maximum_ns, 0u);
    EXPECT_EQ(gpu.state, ncl_perf::PerfSampleState::Unavailable);
    history.BuildFrameGraphs(101000, 101, graphs);
    EXPECT_EQ(gpu.count, 100u);
    EXPECT_TRUE(gpu.percentiles_available);
    EXPECT_EQ(gpu.p99_ns, 9900u);
}

TEST(FrameHistoryTest, FrameGraphsRetainOlderFramesWhilePublicSnapshotKeepsItsTenSecondWindow)
{
    performance::PerfFrameHistory history;
    performance::PerfFrameRecord output = Output(1, 1000000000, 1000000);
    output.frame_id = 1;
    output.values[uint32_t(performance::PerfSeries::RenderGpu)] = {900, ncl_perf::PerfSampleState::Measured};
    history.Append(output);
    performance::PerfFrameRecord host = output;
    host.kind = performance::PerfRecordKind::Host;
    host.values[uint32_t(performance::PerfSeries::HostCpu)] = {1000, ncl_perf::PerfSampleState::Measured};
    history.Append(host);
    output = Output(2, 12000000000, 1000000);
    output.frame_id = 2;
    output.values[uint32_t(performance::PerfSeries::RenderGpu)] = {300, ncl_perf::PerfSampleState::Measured};
    history.Append(output);
    host = output;
    host.kind = performance::PerfRecordKind::Host;
    host.values[uint32_t(performance::PerfSeries::HostCpu)] = {400, ncl_perf::PerfSampleState::Measured};
    history.Append(host);

    performance::PerfFrameGraphs graphs;
    history.BuildFrameGraphs(12000000000, 3, graphs);
    ASSERT_EQ(graphs.count, 2u);
    EXPECT_EQ(graphs.frames[1].output_id, 1u);
    EXPECT_EQ(graphs.frames[2].output_id, 2u);
    EXPECT_EQ(graphs.summaries[uint32_t(performance::PerfSeries::HostCpu)].mean_ns, 700u);
    EXPECT_EQ(graphs.summaries[uint32_t(performance::PerfSeries::RenderGpu)].mean_ns, 600u);
    EXPECT_EQ(graphs.summaries[uint32_t(performance::PerfSeries::RenderGpu)].maximum_ns, 900u);
    EXPECT_EQ(history.CountOutputsAfter(0), 2u);
    EXPECT_EQ(history.CountOutputsAfter(1), 1u);
    EXPECT_DOUBLE_EQ(history.CalculateFps(12000000000, 500000000), 1000.0);

    performance::PerfSnapshot snapshot;
    history.BuildSnapshot(12000000000, snapshot);
    EXPECT_EQ(snapshot.record_count, 2u);
    EXPECT_EQ(snapshot.summaries[uint32_t(performance::PerfSeries::HostCpu)].count, 1u);
    EXPECT_EQ(snapshot.summaries[uint32_t(performance::PerfSeries::HostCpu)].mean_ns, 400u);
    EXPECT_EQ(snapshot.summaries[uint32_t(performance::PerfSeries::RenderGpu)].count, 1u);
    EXPECT_EQ(snapshot.summaries[uint32_t(performance::PerfSeries::RenderGpu)].mean_ns, 300u);
}

TEST(FrameHistoryTest, FrameGraphSummariesApplyDelayedGpuOnlyInsideTheSelectedColumnWindow)
{
    performance::PerfFrameHistory history;
    performance::PerfFrameRecord output = Output(1, 1000000000, 1000);
    output.frame_id = 10;
    output.values[uint32_t(performance::PerfSeries::RenderGpu)].state = ncl_perf::PerfSampleState::Pending;
    history.Append(output);
    output = Output(2, 12000000000, 1000);
    output.values[uint32_t(performance::PerfSeries::RenderGpu)] = {200, ncl_perf::PerfSampleState::Measured};
    history.Append(output);

    performance::PerfFrameGraphs graphs;
    history.BuildFrameGraphs(12000000000, 2, graphs);
    ASSERT_EQ(graphs.count, 2u);
    const performance::PerfSummary& gpu = graphs.summaries[uint32_t(performance::PerfSeries::RenderGpu)];
    EXPECT_EQ(gpu.count, 1u);
    EXPECT_EQ(gpu.mean_ns, 200u);
    performance::PerfGpuResult result{10, 1, 0, 1, 2, 1000, performance::PerfSeries::RenderGpu, ncl_perf::PerfSampleState::Measured};
    ASSERT_TRUE(history.ApplyGpu(result));
    history.BuildFrameGraphs(12000000000, 2, graphs);
    EXPECT_EQ(graphs.frames[0].output_id, 1u);
    EXPECT_EQ(graphs.frames[0].values[uint32_t(performance::PerfSeries::RenderGpu)].duration_ns, 1000u);
    EXPECT_EQ(gpu.count, 2u);
    EXPECT_EQ(gpu.mean_ns, 600u);
    EXPECT_EQ(gpu.maximum_ns, 1000u);

    history.BuildFrameGraphs(12000000000, 1, graphs);
    EXPECT_EQ(gpu.count, 1u);
    EXPECT_EQ(gpu.mean_ns, 200u);
    result.duration_ns = 200;
    ASSERT_TRUE(history.ApplyGpu(result));
    history.BuildFrameGraphs(12000000000, 1, graphs);
    EXPECT_EQ(gpu.mean_ns, 200u);
    history.BuildFrameGraphs(12000000000, 2, graphs);
    EXPECT_EQ(gpu.mean_ns, 700u);
    EXPECT_EQ(gpu.maximum_ns, 1200u);
    history.Reset(2);
    history.BuildFrameGraphs(12000000000, 2, graphs);
    EXPECT_EQ(gpu.count, 0u);
    EXPECT_EQ(gpu.mean_ns, 0u);
    EXPECT_EQ(gpu.maximum_ns, 0u);
}

TEST(FrameHistoryTest, CatalogSelectsEachSummaryAndGraphFromItsDeclaredRecordDomain)
{
    performance::PerfFrameHistory history;
    performance::PerfFrameRecord output = Output(1, 1000000, 1000);
    output.frame_id = 7;
    performance::PerfFrameRecord host = output;
    host.kind = performance::PerfRecordKind::Host;

    for (const performance::SeriesDescriptor& series : performance::kSeries)
    {
        output.values[uint32_t(series.id)] = {100 + uint32_t(series.id), ncl_perf::PerfSampleState::Measured};
        host.values[uint32_t(series.id)] = {200 + uint32_t(series.id), ncl_perf::PerfSampleState::Measured};
    }

    history.Append(output);
    history.Append(host);
    performance::PerfSnapshot snapshot;
    history.BuildSnapshot(1000000, snapshot);
    performance::PerfFrameGraphs graphs;
    history.BuildFrameGraphs(1000000, 1, graphs);
    ASSERT_EQ(graphs.count, 1u);

    for (const performance::SeriesDescriptor& series : performance::kSeries)
    {
        SCOPED_TRACE(series.name);
        uint32_t index = uint32_t(series.id);
        ncl_perf::PerfDuration_t expected = (series.domain == performance::PerfRecordKind::Host ? 200u : 100u) + index;
        EXPECT_EQ(snapshot.summaries[index].count, 1u);
        EXPECT_EQ(snapshot.summaries[index].mean_ns, expected);
        EXPECT_EQ(graphs.frames[0].values[index].duration_ns, expected);
        EXPECT_EQ(graphs.summaries[index].mean_ns, expected);
    }
}

TEST(FrameHistoryTest, CatalogDomainsKeepMissingStatesAndZeroIntervalsOutOfSummaries)
{
    for (const performance::SeriesDescriptor& series : performance::kSeries)
    {
        SCOPED_TRACE(series.name);
        performance::PerfFrameHistory history;
        performance::PerfFrameRecord record = Output(1, 1000000, 0);
        record.kind = series.domain;
        record.values[uint32_t(series.id)] = {999, ncl_perf::PerfSampleState::Pending};
        history.Append(record);
        performance::PerfSnapshot snapshot;
        history.BuildSnapshot(1000000, snapshot);
        EXPECT_EQ(snapshot.summaries[uint32_t(series.id)].count, 0u);
        EXPECT_EQ(snapshot.summaries[uint32_t(series.id)].state, ncl_perf::PerfSampleState::Pending);

        record.completed_at += 1000;
        record.values[uint32_t(series.id)] = {0, ncl_perf::PerfSampleState::Measured};
        history.Append(record);
        history.BuildSnapshot(record.completed_at, snapshot);
        EXPECT_EQ(snapshot.summaries[uint32_t(series.id)].count, series.id == performance::PerfSeries::FrameInterval ? 0u : 1u);
    }
}

TEST(FrameHistoryTest, InvalidGpuSeriesCannotIndexHistoryOrUpdateCpuSeries)
{
    performance::PerfFrameHistory history;
    performance::PerfFrameRecord record = Output(1, 1000, 1000);
    record.frame_id = 10;
    history.Append(record);
    performance::PerfGpuResult result{10, 1, 0, 1, 2, 500, performance::PerfSeries(UINT32_MAX), ncl_perf::PerfSampleState::Measured};
    EXPECT_FALSE(history.ApplyGpu(result));
    result.series = performance::PerfSeries::HostCpu;
    EXPECT_FALSE(history.ApplyGpu(result));
    result.series = performance::PerfSeries::Count;
    EXPECT_TRUE(history.ApplyGpu(result));
}

TEST(FrameHistoryTest, AcceptedFrameCountersSumWithoutAccumulatingMonitorSnapshots)
{
    performance::PerfFrameHistory history;
    performance::PerfFrameRecord record = Output(1, 1000, 1000);
    record.kind = performance::PerfRecordKind::Host;
    record.counters[uint32_t(performance::PerfCounterId::StudioPackets)] = 3;
    record.counters[uint32_t(performance::PerfCounterId::CpuDroppedScopes)] = 1;
    record.counters[uint32_t(performance::PerfCounterId::GpuDroppedScopes)] = 10;
    record.counters[uint32_t(performance::PerfCounterId::GpuExpiredResults)] = 20;
    history.Append(record);
    record.completed_at = 2000;
    history.Append(record);
    performance::PerfSnapshot snapshot;
    history.BuildSnapshot(2000, snapshot);
    EXPECT_EQ(snapshot.counters[uint32_t(performance::PerfCounterId::StudioPackets)], 6u);
    EXPECT_EQ(snapshot.counters[uint32_t(performance::PerfCounterId::CpuDroppedScopes)], 2u);
    EXPECT_EQ(snapshot.counters[uint32_t(performance::PerfCounterId::GpuDroppedScopes)], 0u);
    EXPECT_EQ(snapshot.counters[uint32_t(performance::PerfCounterId::GpuExpiredResults)], 0u);
}

TEST(FrameHistoryTest, GaugeSummariesUseMeasuredHostRecordsAndLatestHostState)
{
    performance::PerfFrameHistory history;
    for (uint32_t frame = 1; frame <= 4; ++frame)
    {
        performance::PerfFrameRecord host;
        host.kind = performance::PerfRecordKind::Host;
        host.frame_id = frame;
        host.collector_generation = 1;
        host.completed_at = frame * 100;
        host.gauges[0] =
            frame < 4 ? performance::PerfScalarValue{double(frame * 2), ncl_perf::PerfSampleState::Measured}
                                   : performance::PerfScalarValue{0, ncl_perf::PerfSampleState::Inactive};
        history.Append(host);
    }
    performance::PerfFrameRecord output = Output(8, 500, 100);
    output.gauges[0] = {1000, ncl_perf::PerfSampleState::Measured};
    history.Append(output);
    performance::PerfSnapshot snapshot;
    history.BuildSnapshot(500, snapshot);
    const performance::PerfScalarSummary& summary = snapshot.gauge_summaries[0];
    EXPECT_EQ(summary.state, ncl_perf::PerfSampleState::Measured);
    EXPECT_EQ(summary.count, 3u);
    EXPECT_DOUBLE_EQ(summary.last, 6);
    EXPECT_DOUBLE_EQ(summary.mean, 4);
    EXPECT_DOUBLE_EQ(summary.maximum, 6);
    EXPECT_EQ(snapshot.gauges[0].state, ncl_perf::PerfSampleState::Inactive);
}

TEST(FrameHistoryTest, GaugeSummaryIncludesMeasuredZeroAndNegativeMaximum)
{
    performance::PerfFrameHistory history;
    performance::PerfFrameRecord host;
    host.kind = performance::PerfRecordKind::Host;
    host.collector_generation = 1;
    host.completed_at = 100;
    host.gauges[0] = {-7, ncl_perf::PerfSampleState::Measured};
    history.Append(host);
    performance::PerfSnapshot snapshot;
    history.BuildSnapshot(100, snapshot);
    EXPECT_DOUBLE_EQ(snapshot.gauge_summaries[0].maximum, -7);
    host.completed_at = 200;
    host.gauges[0] = {0, ncl_perf::PerfSampleState::Measured};
    history.Append(host);
    history.BuildSnapshot(200, snapshot);
    EXPECT_EQ(snapshot.gauge_summaries[0].count, 2u);
    EXPECT_DOUBLE_EQ(snapshot.gauge_summaries[0].last, 0);
    EXPECT_DOUBLE_EQ(snapshot.gauge_summaries[0].mean, -3.5);
    EXPECT_DOUBLE_EQ(snapshot.gauge_summaries[0].maximum, 0);
}
