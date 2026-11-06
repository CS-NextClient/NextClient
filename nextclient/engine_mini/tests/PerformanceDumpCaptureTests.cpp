#include <array>
#include <cstring>
#include <memory>

#include <gtest/gtest.h>

#include "performance/PerformanceDumpCapture.h"

namespace
{
    std::array<ncl_perf::PerfCpuSample, ncl_perf::kPerfScopeCapacity> MakeTree()
    {
        std::array<ncl_perf::PerfCpuSample, ncl_perf::kPerfScopeCapacity> samples{};
        samples[0].scope_id = 30;
        samples[0].inclusive_ns = 100;
        samples[0].self_ns = 40;
        samples[0].calls = 2;
        samples[0].state = ncl_perf::PerfSampleState::Incomplete;
        std::strcpy(samples[0].name, "Studio");
        samples.back().scope_id = 127;

        return samples;
    }
} // namespace

TEST(PerformanceDumpCapture, CompletesExactCpuFrameAndDeliversOnceWithoutClearingTheSnapshot)
{
    performance::PerfDumpCapture capture;
    ASSERT_TRUE(capture.Request());
    capture.get_capture_buffer().mode = 3;
    ASSERT_TRUE(capture.ActivatePending());
    capture.SetStartedAt(100);
    performance::PerfSnapshot& buffer = capture.get_capture_buffer();
    buffer.cpu_frame_count = 20;
    buffer.cpu_frame_id = 10;
    buffer.cpu_window_ns = 999;
    buffer.gl_renderer[0] = 'X';
    std::array<ncl_perf::PerfCpuSample, ncl_perf::kPerfScopeCapacity> samples = MakeTree();
    capture.Complete(450, 42, samples, 1);

    EXPECT_FALSE(capture.is_active());
    EXPECT_TRUE(capture.is_ready());
    std::unique_ptr<performance::PerfSnapshot> output = std::make_unique<performance::PerfSnapshot>();
    ASSERT_TRUE(capture.Take(*output));
    EXPECT_FALSE(capture.Take(*output));
    EXPECT_FALSE(capture.is_ready());
    EXPECT_EQ(output->mode, 3);
    EXPECT_EQ(output->gl_renderer[0], 'X');
    EXPECT_EQ(output->cpu_frame_id, 42u);
    EXPECT_EQ(output->cpu_frame_count, 1u);
    EXPECT_EQ(output->cpu_window_ns, 350u);
    ASSERT_EQ(output->cpu_count, 1u);
    EXPECT_EQ(output->cpu_tree[0].inclusive_ns, 100u);
    EXPECT_EQ(output->cpu_tree[0].self_ns, 40u);
    EXPECT_EQ(output->cpu_tree[0].calls, 2u);
    EXPECT_EQ(output->cpu_tree[0].state, ncl_perf::PerfSampleState::Incomplete);
    EXPECT_STREQ(output->cpu_tree[0].name, "Studio");
    EXPECT_EQ(output->cpu_tree.back().scope_id, 127u);
    ASSERT_NE(capture.get_snapshot(), nullptr);
    EXPECT_EQ(capture.get_snapshot()->cpu_frame_id, 42u);
}

TEST(PerformanceDumpCapture, RejectsDuplicateRequestsUntilTheReadyCaptureIsTaken)
{
    performance::PerfDumpCapture capture;
    ASSERT_TRUE(capture.Request());
    EXPECT_FALSE(capture.Request());
    ASSERT_TRUE(capture.ActivatePending());
    EXPECT_FALSE(capture.ActivatePending());
    EXPECT_FALSE(capture.Request());
    capture.SetStartedAt(100);
    capture.Complete(200, 1, MakeTree(), 1);
    EXPECT_FALSE(capture.Request());
    std::unique_ptr<performance::PerfSnapshot> output = std::make_unique<performance::PerfSnapshot>();
    ASSERT_TRUE(capture.Take(*output));
    EXPECT_TRUE(capture.Request());
}

TEST(PerformanceDumpCapture, RetainsConsumedSnapshotDuringTheNextPendingAndRetriedCapture)
{
    performance::PerfDumpCapture capture;
    ASSERT_TRUE(capture.Request());
    ASSERT_TRUE(capture.ActivatePending());
    capture.SetStartedAt(100);
    capture.Complete(200, 1, MakeTree(), 1);
    std::unique_ptr<performance::PerfSnapshot> output = std::make_unique<performance::PerfSnapshot>();
    ASSERT_TRUE(capture.Take(*output));
    const performance::PerfSnapshot* frozen = capture.get_snapshot();
    capture.RetryActive();
    ASSERT_TRUE(capture.Request());
    EXPECT_EQ(capture.get_snapshot(), frozen);
    EXPECT_EQ(capture.get_snapshot()->cpu_frame_id, 1u);
    capture.RetryActive();
    ASSERT_TRUE(capture.ActivatePending());
    capture.SetStartedAt(300);
    capture.RetryActive();
    EXPECT_FALSE(capture.is_active());
    EXPECT_FALSE(capture.is_ready());
    EXPECT_EQ(capture.get_snapshot(), frozen);
    ASSERT_TRUE(capture.ActivatePending());
    capture.SetStartedAt(500);
    capture.Complete(700, 2, MakeTree(), 1);
    EXPECT_EQ(capture.get_snapshot()->cpu_frame_id, 2u);
    EXPECT_EQ(capture.get_snapshot()->cpu_window_ns, 200u);
}

TEST(PerformanceDumpCapture, RetryPreservesPendingAndReadyStates)
{
    performance::PerfDumpCapture capture;
    capture.RetryActive();
    EXPECT_FALSE(capture.ActivatePending());
    ASSERT_TRUE(capture.Request());
    capture.RetryActive();
    ASSERT_TRUE(capture.ActivatePending());
    capture.SetStartedAt(10);
    capture.Complete(30, 1, MakeTree(), 1);
    capture.RetryActive();
    EXPECT_TRUE(capture.is_ready());
    EXPECT_FALSE(capture.ActivatePending());
    EXPECT_EQ(capture.get_snapshot()->cpu_frame_id, 1u);
}

TEST(PerformanceDumpCapture, CancelClearsPendingActiveReadyAndConsumedCaptures)
{
    for (uint32_t phase = 0; phase < 4; ++phase)
    {
        performance::PerfDumpCapture capture;
        ASSERT_TRUE(capture.Request());

        if (phase >= 1)
        {
            ASSERT_TRUE(capture.ActivatePending());
            capture.SetStartedAt(10);
        }

        if (phase >= 2)
        {
            capture.Complete(30, 1, MakeTree(), 1);
        }

        std::unique_ptr<performance::PerfSnapshot> output = std::make_unique<performance::PerfSnapshot>();

        if (phase == 3)
        {
            ASSERT_TRUE(capture.Take(*output));
        }

        capture.Cancel();
        EXPECT_FALSE(capture.is_active());
        EXPECT_FALSE(capture.is_ready());
        EXPECT_FALSE(capture.Take(*output));
        EXPECT_FALSE(capture.ActivatePending());
        EXPECT_EQ(capture.get_snapshot(), nullptr);
        EXPECT_TRUE(capture.Request());
    }
}
