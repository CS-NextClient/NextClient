#include <array>

#include <gtest/gtest.h>

#include "performance/PerformanceSnapshotPublisher.h"
#include "performance/performance_stages.h"

namespace
{
    class PerfSnapshotPublisherTest : public testing::Test
    {
    protected:
        performance::PerfFrameHistory history;
        performance::PerfSnapshotPublisher publisher;
        performance::PerfSnapshot snapshot;

        void Complete(performance::PerfFrameId_t frame_id, ncl_perf::PerfTimestamp_t completed_at, ncl_perf::PerfDuration_t duration)
        {
            std::array<ncl_perf::PerfCpuSample, 1> samples{};
            samples[0].scope_id = performance::PerfStage::Studio;
            samples[0].inclusive_ns = duration;
            samples[0].self_ns = duration;
            samples[0].calls = 1;
            samples[0].state = ncl_perf::PerfSampleState::Measured;
            publisher.CompleteCpuFrame(frame_id, samples, completed_at);

            performance::PerfFrameRecord record;
            record.kind = performance::PerfRecordKind::Host;
            record.frame_id = frame_id;
            record.completed_at = completed_at;
            record.collector_generation = 1;
            record.values[uint32_t(performance::PerfSeries::HostCpu)] = {duration, ncl_perf::PerfSampleState::Measured};
            history.Append(record);
            publisher.Advance(completed_at, history);
        }
    };
} // namespace

TEST_F(PerfSnapshotPublisherTest, FirstCompletionPublishesWithoutAReader)
{
    publisher.Reset(1, 0);
    Complete(1, 100, 70);
    publisher.CopySnapshot(snapshot);

    EXPECT_EQ(snapshot.captured_at, 100u);
    EXPECT_EQ(snapshot.record_count, 1u);
    EXPECT_EQ(snapshot.cpu_frame_id, 1u);
    EXPECT_EQ(snapshot.cpu_frame_count, 1u);
    ASSERT_EQ(snapshot.cpu_count, 1u);
    EXPECT_EQ(snapshot.cpu_tree[0].inclusive_ns, 70u);
    EXPECT_EQ(snapshot.cpu_window_ns, 100u);
}

TEST_F(PerfSnapshotPublisherTest, NumericWindowPublishesAt250Milliseconds)
{
    publisher.Reset(1, 0);
    Complete(1, 0, 10);
    Complete(2, 100000000, 20);
    Complete(3, 249000000, 30);
    publisher.CopySnapshot(snapshot);
    EXPECT_EQ(snapshot.cpu_frame_id, 1u);
    EXPECT_EQ(snapshot.cpu_frame_count, 1u);
    EXPECT_EQ(snapshot.cpu_tree[0].inclusive_ns, 10u);

    publisher.Advance(250000000, history);
    publisher.CopySnapshot(snapshot);
    EXPECT_EQ(snapshot.cpu_frame_id, 3u);
    EXPECT_EQ(snapshot.cpu_frame_count, 2u);
    EXPECT_EQ(snapshot.cpu_tree[0].inclusive_ns, 50u);
    EXPECT_EQ(snapshot.cpu_tree[0].calls, 2u);
    EXPECT_EQ(snapshot.cpu_window_ns, 250000000u);
}

TEST_F(PerfSnapshotPublisherTest, HistoryInvalidationPreservesThePendingCpuWindow)
{
    publisher.Reset(1, 0);
    Complete(1, 0, 10);
    Complete(2, 100000000, 20);
    performance::PerfFrameRecord output;
    output.kind = performance::PerfRecordKind::Output;
    output.output_id = 1;
    output.collector_generation = 1;
    output.context_generation = 1;
    output.completed_at = 125000000;
    output.values[uint32_t(performance::PerfSeries::RenderGpu)].state = ncl_perf::PerfSampleState::Pending;
    history.Append(output);
    performance::PerfGpuResult result;
    result.collector_generation = 1;
    result.context_generation = 1;
    result.output_id = 1;
    result.series = performance::PerfSeries::RenderGpu;
    result.duration_ns = 77;
    result.state = ncl_perf::PerfSampleState::Measured;
    ASSERT_TRUE(history.ApplyGpu(result));
    publisher.InvalidateHistory();
    publisher.Advance(150000000, history);
    publisher.CopySnapshot(snapshot);
    EXPECT_EQ(snapshot.captured_at, 150000000u);
    EXPECT_EQ(snapshot.cpu_frame_id, 1u);
    EXPECT_EQ(snapshot.cpu_tree[0].inclusive_ns, 10u);
    EXPECT_EQ(snapshot.summaries[uint32_t(performance::PerfSeries::RenderGpu)].count, 1u);
    EXPECT_EQ(snapshot.summaries[uint32_t(performance::PerfSeries::RenderGpu)].mean_ns, 77u);

    Complete(3, 249000000, 30);
    publisher.Advance(250000000, history);
    publisher.CopySnapshot(snapshot);
    EXPECT_EQ(snapshot.cpu_frame_id, 3u);
    EXPECT_EQ(snapshot.cpu_frame_count, 2u);
    EXPECT_EQ(snapshot.cpu_tree[0].inclusive_ns, 50u);
    EXPECT_EQ(snapshot.cpu_window_ns, 250000000u);
}

