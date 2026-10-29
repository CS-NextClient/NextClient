#include <array>
#include <cstring>

#include <gtest/gtest.h>

#include "performance/PerformanceBatchWindow.h"

namespace
{
    performance::PerfFrameRecord MakeRecord(
        performance::PerfFrameId_t frame_id,
        ncl_perf::PerfTimestamp_t completed_at,
        uint64_t packets = 0
    )
    {
        performance::PerfFrameRecord record;
        record.kind = performance::PerfRecordKind::Host;
        record.frame_id = frame_id;
        record.completed_at = completed_at;
        record.collector_generation = 7;
        record.counters[uint32_t(performance::PerfCounterId::StudioPackets)] = packets;

        return record;
    }

    ncl_perf::PerfCpuSample MakeSample(
        ncl_perf::PerfDuration_t duration,
        ncl_perf::PerfScopeId_t parent,
        ncl_perf::PerfSampleState state = ncl_perf::PerfSampleState::Measured
    )
    {
        ncl_perf::PerfCpuSample sample;
        sample.scope_id = 30;
        sample.parent_id = parent;
        sample.inclusive_ns = duration;
        sample.self_ns = duration / 2;
        sample.calls = 1;
        sample.state = state;
        std::strcpy(sample.name, "Studio");

        return sample;
    }
} // namespace

TEST(PerformanceBatchWindow, EvictsTheNinthFrameAndUsesOldestRetainedCompletion)
{
    performance::PerfBatchWindow window;
    ASSERT_TRUE(window.SetLevel(2));

    for (uint32_t frame = 1; frame <= 9; ++frame)
    {
        std::array<ncl_perf::PerfCpuSample, 1> samples = {MakeSample(frame * 10, 20)};
        window.BeginFrame();
        window.CompleteFrame(MakeRecord(frame, frame * 100, frame), samples);

        if (frame == 1)
        {
            EXPECT_EQ(window.get_snapshot()->cpu_window_ns, 0u);
        }
    }

    const performance::PerfSnapshot& snapshot = *window.get_snapshot();
    EXPECT_EQ(snapshot.collector_generation, 7u);
    EXPECT_EQ(snapshot.cpu_frame_id, 9u);
    EXPECT_EQ(snapshot.cpu_frame_count, 8u);
    EXPECT_EQ(snapshot.cpu_window_ns, 700u);
    EXPECT_EQ(snapshot.coverage_ns, 700u);
    EXPECT_EQ(snapshot.counters[uint32_t(performance::PerfCounterId::StudioPackets)], 44u);
    ASSERT_EQ(snapshot.cpu_count, 1u);
    EXPECT_EQ(snapshot.cpu_tree[0].inclusive_ns, 440u);
    EXPECT_EQ(snapshot.cpu_tree[0].self_ns, 220u);
    EXPECT_EQ(snapshot.cpu_tree[0].calls, 8u);
    EXPECT_STREQ(snapshot.cpu_tree[0].name, "Studio");
}

TEST(PerformanceBatchWindow, BasicWindowIncludesEmptyFramesAndEvictsTheirPredecessors)
{
    performance::PerfBatchWindow window;
    window.SetLevel(1);

    for (uint32_t frame = 1; frame <= 9; ++frame)
    {
        window.BeginFrame();
        window.CompleteFrame(MakeRecord(frame, frame * 100, frame == 1 ? 80 : 0), {});
    }

    const performance::PerfSnapshot& snapshot = *window.get_snapshot();
    EXPECT_EQ(snapshot.cpu_frame_count, 8u);
    EXPECT_EQ(snapshot.cpu_count, 0u);
    EXPECT_EQ(snapshot.counters[uint32_t(performance::PerfCounterId::StudioPackets)], 0u);
    EXPECT_EQ(snapshot.cpu_window_ns, 700u);
}

TEST(PerformanceBatchWindow, IncompleteSampleRemainsIncompleteAcrossLaterFrames)
{
    performance::PerfBatchWindow window;
    window.SetLevel(2);
    std::array<ncl_perf::PerfCpuSample, 1> samples = {MakeSample(10, 20, ncl_perf::PerfSampleState::Incomplete)};
    window.BeginFrame();
    window.CompleteFrame(MakeRecord(1, 100), samples);
    samples[0] = MakeSample(30, 20);
    window.BeginFrame();
    window.CompleteFrame(MakeRecord(2, 200), samples);

    EXPECT_EQ(window.get_snapshot()->cpu_tree[0].state, ncl_perf::PerfSampleState::Incomplete);
    EXPECT_EQ(window.get_snapshot()->cpu_tree[0].inclusive_ns, 40u);
    EXPECT_EQ(window.get_snapshot()->cpu_tree[0].self_ns, 20u);
    EXPECT_EQ(window.get_snapshot()->cpu_tree[0].calls, 2u);

    for (uint32_t frame = 3; frame <= 9; ++frame)
    {
        window.BeginFrame();
        window.CompleteFrame(MakeRecord(frame, frame * 100), samples);
    }

    EXPECT_EQ(window.get_snapshot()->cpu_tree[0].state, ncl_perf::PerfSampleState::Measured);
    EXPECT_EQ(window.get_snapshot()->cpu_tree[0].calls, 8u);
}