TEST_F(PerfSnapshotPublisherTest, OrdinaryCompletionsPreserveTheHistoryRefreshCadence)
{
    publisher.Reset(1, 0);
    Complete(1, 0, 10);
    uint64_t revision = publisher.get_revision();
    Complete(2, 100000000, 20);
    Complete(3, 249000000, 30);
    publisher.CopySnapshot(snapshot);
    EXPECT_EQ(publisher.get_revision(), revision);
    EXPECT_EQ(snapshot.captured_at, 0u);
    EXPECT_EQ(snapshot.record_count, 1u);

    publisher.Advance(250000000, history);
    publisher.CopySnapshot(snapshot);
    EXPECT_GT(publisher.get_revision(), revision);
    EXPECT_EQ(snapshot.captured_at, 250000000u);
    EXPECT_EQ(snapshot.record_count, 3u);
}

TEST_F(PerfSnapshotPublisherTest, EmptyPresentationDoesNotDelayTheFirstCompletedCollection)
{
    publisher.Reset(1, 0);
    publisher.Advance(0, history);
    Complete(1, 100, 70);
    publisher.CopySnapshot(snapshot);
    EXPECT_EQ(snapshot.captured_at, 100u);
    EXPECT_EQ(snapshot.record_count, 1u);
    EXPECT_EQ(snapshot.cpu_frame_count, 1u);
}

TEST_F(PerfSnapshotPublisherTest, CopyDoesNotConsumePendingOrPublishedMeasurements)
{
    publisher.Reset(1, 0);
    Complete(1, 0, 10);
    Complete(2, 100000000, 20);
    for (uint32_t index = 0; index < 3; ++index)
    {
        publisher.CopySnapshot(snapshot);
        EXPECT_EQ(snapshot.cpu_frame_id, 1u);
        EXPECT_EQ(snapshot.cpu_frame_count, 1u);
        EXPECT_EQ(snapshot.cpu_tree[0].inclusive_ns, 10u);
    }

    publisher.Advance(250000000, history);
    publisher.CopySnapshot(snapshot);
    EXPECT_EQ(snapshot.cpu_frame_id, 2u);
    EXPECT_EQ(snapshot.cpu_frame_count, 1u);
    EXPECT_EQ(snapshot.cpu_tree[0].inclusive_ns, 20u);
}

TEST_F(PerfSnapshotPublisherTest, IdleAdvanceExpiresHistoryAndPanelFps)
{
    publisher.Reset(1, 0);
    performance::PerfFrameRecord output;
    output.kind = performance::PerfRecordKind::Output;
    output.output_id = 1;
    output.collector_generation = 1;
    output.completed_at = 100000000;
    output.values[uint32_t(performance::PerfSeries::FrameInterval)] = {10000000, ncl_perf::PerfSampleState::Measured};
    history.Append(output);
    publisher.Advance(100000000, history);
    EXPECT_DOUBLE_EQ(publisher.get_panel_fps(), 100.0);

    publisher.Advance(600000000, history);
    EXPECT_DOUBLE_EQ(publisher.get_panel_fps(), 0.0);
    publisher.Advance(12000000000, history);
    publisher.CopySnapshot(snapshot);
    EXPECT_EQ(snapshot.record_count, 0u);
}

TEST_F(PerfSnapshotPublisherTest, ResetRejectsPendingMeasurementsFromThePreviousWindow)
{
    publisher.Reset(1, 0);
    Complete(1, 0, 10);
    Complete(2, 100000000, 20);
    history.Reset(2);
    publisher.Reset(2, 150000000);
    publisher.Advance(250000000, history);
    publisher.CopySnapshot(snapshot);
    EXPECT_EQ(snapshot.collector_generation, 2u);
    EXPECT_EQ(snapshot.record_count, 0u);
    EXPECT_EQ(snapshot.cpu_count, 0u);
    EXPECT_EQ(snapshot.cpu_frame_count, 0u);
}

TEST_F(PerfSnapshotPublisherTest, PanelResizeUsesPublishedObservationWithoutChangingDiagnosticSummary)
{
    publisher.Reset(1, 0);
    for (uint32_t index = 1; index <= 4; ++index)
    {
        performance::PerfFrameRecord output;
        output.kind = performance::PerfRecordKind::Output;
        output.output_id = index;
        output.collector_generation = 1;
        output.completed_at = index * 100;
        output.values[uint32_t(performance::PerfSeries::FrameInterval)] = {index * 10, ncl_perf::PerfSampleState::Measured};
        history.Append(output);
    }

    publisher.Advance(400, history);
    performance::PerfPanelProjection panel;
    performance::PerfFrameGraphs frames;
    publisher.CopySnapshot(snapshot);
    panel.Project(snapshot, history, publisher.get_revision(), publisher.get_panel_fps(), &frames, 2);
    EXPECT_EQ(snapshot.summaries[uint32_t(performance::PerfSeries::FrameInterval)].count, 2u);
    EXPECT_EQ(snapshot.summaries[uint32_t(performance::PerfSeries::FrameInterval)].mean_ns, 35u);
    EXPECT_EQ(frames.captured_at, 400u);
    EXPECT_EQ(frames.frames.front().output_id, 3u);

    publisher.CopySnapshot(snapshot);
    panel.Project(snapshot, history, publisher.get_revision(), publisher.get_panel_fps(), &frames, 3);
    EXPECT_EQ(snapshot.summaries[uint32_t(performance::PerfSeries::FrameInterval)].count, 3u);
    EXPECT_EQ(snapshot.summaries[uint32_t(performance::PerfSeries::FrameInterval)].mean_ns, 30u);
    EXPECT_EQ(frames.captured_at, 400u);
    EXPECT_EQ(frames.frames.front().output_id, 2u);

    publisher.CopySnapshot(snapshot);
    EXPECT_EQ(snapshot.summaries[uint32_t(performance::PerfSeries::FrameInterval)].count, 4u);
    EXPECT_EQ(snapshot.summaries[uint32_t(performance::PerfSeries::FrameInterval)].mean_ns, 25u);
}

TEST_F(PerfSnapshotPublisherTest, PanelReprojectsGpuChangesAtTheSameObservationTime)
{
    publisher.Reset(1, 0);
    performance::PerfFrameRecord output;
    output.kind = performance::PerfRecordKind::Output;
    output.output_id = 1;
    output.collector_generation = 1;
    output.context_generation = 1;
    output.completed_at = 100;
    output.values[uint32_t(performance::PerfSeries::RenderGpu)].state = ncl_perf::PerfSampleState::Pending;
    history.Append(output);
    publisher.Advance(100, history);
    performance::PerfPanelProjection panel;
    publisher.CopySnapshot(snapshot);
    panel.Project(snapshot, history, publisher.get_revision(), publisher.get_panel_fps(), nullptr, 1);
    EXPECT_EQ(snapshot.summaries[uint32_t(performance::PerfSeries::RenderGpu)].state, ncl_perf::PerfSampleState::Pending);

    performance::PerfGpuResult result;
    result.collector_generation = 1;
    result.context_generation = 1;
    result.output_id = 1;
    result.series = performance::PerfSeries::RenderGpu;
    result.duration_ns = 77;
    result.state = ncl_perf::PerfSampleState::Measured;
    ASSERT_TRUE(history.ApplyGpu(result));
    publisher.InvalidateHistory();
    publisher.Advance(100, history);
    publisher.CopySnapshot(snapshot);
    panel.Project(snapshot, history, publisher.get_revision(), publisher.get_panel_fps(), nullptr, 1);
    EXPECT_EQ(snapshot.captured_at, 100u);
    EXPECT_EQ(snapshot.summaries[uint32_t(performance::PerfSeries::RenderGpu)].state, ncl_perf::PerfSampleState::Measured);
    EXPECT_EQ(snapshot.summaries[uint32_t(performance::PerfSeries::RenderGpu)].mean_ns, 77u);
}