TEST(PerformanceBatchWindow, ConflictingCalledParentsCollapseWhileInactiveParentsAreIgnored)
{
    performance::PerfBatchWindow window;
    window.SetLevel(2);
    std::array<ncl_perf::PerfCpuSample, 1> samples = {MakeSample(10, 20)};
    window.BeginFrame();
    window.CompleteFrame(MakeRecord(1, 100), samples);
    samples[0] = MakeSample(0, 60, ncl_perf::PerfSampleState::Inactive);
    samples[0].calls = 0;
    window.BeginFrame();
    window.CompleteFrame(MakeRecord(2, 200), samples);
    EXPECT_EQ(window.get_snapshot()->cpu_tree[0].parent_id, 20u);
    EXPECT_EQ(window.get_snapshot()->cpu_tree[0].calls, 1u);
    samples[0] = MakeSample(30, 40);
    window.BeginFrame();
    window.CompleteFrame(MakeRecord(3, 300), samples);

    const ncl_perf::PerfCpuSample& total = window.get_snapshot()->cpu_tree[0];
    EXPECT_EQ(total.parent_id, 0u);
    EXPECT_EQ(total.calls, 2u);
    EXPECT_EQ(total.inclusive_ns, 40u);
    EXPECT_EQ(total.state, ncl_perf::PerfSampleState::Measured);
}

TEST(PerformanceBatchWindow, ResetRejectsTheOpenFrameAndKeepsTheConfiguredLevel)
{
    performance::PerfBatchWindow window;
    window.SetLevel(1);
    window.BeginFrame();
    window.CompleteFrame(MakeRecord(1, 100, 3), {});
    window.BeginFrame();
    window.Reset();
    window.CompleteFrame(MakeRecord(2, 200, 7), {});

    EXPECT_EQ(window.get_level(), 1);
    ASSERT_NE(window.get_snapshot(), nullptr);
    EXPECT_EQ(window.get_snapshot()->cpu_frame_count, 0u);
    EXPECT_EQ(window.get_snapshot()->counters[uint32_t(performance::PerfCounterId::StudioPackets)], 0u);

    window.BeginFrame();
    window.CompleteFrame(MakeRecord(3, 300, 11), {});
    EXPECT_EQ(window.get_snapshot()->cpu_frame_count, 1u);
    EXPECT_EQ(window.get_snapshot()->counters[uint32_t(performance::PerfCounterId::StudioPackets)], 11u);
}

TEST(PerformanceBatchWindow, ReturningToTheOriginalLevelRejectsPartialMeasurement)
{
    performance::PerfBatchWindow window;
    window.SetLevel(2);
    window.BeginFrame();
    window.SetLevel(1);
    window.SetLevel(2);
    std::array<ncl_perf::PerfCpuSample, 1> samples = {MakeSample(10, 20)};
    window.CompleteFrame(MakeRecord(1, 100, 3), samples);
    EXPECT_EQ(window.get_snapshot()->cpu_frame_count, 0u);

    window.BeginFrame();
    EXPECT_FALSE(window.SetLevel(99));
    window.CompleteFrame(MakeRecord(2, 200, 7), samples);
    EXPECT_EQ(window.get_snapshot()->cpu_frame_count, 1u);
    EXPECT_EQ(window.get_snapshot()->cpu_tree[0].inclusive_ns, 10u);

    window.SetLevel(-1);
    EXPECT_EQ(window.get_snapshot(), nullptr);
}

TEST(PerformanceBatchWindow, GaugeEvictionKeepsLastMeasuredAndMaximumAcrossEightFrames)
{
    performance::PerfBatchWindow window;
    window.SetLevel(1);
    for (uint32_t frame = 1; frame <= 10; ++frame)
    {
        performance::PerfFrameRecord record = MakeRecord(frame, frame * 100);
        record.gauges[0] = frame < 10 ? performance::PerfScalarValue{frame == 1 ? 90.0 : double(frame), ncl_perf::PerfSampleState::Measured}
                                      : performance::PerfScalarValue{0, ncl_perf::PerfSampleState::Inactive};
        window.BeginFrame();
        window.CompleteFrame(record, {});
        if (frame == 9)
        {
            const performance::PerfScalarSummary& summary = window.get_snapshot()->gauge_summaries[0];
            EXPECT_EQ(summary.count, 8u);
            EXPECT_DOUBLE_EQ(summary.last, 9);
            EXPECT_DOUBLE_EQ(summary.mean, 5.5);
            EXPECT_DOUBLE_EQ(summary.maximum, 9);
        }
    }
    const performance::PerfSnapshot& snapshot = *window.get_snapshot();
    EXPECT_EQ(snapshot.gauges[0].state, ncl_perf::PerfSampleState::Inactive);
    EXPECT_EQ(snapshot.gauge_summaries[0].count, 7u);
    EXPECT_DOUBLE_EQ(snapshot.gauge_summaries[0].last, 9);
    EXPECT_DOUBLE_EQ(snapshot.gauge_summaries[0].mean, 6);
    EXPECT_DOUBLE_EQ(snapshot.gauge_summaries[0].maximum, 9);
}
TEST(PerformanceBatchWindow, GaugeResetClearsLatestObservationAndSummary)
{
    performance::PerfBatchWindow window;
    window.SetLevel(1);
    performance::PerfFrameRecord record = MakeRecord(1, 100);
    record.gauges[0] = {7, ncl_perf::PerfSampleState::Measured};
    window.BeginFrame();
    window.CompleteFrame(record, {});
    window.BeginFrame();
    window.Reset();
    window.CompleteFrame(record, {});
    EXPECT_EQ(window.get_snapshot()->gauges[0].state, ncl_perf::PerfSampleState::Inactive);
    EXPECT_EQ(window.get_snapshot()->gauge_summaries[0].count, 0u);
    EXPECT_EQ(window.get_snapshot()->gauge_summaries[0].state, ncl_perf::PerfSampleState::Inactive);
}
