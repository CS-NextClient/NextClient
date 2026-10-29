#include <gtest/gtest.h>

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <limits>
#include <string>
#include <vector>
#include <strtools.h>
#include <next_engine_mini/PerformanceMonitorInterface.h>

#include "performance/PerformancePanelLayout.h"
#include "performance/PerformanceMetrics.h"
#include "performance/PerformanceNetwork.h"
#include "performance/PerformanceMapper.h"

namespace
{
    class PerformancePanelTest : public testing::Test
    {
    protected:
        std::unique_ptr<performance::PerformancePanelLayout> layout = std::make_unique<performance::PerformancePanelLayout>();
        std::unique_ptr<performance::PerfSnapshot> snapshot = std::make_unique<performance::PerfSnapshot>();

        bool HasText(const char* needle)
        {
            for (const performance::PerfPanelCommand& command : layout->get_commands())
            {
                if (command.kind == performance::PerfPanelCommandKind::Text &&
                    std::string(layout->get_text(command)).find(needle) != std::string::npos)
                {
                    return true;
                }
            }
            return false;
        }

        const performance::PerfPanelCommand* FindText(const char* text, bool exact = true)
        {
            for (const performance::PerfPanelCommand& command : layout->get_commands())
            {
                if (command.kind == performance::PerfPanelCommandKind::Text)
                {
                    std::string value = layout->get_text(command);
                    if (exact ? value == text : value.find(text) != std::string::npos)
                    {
                        return &command;
                    }
                }
            }
            return nullptr;
        }

        bool RowContains(const char* name, const char* value)
        {
            const performance::PerfPanelCommand* row = FindText(name);
            if (!row)
            {
                return false;
            }

            for (const performance::PerfPanelCommand& command : layout->get_commands())
            {
                if (command.kind == performance::PerfPanelCommandKind::Text && command.y == row->y &&
                    std::string(layout->get_text(command)) == value)
                {
                    return true;
                }
            }
            return false;
        }
    };
} // namespace

TEST_F(PerformancePanelTest, DisabledModeProducesNoCommands)
{
    layout->Build(*snapshot, 1920, 1080, 0);
    EXPECT_TRUE(layout->get_commands().empty());
}

TEST_F(PerformancePanelTest, BatchingBlockIsHiddenByDefault)
{
    for (int mode : {1, 2, 3})
    {
        layout->Build(*snapshot, 1600, 1200, mode);
        EXPECT_FALSE(HasText("Studio batching"));
        EXPECT_FALSE(HasText("MultiDraw"));
        EXPECT_TRUE(HasText("FPS"));
    }
}

TEST_F(PerformancePanelTest, BatchingToggleReleasesSpaceInNarrowPanel)
{
    layout->Build(*snapshot, 640, 900, 1, nullptr, nullptr, 2);
    int batching_bottom = layout->get_commands().back().y + layout->get_commands().back().height;
    int performance_height = layout->get_commands()[0].height;
    EXPECT_TRUE(HasText("Model draws"));

    layout->Build(*snapshot, 640, 900, 1);
    EXPECT_EQ(layout->get_commands()[0].height, performance_height);
    EXPECT_LT(layout->get_commands().back().y + layout->get_commands().back().height, batching_bottom);
    EXPECT_FALSE(HasText("Model draws"));
    EXPECT_FALSE(HasText("MultiDraw"));
    EXPECT_TRUE(HasText("Host Frame"));
}

TEST_F(PerformancePanelTest, NumericModeKeepsMeanSummaryAndMonitorFooter)
{
    performance::PerfSummary& summary = snapshot->summaries[uint32_t(performance::PerfSeries::FrameInterval)];
    summary.count = 3;
    summary.state = ncl_perf::PerfSampleState::Measured;
    summary.mean_ns = 400000;
    summary.median_ns = 250000;
    summary.p99_ns = 250000;
    summary.percentiles_available = true;
    summary.maximum_ns = 750000;
    snapshot->summaries[uint32_t(performance::PerfSeries::MonitorCpu)].count = 1;
    snapshot->summaries[uint32_t(performance::PerfSeries::MonitorCpu)].state = ncl_perf::PerfSampleState::Measured;
    snapshot->summaries[uint32_t(performance::PerfSeries::MonitorCpu)].mean_ns = 200000;
    layout->Build(*snapshot, 1600, 1200, 1);

    EXPECT_TRUE(RowContains("Swap Interval", "0.40"));
    EXPECT_TRUE(HasText("Monitor 0.20 ms | GPU age"));
    EXPECT_FALSE(HasText("Frame timings"));
    EXPECT_NE(FindText("avg"), nullptr);
    EXPECT_EQ(FindText("median"), nullptr);
    EXPECT_NE(FindText("max"), nullptr);
    EXPECT_NE(FindText("p99"), nullptr);
}

TEST_F(PerformancePanelTest, BatchingShowsGroupingAndActualDrawCallSavings)
{
    snapshot->coverage_ns = 2000000000;
    snapshot->counters[uint32_t(performance::PerfCounterId::StudioPackets)] = 10;
    snapshot->counters[uint32_t(performance::PerfCounterId::StudioSharedPackets)] = 8;
    snapshot->counters[uint32_t(performance::PerfCounterId::StudioSinglePackets)] = 2;
    snapshot->counters[uint32_t(performance::PerfCounterId::StudioSharedRuns)] = 3;
    snapshot->counters[uint32_t(performance::PerfCounterId::StudioMultiDrawBatches)] = 5;
    snapshot->counters[uint32_t(performance::PerfCounterId::StudioBatchedMeshes)] = 15;
    snapshot->counters[uint32_t(performance::PerfCounterId::StudioSingleMeshes)] = 4;
    snapshot->counters[uint32_t(performance::PerfCounterId::StudioProgramBinds)] = 6;
    snapshot->counters[uint32_t(performance::PerfCounterId::StudioFlushBrush)] = 2;
    snapshot->counters[uint32_t(performance::PerfCounterId::StudioImmediateMutableSkin)] = 1;
    snapshot->counters[uint32_t(performance::PerfCounterId::StudioConstituentDraws)] = 19;
    snapshot->counters[uint32_t(performance::PerfCounterId::StudioDrawCalls)] = 9;
    for (int mode : {1, 2, 3})
    {
        layout->Build(*snapshot, 1600, 1200, mode, nullptr, nullptr, 2);
        EXPECT_TRUE(HasText("Studio batching | totals, last 0 frames"));
        EXPECT_TRUE(RowContains("Model draws", "8"));
        EXPECT_TRUE(RowContains("Model draws", "2"));
        EXPECT_TRUE(RowContains("Model draws", "80.0%"));
        EXPECT_TRUE(HasText("Total"));
        EXPECT_TRUE(RowContains("Model draws", "10"));
        EXPECT_TRUE(RowContains("MultiDraw meshes", "15"));
        EXPECT_TRUE(RowContains("MultiDraw meshes", "4"));
        EXPECT_TRUE(RowContains("MultiDraw meshes", "78.9%"));
        EXPECT_TRUE(RowContains("MultiDraw meshes", "19"));
        EXPECT_TRUE(HasText("Draw calls saved: 10 / 19 (52.6%)"));
        EXPECT_TRUE(HasText("Detailed counters: perf_dump"));
        EXPECT_FALSE(HasText("Why packets stay single"));
        EXPECT_FALSE(HasText("Why submission batches end"));
        ASSERT_NE(FindText("Studio batching", false), nullptr);
        EXPECT_LT(layout->get_commands().back().y - FindText("Studio batching", false)->y, 300);
    }
}

TEST_F(PerformancePanelTest, NarrowBatchingSummaryKeepsCountsInsideDrawableArea)
{
    snapshot->counters[uint32_t(performance::PerfCounterId::StudioSharedPackets)] = 8;
    snapshot->counters[uint32_t(performance::PerfCounterId::StudioPackets)] = 10;
    snapshot->counters[uint32_t(performance::PerfCounterId::StudioSinglePackets)] = 2;
    layout->Build(*snapshot, 640, 900, 1, nullptr, nullptr, 2);
    EXPECT_TRUE(RowContains("Model draws", "8"));
    EXPECT_TRUE(RowContains("Model draws", "2"));
    EXPECT_TRUE(RowContains("Model draws", "80.0%"));
    EXPECT_TRUE(HasText("CPU live: waiting for frame"));
    EXPECT_TRUE(HasText("Detailed counters: perf_dump"));
    for (const performance::PerfPanelCommand& command : layout->get_commands())
    {
        EXPECT_GE(command.x, 0);
        EXPECT_GE(command.y, 0);
        EXPECT_LE(command.x + command.width, 640);
        EXPECT_LE(command.y + command.height, 900);
    }
}

TEST_F(PerformancePanelTest, BatchingWithoutDrawsHasNoInventedShare)
{
    layout->Build(*snapshot, 1600, 1200, 1, nullptr, nullptr, 2);
    EXPECT_TRUE(RowContains("Model draws", "0"));
    EXPECT_TRUE(RowContains("Model draws", "-"));
    EXPECT_TRUE(RowContains("MultiDraw meshes", "-"));
    EXPECT_TRUE(HasText("Draw calls saved: -"));
    EXPECT_TRUE(RowContains("GL save", "-"));
    EXPECT_TRUE(RowContains("Bone upload", "-"));
    EXPECT_FALSE(HasText("nan"));
    EXPECT_FALSE(HasText("inf"));
}

TEST_F(PerformancePanelTest, NarrowBatchingTablePreservesAllOverviewSeries)
{
    for (uint32_t series = 0; series < uint32_t(performance::PerfSeries::MonitorCpu); ++series)
    {
        snapshot->graphs[series][100] = {1000000, 500000, 1};
    }
    layout->Build(*snapshot, 640, 480, 2, nullptr, nullptr, 2);
    uint32_t peaks = 0;
    for (const performance::PerfPanelCommand& command : layout->get_commands())
    {
        peaks += command.kind == performance::PerfPanelCommandKind::Peak;
        EXPECT_LE(command.y + command.height, 480);
    }
    EXPECT_EQ(peaks, 0u);
    EXPECT_NE(FindText("CPU (ms)"), nullptr);
    EXPECT_NE(FindText("GPU (ms)"), nullptr);
    EXPECT_TRUE(HasText("Detailed counters: perf_dump"));
    EXPECT_TRUE(HasText("Monitor"));
}

TEST_F(PerformancePanelTest, BasicBatchingShowsOnlyGroupingAndSavings)
{
    layout->Build(*snapshot, 1600, 1200, 2, nullptr, nullptr, 1);
    EXPECT_TRUE(HasText("Model draws"));
    EXPECT_TRUE(HasText("MultiDraw meshes"));
    EXPECT_TRUE(HasText("Draw calls saved"));
    EXPECT_FALSE(HasText("CPU operation"));
    EXPECT_FALSE(HasText("Top GL cost"));
    EXPECT_FALSE(HasText("Other Studio"));
    ASSERT_NE(FindText("Studio batching", false), nullptr);
    EXPECT_LT(layout->get_commands().back().y - FindText("Studio batching", false)->y, 130);
}

TEST_F(PerformancePanelTest, StudioCostsUseOperationNameOrderAndTheCpuWindow)
{
    snapshot->coverage_ns = 10000000000;
    snapshot->cpu_window_ns = 250000000;
    snapshot->cpu_frame_count = 10;
    snapshot->cpu_count = 6;
    const ncl_perf::PerfScopeId_t ids[] = {
        performance::PerfStage::StudioProgramBind,
        performance::PerfStage::StudioGlSave,
        performance::PerfStage::StudioGlRestore,
        performance::PerfStage::StudioBufferBind,
        performance::PerfStage::StudioBoneUpload,
        performance::PerfStage::Studio
    };
    const ncl_perf::PerfDuration_t durations[] = {200000, 1000000, 300000, 400000, 2000000, 5000000};
    const uint64_t calls[] = {2, 100, 10, 20, 1000, 1000};
    for (uint32_t index = 0; index < 6; ++index)
    {
        ncl_perf::PerfCpuSample& sample = snapshot->cpu_tree[index];
        sample.scope_id = ids[index];
        sample.inclusive_ns = durations[index];
        sample.self_ns = 100000;
        sample.calls = calls[index];
        sample.state = ncl_perf::PerfSampleState::Measured;
    }
    layout->Build(*snapshot, 1600, 1200, 2, nullptr, nullptr, 2, snapshot.get());
    EXPECT_TRUE(HasText("CPU live: averages, last 10 frames"));
    EXPECT_NE(FindText("Calls total"), nullptr);
    EXPECT_TRUE(RowContains("Bone upload", "0.200"));
    EXPECT_TRUE(RowContains("Bone upload", "2.000"));
    EXPECT_TRUE(RowContains("Bone upload", "1000"));
    EXPECT_TRUE(RowContains("Program bind", "100.000"));
    EXPECT_TRUE(HasText("Other Studio: 0.010 ms/frame"));
    const performance::PerfPanelCommand* bones = FindText("Bone upload");
    const performance::PerfPanelCommand* save = FindText("GL save");
    const performance::PerfPanelCommand* program = FindText("Program bind");
    ASSERT_NE(bones, nullptr);
    ASSERT_NE(save, nullptr);
    ASSERT_NE(program, nullptr);
    const performance::PerfPanelCommand* buffers = FindText("Buffer binds");
    const performance::PerfPanelCommand* restore = FindText("GL restore");
    ASSERT_NE(buffers, nullptr);
    ASSERT_NE(restore, nullptr);
    EXPECT_LT(bones->y, buffers->y);
    EXPECT_LT(buffers->y, restore->y);
    EXPECT_LT(restore->y, save->y);
    EXPECT_LT(save->y, program->y);

    const std::array<const char*, 5> names = {"Bone upload", "Buffer binds", "GL restore", "GL save", "Program bind"};
    std::array<int, 5> positions{};
    for (uint32_t index = 0; index < names.size(); ++index)
    {
        positions[index] = FindText(names[index])->y;
    }
    snapshot->cpu_tree[0].inclusive_ns = 3000000;
    snapshot->cpu_tree[1].state = ncl_perf::PerfSampleState::Unavailable;
    layout->RefreshBatching(*snapshot, snapshot.get());
    for (uint32_t index = 0; index < names.size(); ++index)
    {
        ASSERT_NE(FindText(names[index]), nullptr);
        EXPECT_EQ(FindText(names[index])->y, positions[index]);
    }
    snapshot->cpu_tree[0].inclusive_ns = 200000;
    snapshot->cpu_tree[1].state = ncl_perf::PerfSampleState::Measured;
    layout->Build(*snapshot, 640, 900, 1, nullptr, nullptr, 2, snapshot.get());
    EXPECT_TRUE(RowContains("Bone upload", "0.200"));
}

TEST_F(PerformancePanelTest, StudioCostsDistinguishMeasuredZeroFromUnavailable)
{
    snapshot->cpu_count = 2;
    snapshot->cpu_frame_count = 1;
    snapshot->cpu_tree[0].scope_id = performance::PerfStage::StudioGlSave;
    snapshot->cpu_tree[0].calls = 1;
    snapshot->cpu_tree[0].state = ncl_perf::PerfSampleState::Measured;
    snapshot->cpu_tree[1].scope_id = performance::PerfStage::StudioBoneUpload;
    snapshot->cpu_tree[1].calls = 10;
    snapshot->cpu_tree[1].inclusive_ns = 99999999;
    snapshot->cpu_tree[1].state = ncl_perf::PerfSampleState::Incomplete;
    layout->Build(*snapshot, 1600, 1200, 1, nullptr, nullptr, 2, snapshot.get());
    EXPECT_TRUE(RowContains("GL save", "0.000"));
    EXPECT_TRUE(RowContains("Bone upload", "-"));
    snapshot->cpu_frame_count = 0;
    layout->Build(*snapshot, 1600, 1200, 1, nullptr, nullptr, 2, snapshot.get());
    EXPECT_TRUE(RowContains("GL save", "-"));
}

TEST_F(PerformancePanelTest, StudioDetailKeepsBroadOtherTotalAndOneTopDetail)
{
    snapshot->cpu_frame_count = 10;
    snapshot->cpu_count = 5;
    const ncl_perf::PerfScopeId_t ids[] = {
        performance::PerfStage::Studio,
        performance::PerfStage::StudioDataPrepare,
        performance::PerfStage::StudioMeshSort,
        performance::PerfStage::StudioBatchStateSave,
        performance::PerfStage::StudioRestoreTextures
    };
    const ncl_perf::PerfDuration_t times[] = {1000000, 3000000, 2000000, 5000000, 99999999};
    for (uint32_t index = 0; index < 5; ++index)
    {
        ncl_perf::PerfCpuSample& sample = snapshot->cpu_tree[index];
        sample.scope_id = ids[index];
        sample.self_ns = times[index];
        sample.inclusive_ns = times[index];
        sample.calls = 10;
        sample.state = index == 4 ? ncl_perf::PerfSampleState::Incomplete : ncl_perf::PerfSampleState::Measured;
    }
    layout->Build(*snapshot, 1600, 1200, 2, nullptr, nullptr, 2, snapshot.get());
    EXPECT_TRUE(HasText("Other Studio: 0.600 ms/frame"));
    EXPECT_TRUE(HasText("Top detail: Batch state save 0.500 ms/frame"));
    EXPECT_EQ(FindText("Model data preparation"), nullptr);
    EXPECT_EQ(FindText("Mesh sorting"), nullptr);
    EXPECT_TRUE(HasText("Detailed counters: perf_dump"));
    ASSERT_NE(FindText("Studio batching", false), nullptr);
    EXPECT_LT(layout->get_commands().back().y - FindText("Studio batching", false)->y, 270);
    layout->Build(*snapshot, 640, 480, 2, nullptr, nullptr, 2, snapshot.get());
    EXPECT_TRUE(HasText("Top detail: Batch state save 0.500 ms/frame"));
    EXPECT_TRUE(HasText("Detailed counters: perf_dump"));
    layout->Build(*snapshot, 1600, 1200, 2, nullptr, nullptr, 1);
    EXPECT_FALSE(HasText("Top detail"));
    EXPECT_FALSE(HasText("Other Studio"));
}

TEST_F(PerformancePanelTest, ShortWideBatchingShowsCompactTableAndDumpHint)
{
    snapshot->panel_scale = 1;
    for (int mode : {1, 2, 3})
    {
        layout->Build(*snapshot, 1280, 240, mode, nullptr, nullptr, 2);
        EXPECT_TRUE(HasText("Model draws"));
        EXPECT_TRUE(HasText("MultiDraw meshes"));
        EXPECT_TRUE(HasText("Detailed counters: perf_dump"));
        EXPECT_FALSE(HasText("Why submission batches end"));
        for (const performance::PerfPanelCommand& command : layout->get_commands())
        {
            EXPECT_LE(command.x + command.width, 1280);
            EXPECT_LE(command.y + command.height, 240);
        }
    }
}

TEST_F(PerformancePanelTest, ActiveModesDrawWithoutHudState)
{
    for (int mode = 1; mode <= 3; ++mode)
    {
        layout->Build(*snapshot, 1920, 1080, mode);
        EXPECT_FALSE(layout->get_commands().empty());
        EXPECT_TRUE(HasText("FPS"));
    }
}

TEST_F(PerformancePanelTest, MissingValuesRemainExplicit)
{
    snapshot->summaries[uint32_t(performance::PerfSeries::RenderGpu)].state = ncl_perf::PerfSampleState::Pending;
    snapshot->summaries[uint32_t(performance::PerfSeries::RadarGpu)].state = ncl_perf::PerfSampleState::Dropped;
    snapshot->summaries[uint32_t(performance::PerfSeries::StudioGpu)].state = ncl_perf::PerfSampleState::Unavailable;
    layout->Build(*snapshot, 800, 600, 1);
    EXPECT_TRUE(HasText("Pending"));
    EXPECT_TRUE(HasText("Dropped"));
    EXPECT_TRUE(HasText("Unavailable"));
}

TEST_F(PerformancePanelTest, NarrowAndShortWindowsClipEveryCommand)
{
    for (int width : {32, 160, 320})
    {
        for (int height : {20, 120, 200})
        {
            layout->Build(*snapshot, width, height, 3);
            for (const performance::PerfPanelCommand& command : layout->get_commands())
            {
                EXPECT_GE(command.x, 0);
                EXPECT_GE(command.y, 0);
                EXPECT_LE(command.x + command.width, width);
                EXPECT_LE(command.y + command.height, height);
            }
        }
    }
}

TEST_F(PerformancePanelTest, GraphUsesMaximumAndLeavesEmptyBinsAsGaps)
{
    snapshot->graphs[0][100] = {9000000, 1000000, 20};
    layout->Build(*snapshot, 800, 1000, 2);
    uint32_t peaks = 0;
    uint32_t means = 0;
    for (const performance::PerfPanelCommand& command : layout->get_commands())
    {
        peaks += command.kind == performance::PerfPanelCommandKind::Peak;
        means += command.kind == performance::PerfPanelCommandKind::Mean;
        if (command.kind == performance::PerfPanelCommandKind::Peak)
        {
            EXPECT_GT(command.height, 1);
        }
    }
    EXPECT_EQ(peaks, 1u);
    EXPECT_EQ(means, 0u);
}

TEST_F(PerformancePanelTest, OverviewOverlaysTotalsAndStacksOnlyMeasuredParts)
{
    performance::PerfGroupedGraphs groups;
    groups.cpu[10].interval = {20000000, 10000000, 2};
    groups.cpu[10].total = {16000000, 8000000, 2};
    groups.cpu[10].parts = {2000000, 3000000, 4000000};
    groups.cpu[10].parts_available = true;
    groups.gpu[10].total = {8000000, 4000000, 2};
    groups.gpu[10].parts = {1000000, 2000000, 0};
    groups.gpu[10].parts_available = true;
    layout->Build(*snapshot, 1600, 1200, 2, nullptr, nullptr, 0, nullptr, &groups);

    std::vector<performance::PerfPanelCommand> peaks;
    uint32_t means = 0;
    for (const performance::PerfPanelCommand& command : layout->get_commands())
    {
        means += command.kind == performance::PerfPanelCommandKind::Mean;
        if (command.kind == performance::PerfPanelCommandKind::Peak)
        {
            peaks.push_back(command);
        }
    }
    ASSERT_EQ(peaks.size(), 8u);
    EXPECT_EQ(means, 0u);
    const performance::PerfPanelCommand& interval = peaks[0];
    const performance::PerfPanelCommand& processing = peaks[1];
    EXPECT_EQ(interval.x, processing.x);
    EXPECT_EQ(processing.x, peaks[2].x);
    EXPECT_GE(interval.height, 250);
    EXPECT_LE(interval.width, 3);
    for (const performance::PerfPanelCommand& peak : peaks)
    {
        EXPECT_EQ(peak.width, interval.width);
    }
    EXPECT_NEAR(processing.height, interval.height * 0.8, 1);
    EXPECT_EQ(peaks[2].x, peaks[3].x);
    EXPECT_EQ(peaks[3].x, peaks[4].x);
    EXPECT_NEAR(peaks[3].y + peaks[3].height, peaks[2].y, 1);
    EXPECT_NEAR(peaks[4].y + peaks[4].height, peaks[3].y, 1);
    EXPECT_GT(peaks[4].y, processing.y);
    EXPECT_EQ(peaks[5].x, peaks[6].x);
    EXPECT_EQ(peaks[6].x, peaks[7].x);
    EXPECT_NEAR(peaks[5].height, interval.height * 0.4, 1);
}

TEST_F(PerformancePanelTest, TableGroupsShowInclusionAndWarmupWithoutInventingP99)
{
    performance::PerfSummary& summary = snapshot->summaries[uint32_t(performance::PerfSeries::HostCpu)];
    summary.count = 5;
    summary.state = ncl_perf::PerfSampleState::Measured;
    summary.mean_ns = 2000000;
    summary.maximum_ns = 4000000;
    layout->Build(*snapshot, 1600, 1200, 1);
    ASSERT_NE(FindText("Swap Interval"), nullptr);
    ASSERT_NE(FindText("  Host Frame"), nullptr);
    ASSERT_NE(FindText("    Studio"), nullptr);
    ASSERT_NE(FindText("    Radar"), nullptr);
    ASSERT_NE(FindText("    Swap"), nullptr);
    EXPECT_TRUE(RowContains("  Host Frame", "2.00"));
    EXPECT_TRUE(RowContains("  Host Frame", "-"));
    EXPECT_TRUE(RowContains("  Host Frame", "4.00"));
    EXPECT_FALSE(RowContains("  Host Frame", "0.00"));
    EXPECT_NE(FindText("GPU (ms)"), nullptr);
    EXPECT_FALSE(HasText("Other Host"));
}

TEST_F(PerformancePanelTest, ColumnsSmoothAdjacentTopsAndLeaveMissingPartsEmpty)
{
    performance::PerfGroupedGraphs groups;
    groups.cpu[0].interval = {20000000, 10000000, 1};
    groups.gpu[10].total = {8000000, 4000000, 1};
    groups.gpu[10].parts = {1000000, 2000000, 0};
    groups.gpu[10].parts_available = true;
    groups.gpu[11].total = {16000000, 8000000, 1};
    groups.gpu[11].parts = {2000000, 4000000, 0};
    groups.gpu[11].parts_available = true;
    groups.gpu[12].total = {12000000, 6000000, 1};
    groups.gpu[13].total = {4000000, 2000000, 1};
    groups.gpu[13].parts = {500000, 1000000, 0};
    groups.gpu[13].parts_available = true;
    layout->Build(*snapshot, 1600, 1200, 2, nullptr, nullptr, 0, nullptr, &groups);

    std::vector<performance::PerfPanelCommand> studio;
    std::vector<performance::PerfPanelCommand> radar;
    for (const performance::PerfPanelCommand& command : layout->get_commands())
    {
        if (command.kind == performance::PerfPanelCommandKind::Peak && (command.color & 0xffffff00) == 0x53dc9a00)
        {
            studio.push_back(command);
        }
        if (command.kind == performance::PerfPanelCommandKind::Peak && (command.color & 0xffffff00) == 0x52e5ff00)
        {
            radar.push_back(command);
        }
    }
    ASSERT_EQ(studio.size(), 3u);
    ASSERT_EQ(radar.size(), 3u);
    EXPECT_GT(studio[0].vertex_y[0], studio[0].vertex_y[1]);
    EXPECT_EQ(studio[0].vertex_y[1], studio[1].vertex_y[0]);
    EXPECT_LE(studio[0].x + studio[0].width, studio[1].x);
    EXPECT_GT(studio[0].vertex_y[0], studio[1].vertex_y[0]);
    EXPECT_EQ(studio[0].y + studio[0].height, studio[1].y + studio[1].height);
    EXPECT_EQ(studio[1].vertex_y[0], studio[1].vertex_y[1]);
    EXPECT_GT(studio[2].x, studio[1].x + studio[1].width);
    for (uint32_t index = 0; index < studio.size(); ++index)
    {
        EXPECT_EQ(radar[index].vertex_y[3], studio[index].vertex_y[0]);
        EXPECT_EQ(radar[index].vertex_y[2], studio[index].vertex_y[1]);
    }
}

TEST_F(PerformancePanelTest, PerformanceGraphsDoNotAddMeanOverlays)
{
    performance::PerfFrameGraphs frames;
    frames.frames.resize(256);
    frames.frames.back().values[uint32_t(performance::PerfSeries::FrameInterval)] = {10000000, ncl_perf::PerfSampleState::Measured};
    frames.frames.back().values[uint32_t(performance::PerfSeries::HostCpu)] = {5000000, ncl_perf::PerfSampleState::Measured};
    frames.frames.back().values[uint32_t(performance::PerfSeries::RenderGpu)] = {2000000, ncl_perf::PerfSampleState::Measured};
    snapshot->graphs[0].back() = {10000000, 5000000, 2};
    for (int mode : {2, 3})
    {
        for (bool per_frame : {false, true})
        {
            layout->Build(*snapshot, 1600, 1200, mode, nullptr, nullptr, 0, nullptr, nullptr, per_frame ? &frames : nullptr);
            for (const performance::PerfPanelCommand& command : layout->get_commands())
            {
                EXPECT_NE(command.kind, performance::PerfPanelCommandKind::Mean);
            }
        }
    }
}

TEST_F(PerformancePanelTest, PerformanceAndNetworkUseTheSameNarrowSmoothedColumns)
{
    performance::PerfGroupedGraphs groups;
    performance::PerfNetworkSnapshot network;
    network.enabled = true;
    groups.cpu[10].total = {8000000, 4000000, 1};
    groups.cpu[11].total = {16000000, 8000000, 1};
    groups.cpu[13].total = {4000000, 2000000, 1};
    network.graphs[uint32_t(performance::PerfNetworkSeries::Latency)][10] = {8, 4, 1};
    network.graphs[uint32_t(performance::PerfNetworkSeries::Latency)][11] = {16, 8, 1};
    network.graphs[uint32_t(performance::PerfNetworkSeries::Latency)][13] = {4, 2, 1};
    for (uint32_t index : {10u, 11u, 13u})
    {
        snapshot->graphs[uint32_t(performance::PerfSeries::HostCpu)][index] = groups.cpu[index].total;
    }

    for (int mode : {2, 3})
    {
        layout->Build(*snapshot, 1600, 1200, mode, &network, nullptr, 0, nullptr, &groups);
        std::vector<performance::PerfPanelCommand> timing;
        std::vector<performance::PerfPanelCommand> latency;
        for (const performance::PerfPanelCommand& command : layout->get_commands())
        {
            if (command.kind == performance::PerfPanelCommandKind::Peak && (command.color & 0xffffff00) == 0xe6ab6300)
            {
                timing.push_back(command);
            }
            if (command.kind == performance::PerfPanelCommandKind::Peak && (command.color & 0xffffff00) == 0xd8cf7a00)
            {
                latency.push_back(command);
            }
        }
        ASSERT_EQ(timing.size(), 3u);
        ASSERT_EQ(latency.size(), 3u);
        for (uint32_t index = 0; index < timing.size(); ++index)
        {
            EXPECT_EQ(timing[index].x - timing.front().x, latency[index].x - latency.front().x) << mode;
            EXPECT_EQ(timing[index].width, latency[index].width) << mode;
            EXPECT_LE(timing[index].width, 3) << mode;
        }
        for (const auto* columns : {&timing, &latency})
        {
            EXPECT_GT((*columns)[0].vertex_y[0], (*columns)[0].vertex_y[1]) << mode;
            EXPECT_EQ((*columns)[0].vertex_y[1], (*columns)[1].vertex_y[0]) << mode;
            EXPECT_EQ((*columns)[1].vertex_y[0], (*columns)[1].vertex_y[1]) << mode;
            EXPECT_LT((*columns)[1].x + (*columns)[1].width, (*columns)[2].x) << mode;
        }
    }
}

TEST_F(PerformancePanelTest, BackingOpacityClampsWithoutChangingItsColor)
{
    struct Case
    {
        float alpha;
        uint32_t expected;
        uint32_t plot;
    };
    for (const Case& item :
         {Case{-1.0f, 0, 0}, Case{0.25f, 64, 40}, Case{2.0f, 255, 160}, Case{std::numeric_limits<float>::quiet_NaN(), 51, 32}})
    {
        layout->SetBackgroundAlpha(item.alpha);
        layout->Build(*snapshot, 1600, 1200, 2, nullptr, nullptr, 2);
        uint32_t backings = 0;
        uint32_t plots = 0;
        for (const performance::PerfPanelCommand& command : layout->get_commands())
        {
            if (command.kind == performance::PerfPanelCommandKind::Rect && (command.color & 0xffffff00) == 0x11131d00)
            {
                ++backings;
                EXPECT_EQ(command.color & 255, item.expected);
            }
            if (command.kind == performance::PerfPanelCommandKind::Rect && (command.color & 0xffffff00) == 0x06091100)
            {
                ++plots;
                EXPECT_EQ(command.color & 255, item.plot);
            }
        }
        EXPECT_EQ(backings, 2u);
        EXPECT_EQ(plots, 2u);
    }
}

TEST_F(PerformancePanelTest, PartsRemainInsideTheirParentWhenTheFollowingBucketHasNoParts)
{
    performance::PerfGroupedGraphs groups;
    groups.gpu[10].total = {16000000, 12000000, 1};
    groups.gpu[10].parts = {8000000, 4000000, 0};
    groups.gpu[10].parts_available = true;
    groups.gpu[11].total = {4000000, 2000000, 1};
    layout->Build(*snapshot, 1600, 1200, 2, nullptr, nullptr, 0, nullptr, &groups);

    const performance::PerfPanelCommand* render = nullptr;
    const performance::PerfPanelCommand* radar = nullptr;
    for (const performance::PerfPanelCommand& command : layout->get_commands())
    {
        if (command.kind == performance::PerfPanelCommandKind::Peak && (command.color & 0xffffff00) == 0xff707000 && !render)
        {
            render = &command;
        }
        if (command.kind == performance::PerfPanelCommandKind::Peak && (command.color & 0xffffff00) == 0x52e5ff00)
        {
            radar = &command;
        }
    }
    ASSERT_NE(render, nullptr);
    ASSERT_NE(radar, nullptr);
    EXPECT_EQ(render->x, radar->x);
    EXPECT_GE(radar->vertex_y[0], render->vertex_y[0]);
    EXPECT_GE(radar->vertex_y[1], render->vertex_y[1]);
}

TEST_F(PerformancePanelTest, FractionalScaleKeepsStackedColumnEdgesJoinedAndInsideTheWindow)
{
    performance::PerfGroupedGraphs groups;
    groups.cpu[255].interval = {10000000, 10000000, 1};
    groups.cpu[255].total = {10000000, 10000000, 1};
    groups.cpu[255].parts = {50000, 1000000, 1000000};
    groups.cpu[255].parts_available = true;
    for (float scale : {0.5f, 0.75f, 1.0f, 1.3f})
    {
        snapshot->panel_scale = scale;
        layout->Build(*snapshot, 1600, 1200, 2, nullptr, nullptr, 0, nullptr, &groups);
        const performance::PerfPanelCommand* studio = nullptr;
        const performance::PerfPanelCommand* radar = nullptr;
        for (const performance::PerfPanelCommand& command : layout->get_commands())
        {
            if (command.kind != performance::PerfPanelCommandKind::Text)
            {
                for (int vertex : command.vertex_y)
                {
                    EXPECT_GE(vertex, command.y);
                    EXPECT_LE(vertex, command.y + command.height);
                    EXPECT_GE(vertex, 0);
                    EXPECT_LE(vertex, 1200);
                }
            }
            if (command.kind == performance::PerfPanelCommandKind::Peak && (command.color & 0xffffff00) == 0x90cf7000)
            {
                studio = &command;
            }
            if (command.kind == performance::PerfPanelCommandKind::Peak && (command.color & 0xffffff00) == 0x71bdff00)
            {
                radar = &command;
            }
        }
        ASSERT_NE(studio, nullptr);
        ASSERT_NE(radar, nullptr);
        EXPECT_EQ(radar->vertex_y[3], studio->vertex_y[0]) << scale;
        EXPECT_EQ(radar->vertex_y[2], studio->vertex_y[1]) << scale;
    }
}

TEST_F(PerformancePanelTest, GraphStatisticsUseHistoryValuesBelowMinimumScale)
{
    performance::PerfSummary& summary = snapshot->summaries[0];
    summary.count = 3;
    summary.state = ncl_perf::PerfSampleState::Measured;
    summary.maximum_ns = 750000;
    summary.median_ns = 250000;
    summary.p99_ns = 250000;
    summary.percentiles_available = true;
    summary.mean_ns = 400000;
    snapshot->graphs[0][100] = {750000, 400000, 3};

    layout->Build(*snapshot, 1600, 1200, 3);
    EXPECT_NE(FindText("1.00"), nullptr);
    EXPECT_NE(FindText("0.50"), nullptr);
    const performance::PerfPanelCommand* average = FindText("0.40");
    const performance::PerfPanelCommand* percentile = FindText("0.25");
    const performance::PerfPanelCommand* maximum = FindText("0.75");
    ASSERT_NE(average, nullptr);
    ASSERT_NE(percentile, nullptr);
    ASSERT_NE(maximum, nullptr);
    EXPECT_EQ(average->y, percentile->y);
    EXPECT_EQ(percentile->y, maximum->y);
    EXPECT_LT(average->x, percentile->x);
    EXPECT_LT(percentile->x, maximum->x);
}

TEST_F(PerformancePanelTest, IndependentAxesMatchDurationsAndReserveLabelSpace)
{
    snapshot->graphs[0][0] = {9000000, 4500000, 20};
    snapshot->graphs[1][0] = {2000000, 1000000, 20};
    layout->Build(*snapshot, 1600, 1200, 3);

    const performance::PerfPanelCommand* top = FindText("9.00");
    const performance::PerfPanelCommand* middle = FindText("4.50");
    const performance::PerfPanelCommand* bottom = FindText("0.00");
    ASSERT_NE(top, nullptr);
    ASSERT_NE(middle, nullptr);
    ASSERT_NE(bottom, nullptr);
    EXPECT_LT(top->y, middle->y);
    EXPECT_LT(middle->y, bottom->y);
    EXPECT_NE(FindText("2.00"), nullptr);

    const performance::PerfPanelCommand* frame_peak = nullptr;
    const performance::PerfPanelCommand* host_peak = nullptr;
    for (const performance::PerfPanelCommand& command : layout->get_commands())
    {
        if (command.kind == performance::PerfPanelCommandKind::Peak)
        {
            if (!frame_peak)
            {
                frame_peak = &command;
            }
            else
            {
                host_peak = &command;
            }
        }
    }
    ASSERT_NE(frame_peak, nullptr);
    ASSERT_NE(host_peak, nullptr);
    EXPECT_LE(top->x + top->width, frame_peak->x);
    EXPECT_NEAR(frame_peak->height, host_peak->height, 1);
}

TEST_F(PerformancePanelTest, SharedAxisScalesEveryPerformanceSeries)
{
    snapshot->graphs[uint32_t(performance::PerfSeries::FrameInterval)][0] = {18000000, 9000000, 20};
    snapshot->graphs[uint32_t(performance::PerfSeries::HostCpu)][0] = {9000000, 4500000, 20};
    snapshot->graphs[uint32_t(performance::PerfSeries::RenderGpu)][0] = {4500000, 3000000, 20};
    layout->Build(*snapshot, 1600, 1200, 2);

    uint32_t top_labels = 0;
    const performance::PerfPanelCommand* frame_peak = nullptr;
    const performance::PerfPanelCommand* gpu_peak = nullptr;
    for (const performance::PerfPanelCommand& command : layout->get_commands())
    {
        if (command.kind == performance::PerfPanelCommandKind::Text && std::string(layout->get_text(command)) == "18.00")
        {
            ++top_labels;
        }
        if (command.kind == performance::PerfPanelCommandKind::Peak)
        {
            if (!frame_peak)
            {
                frame_peak = &command;
            }
            else
            {
                gpu_peak = &command;
            }
        }
    }
    EXPECT_EQ(top_labels, 2u);
    ASSERT_NE(frame_peak, nullptr);
    ASSERT_NE(gpu_peak, nullptr);
    EXPECT_EQ(frame_peak->x, gpu_peak->x);
    EXPECT_GT(gpu_peak->y, frame_peak->y + frame_peak->height);
    EXPECT_NEAR(gpu_peak->height, frame_peak->height / 4.0, 1);
}

TEST_F(PerformancePanelTest, AxisKeepsLargeDurationsReadable)
{
    snapshot->graphs[0][0] = {1234567890000000, 1000000, 1};
    layout->Build(*snapshot, 1600, 1200, 3);
    double tick_values[2]{};
    uint32_t tick_count = 0;
    for (const performance::PerfPanelCommand& command : layout->get_commands())
    {
        if (command.kind != performance::PerfPanelCommandKind::Text)
        {
            continue;
        }
        const char* text = layout->get_text(command);
        if (*text < '0' || *text > '9')
        {
            continue;
        }

        char* end = nullptr;
        tick_values[tick_count++] = std::strtod(text, &end);
        EXPECT_EQ(*end, '\0');
        if (tick_count == 2)
        {
            break;
        }
    }
    ASSERT_EQ(tick_count, 2u);
    EXPECT_NEAR(tick_values[0], 1234567890.0, 50000000);
    EXPECT_NEAR(tick_values[1], 617283945.0, 25000000);
}

TEST_F(PerformancePanelTest, EmptyGraphStatisticsDoNotReportZeroCost)
{
    layout->Build(*snapshot, 1600, 1200, 3);
    const performance::PerfPanelCommand* status = FindText("Unavailable");
    ASSERT_NE(status, nullptr);
    uint32_t missing_values = 0;
    for (const performance::PerfPanelCommand& command : layout->get_commands())
    {
        if (command.kind == performance::PerfPanelCommandKind::Text && command.y == status->y)
        {
            std::string text = layout->get_text(command);
            missing_values += text == "-";
            EXPECT_NE(text, "0.00");
        }
    }
    EXPECT_EQ(missing_values, 2u);
}

TEST_F(PerformancePanelTest, PlotModesKeepDetailsInDumpAndExposeAllSeries)
{
    snapshot->cpu_count = 1;
    std::snprintf(snapshot->cpu_tree[0].name, ncl_perf::kPerfNameCapacity, "Hidden CPU detail");
    for (int mode : {2, 3})
    {
        layout->Build(*snapshot, 1600, 1200, mode);
        EXPECT_TRUE(HasText("Radar"));
        EXPECT_TRUE(HasText("Monitor"));
        EXPECT_FALSE(HasText("Hidden CPU detail"));
        EXPECT_FALSE(HasText("Worker"));
        EXPECT_FALSE(HasText("draws"));
    }
}

TEST_F(PerformancePanelTest, OverviewKeepsSmallCostsProportionalToTheFrameInterval)
{
    snapshot->graphs[0][0] = {100000000, 100000000, 1};
    snapshot->graphs[uint32_t(performance::PerfSeries::HostCpu)][0] = {10000000, 10000000, 1};
    snapshot->graphs[uint32_t(performance::PerfSeries::RenderGpu)][0] = {5000000, 5000000, 1};
    layout->Build(*snapshot, 1600, 1200, 2);
    EXPECT_NE(FindText("100.0"), nullptr);
    EXPECT_NE(FindText("50.00"), nullptr);
    EXPECT_EQ(FindText("10.00"), nullptr);
    EXPECT_TRUE(HasText("Host Frame"));
    EXPECT_TRUE(HasText("Swap"));
    EXPECT_TRUE(HasText("Render"));
    EXPECT_TRUE(HasText("Radar"));
    EXPECT_FALSE(HasText("Monitor CPU (ms)"));

    int plot_height = 0;
    for (const performance::PerfPanelCommand& command : layout->get_commands())
    {
        if (command.kind == performance::PerfPanelCommandKind::Peak)
        {
            plot_height = std::max(plot_height, command.height);
        }
    }
    EXPECT_GE(plot_height, 140);
}

TEST_F(PerformancePanelTest, NarrowTitleEndsAtUtf8CharacterBoundary)
{
    performance::PerfPanelLabels labels;
    std::snprintf(labels.title, sizeof(labels.title), "1234567890123456789\xd0\x9f\xd1\x80\xd0\xbe\xd0\xb8\xd0\xb7\xd0\xb2\xd0\xbe");
    layout->SetLabels(labels);
    layout->Build(*snapshot, 180, 240, 1);
    for (const performance::PerfPanelCommand& command : layout->get_commands())
    {
        if (command.kind != performance::PerfPanelCommandKind::Text)
        {
            continue;
        }
        const char* cursor = layout->get_text(command);
        while (*cursor)
        {
            uchar32 codepoint = 0;
            bool error = false;
            int consumed = Q_UTF8ToUChar32(cursor, codepoint, error);
            EXPECT_FALSE(error);
            ASSERT_GT(consumed, 0);
            cursor += consumed;
        }
    }
}

TEST_F(PerformancePanelTest, AutomaticScaleUses1600By1200AsItsReference)
{
    snapshot->panel_scale = 0;
    layout->Build(*snapshot, 1600, 1200, 1);
    int base_font = layout->get_font_tall();
    int base_width = layout->get_commands()[0].width;
    layout->Build(*snapshot, 1920, 1440, 1);
    EXPECT_EQ(base_font, 14);
    EXPECT_EQ(layout->get_font_tall(), 17);
    EXPECT_GT(layout->get_commands()[0].width, base_width);
}

TEST_F(PerformancePanelTest, ExplicitScaleOverridesAutomaticScaleAndKeepsClipping)
{
    snapshot->panel_scale = 2.0f;
    layout->Build(*snapshot, 1600, 1200, 3);
    EXPECT_EQ(layout->get_font_tall(), 28);
    for (const performance::PerfPanelCommand& command : layout->get_commands())
    {
        EXPECT_LE(command.x + command.width, 1600);
        EXPECT_LE(command.y + command.height, 1200);
    }
    snapshot->panel_scale = 1.0f;
    layout->Build(*snapshot, 1600, 1200, 1);
    EXPECT_EQ(layout->get_font_tall(), 14);
}

TEST_F(PerformancePanelTest, NewBaseEqualsPrior115ScaleAt1600By1200)
{
    snapshot->panel_scale = 1;
    layout->Build(*snapshot, 1600, 1200, 1);
    EXPECT_FLOAT_EQ(layout->get_scale(), 1.15f);
    EXPECT_EQ(layout->get_font_tall(), 14);
    EXPECT_EQ(layout->get_commands()[0].width, 460);
}

TEST_F(PerformancePanelTest, NetworkOnlyPanelDoesNotRequirePerformanceCollection)
{
    performance::PerfNetworkSnapshot network;
    network.enabled = true;
    network.connected = true;
    network.graphs[uint32_t(performance::PerfNetworkSeries::Latency)][10] = {80, 40, 2};
    network.graphs[uint32_t(performance::PerfNetworkSeries::IncomingTraffic)][10] = {20, 10, 2};
    network.graphs[uint32_t(performance::PerfNetworkSeries::OutgoingTraffic)][10] = {10, 5, 2};
    layout->Build(*snapshot, 1600, 1200, 0, &network, nullptr, 0);
    EXPECT_TRUE(HasText("Network"));
    EXPECT_TRUE(HasText("Latency (ms)"));
    EXPECT_TRUE(HasText("Packet size (B)"));
    EXPECT_FALSE(HasText("FPS"));
    EXPECT_FALSE(HasText("Studio"));
    EXPECT_NE(FindText("80.00"), nullptr);
    EXPECT_NE(FindText("20.00"), nullptr);
    uint32_t means = 0;
    for (const performance::PerfPanelCommand& command : layout->get_commands())
    {
        means += command.kind == performance::PerfPanelCommandKind::Mean;
    }
    EXPECT_EQ(means, 0u);
}

TEST_F(PerformancePanelTest, NetworkBlockLeavesRoomForAllPerformanceSeries)
{
    performance::PerfNetworkSnapshot network;
    network.enabled = true;
    network.connected = true;
    for (uint32_t series = 0; series < uint32_t(performance::PerfSeries::MonitorCpu); ++series)
    {
        snapshot->graphs[series][100] = {1000000, 500000, 1};
    }
    for (int mode : {2, 3})
    {
        layout->Build(*snapshot, 1600, 1200, mode, &network);
        EXPECT_TRUE(HasText("Packet size (B)"));
        EXPECT_TRUE(HasText("Radar"));
        EXPECT_NE(FindText(mode == 2 ? "GPU (ms)" : "GPU"), nullptr);
        EXPECT_TRUE(HasText("Monitor"));
        uint32_t performance_peaks = 0;
        for (const performance::PerfPanelCommand& command : layout->get_commands())
        {
            performance_peaks += command.kind == performance::PerfPanelCommandKind::Peak;
            EXPECT_LE(command.y + command.height, 1200);
        }
        EXPECT_EQ(performance_peaks, mode == 2 ? 3u : 8u);
    }
}

TEST_F(PerformancePanelTest, NetworkFallbackPreservesStockGraphWhenStatisticsCannotFit)
{
    performance::PerfNetworkSnapshot network;
    network.enabled = true;
    EXPECT_FALSE(performance::PerformancePanelLayout::CanShowNetwork(1, 320, 1200));
    EXPECT_FALSE(performance::PerformancePanelLayout::CanShowNetwork(2, 1600, 120));
    EXPECT_TRUE(performance::PerformancePanelLayout::CanShowNetwork(1, 640, 480));
    layout->Build(*snapshot, 640, 150, 2, &network);
    EXPECT_FALSE(HasText("Network"));
    layout->Build(*snapshot, 320, 1200, 0, &network);
    EXPECT_TRUE(layout->get_commands().empty());
}

TEST_F(PerformancePanelTest, VisibleNetworkKeepsTrafficRowsAtShortHeights)
{
    snapshot->panel_scale = 1;
    performance::PerfNetworkSnapshot network;
    network.enabled = true;
    for (int mode : {1, 2, 3})
    {
        for (int height = 180; height <= 420; ++height)
        {
            for (int batching : {0, 1, 2})
            {
                SCOPED_TRACE(testing::Message() << "mode=" << mode << " height=" << height << " batching=" << batching);
                if (!performance::PerformancePanelLayout::CanShowNetwork(1, 640, height))
                {
                    continue;
                }

                layout->Build(*snapshot, 640, height, mode, &network, nullptr, batching);
                EXPECT_TRUE(HasText("Network"));
                EXPECT_TRUE(HasText("Latency "));
                EXPECT_TRUE(HasText("In - KiB/s"));
                EXPECT_TRUE(HasText("Out - KiB/s"));
            }
        }
    }
}

TEST_F(PerformancePanelTest, MapperPanelShowsOnlyRequestedInspectionRowsWithoutPerformance)
{
    performance::PerfMapperSnapshot mapper;
    mapper.enabled = true;
    mapper.level = 1;
    mapper.map_loaded = true;
    mapper.view_measured = true;
    mapper.wpoly = 1234;
    mapper.epoly = 5678;
    layout->Build(*snapshot, 1600, 1200, 0, nullptr, &mapper);
    EXPECT_TRUE(HasText("wpoly 1234"));
    EXPECT_TRUE(HasText("epoly 5678"));
    EXPECT_TRUE(HasText("Brush entities"));
    EXPECT_TRUE(HasText("Position"));
    EXPECT_TRUE(HasText("Angles"));
    EXPECT_TRUE(HasText("Look"));
    EXPECT_TRUE(HasText("E: dump to console"));
    EXPECT_FALSE(HasText("Object"));
    EXPECT_FALSE(HasText("Vanilla"));
    EXPECT_FALSE(HasText("Model slots"));
    EXPECT_FALSE(HasText("FPS"));
}

TEST_F(PerformancePanelTest, MapperDetailsExposeOverflowAndKeepBoundsLabelBeforeLongNames)
{
    performance::PerfMapperSnapshot mapper;
    mapper.enabled = mapper.map_loaded = mapper.view_measured = mapper.target_hit = mapper.target_bounds = true;
    mapper.level = 2;
    mapper.target_name = std::string(63, 'a');
    for (int index = 0; index < 20; ++index)
    {
        mapper.properties.push_back({"key" + std::to_string(index), "value"});
    }
    layout->Build(*snapshot, 1600, 1800, 0, nullptr, &mapper);
    EXPECT_TRUE(HasText("Object [bounds]"));
    EXPECT_TRUE(HasText("Object origin: 0.0 0.0 0.0"));
    EXPECT_TRUE(HasText("E: dump to console"));
    EXPECT_TRUE(HasText("key15: value"));
    EXPECT_FALSE(HasText("key16: value"));
    EXPECT_TRUE(HasText("Properties 16/20"));
    mapper.level = 1;
    layout->Build(*snapshot, 1600, 1800, 0, nullptr, &mapper);
    EXPECT_FALSE(HasText("key0:"));
    EXPECT_FALSE(HasText("Object"));
    EXPECT_FALSE(HasText(mapper.target_name.c_str()));
}

TEST_F(PerformancePanelTest, MapperLevelOneRefreshKeepsLookCoordinatesWithoutObjectRow)
{
    performance::PerfMapperSnapshot mapper;
    mapper.enabled = mapper.map_loaded = mapper.view_measured = mapper.target_hit = true;
    mapper.level = 1;
    mapper.target_name = "func_wall";
    mapper.target_position = {1, 2, 3};
    layout->Build(*snapshot, 1600, 1800, 0, nullptr, &mapper);
    EXPECT_TRUE(HasText("Look: 1.0 2.0 3.0"));
    EXPECT_FALSE(HasText("Object"));
    EXPECT_FALSE(HasText("func_wall"));

    mapper.target_hit = false;
    layout->RefreshMapper(mapper);
    EXPECT_TRUE(HasText("Look: -"));
    EXPECT_FALSE(HasText("Object"));
}

TEST_F(PerformancePanelTest, MapperRefreshUpdatesGazeWithoutChangingCachedPerformance)
{
    performance::PerfMapperSnapshot mapper;
    mapper.enabled = mapper.map_loaded = mapper.view_measured = mapper.target_hit = true;
    mapper.level = 2;
    mapper.target_name = "func_wall";
    mapper.target_position = {1, 2, 3};
    snapshot->summaries[0].fps = 77;
    layout->Build(*snapshot, 1600, 1800, 1, nullptr, &mapper);
    ASSERT_TRUE(HasText("func_wall"));
    mapper.target_name = "func_door";
    mapper.target_position = {4, 5, 6};
    mapper.target_origin = {40, 50, 60};
    snapshot->summaries[0].fps = 999;
    layout->RefreshMapper(mapper);
    EXPECT_TRUE(HasText("func_door"));
    EXPECT_TRUE(HasText("Look: 4.0 5.0 6.0"));
    EXPECT_TRUE(HasText("Object origin: 40.0 50.0 60.0"));
    EXPECT_FALSE(HasText("func_wall"));
    EXPECT_FALSE(HasText("FPS 999"));
}

TEST_F(PerformancePanelTest, AllOptionalBlocksFitTogetherAt1600By1200)
{
    performance::PerfNetworkSnapshot network;
    network.enabled = true;
    performance::PerfMapperSnapshot mapper;
    mapper.enabled = true;
    mapper.map_loaded = true;
    for (uint32_t series = 0; series < uint32_t(performance::PerfSeries::MonitorCpu); ++series)
    {
        snapshot->graphs[series][100] = {1000000, 500000, 1};
    }
    for (int mode : {2, 3})
    {
        layout->Build(*snapshot, 1600, 1200, mode, &network, &mapper, 2);
        EXPECT_TRUE(HasText("wpoly"));
        EXPECT_TRUE(HasText("Packet size (B)"));
        EXPECT_TRUE(HasText("Monitor"));
        uint32_t peaks = 0;
        for (const performance::PerfPanelCommand& command : layout->get_commands())
        {
            peaks += command.kind == performance::PerfPanelCommandKind::Peak;
        }
        EXPECT_EQ(peaks, mode == 2 ? 3u : 8u);
        for (const performance::PerfPanelCommand& first : layout->get_commands())
        {
            if (first.kind != performance::PerfPanelCommandKind::Text)
            {
                continue;
            }
            char* end = nullptr;
            const char* first_text = layout->get_text(first);
            std::strtod(first_text, &end);
            if (end == first_text || *end)
            {
                continue;
            }
            for (const performance::PerfPanelCommand& second : layout->get_commands())
            {
                if (&first == &second || second.kind != performance::PerfPanelCommandKind::Text || first.x != second.x)
                {
                    continue;
                }
                const char* second_text = layout->get_text(second);
                std::strtod(second_text, &end);
                if (end != second_text && !*end)
                {
                    EXPECT_TRUE(first.y + first.height <= second.y || second.y + second.height <= first.y)
                        << first_text << " overlaps " << second_text;
                }
            }
        }
    }
}

TEST_F(PerformancePanelTest, OptionalMonitorsKeepTheirPanelsAndNetworkWidthAcrossPerformanceModes)
{
    performance::PerfNetworkSnapshot network;
    network.enabled = network.connected = true;
    uint32_t count = performance::PerformancePanelLayout::NetworkColumnCount(1, 1600, 3000);
    for (std::vector<performance::PerfPlotBin>& graph : network.graphs)
    {
        graph.assign(count, performance::PerfPlotBin{40, 40, 1});
    }
    performance::PerfMapperSnapshot mapper;
    mapper.enabled = mapper.map_loaded = mapper.view_measured = true;
    mapper.level = 2;

    for (int mode : {0, 1, 2, 3})
    {
        SCOPED_TRACE(mode);
        layout->Build(*snapshot, 1600, 3000, mode, &network, &mapper, 2);
        EXPECT_TRUE(HasText("Network"));
        EXPECT_TRUE(HasText("Model draws"));
        EXPECT_TRUE(HasText("Map inspection"));
        std::vector<performance::PerfPanelCommand> panels;
        uint32_t packets = 0;
        for (const performance::PerfPanelCommand& command : layout->get_commands())
        {
            if (command.kind == performance::PerfPanelCommandKind::Rect && (command.color & 0xffffff00) == 0x11131d00)
            {
                panels.push_back(command);
            }
            packets += command.kind == performance::PerfPanelCommandKind::Peak && (command.color & 0xffffff00) == 0xd8cf7a00;
        }
        ASSERT_EQ(panels.size(), mode ? 4u : 3u);
        EXPECT_EQ(packets, count);
        for (size_t index = 1; index < panels.size(); ++index)
        {
            EXPECT_EQ(panels[index].x + panels[index].width, panels.front().x + panels.front().width);
            EXPECT_GT(panels[index].y, panels[index - 1].y + panels[index - 1].height);
        }
        const performance::PerfPanelCommand* network_title = FindText(("Network | last " + std::to_string(count) + " packets").c_str());
        const performance::PerfPanelCommand* batch_title = FindText("Studio batching | totals, last 0 frames");
        const performance::PerfPanelCommand* map_title = FindText("Map inspection");
        ASSERT_NE(network_title, nullptr);
        ASSERT_NE(batch_title, nullptr);
        ASSERT_NE(map_title, nullptr);
        EXPECT_LT(network_title->y, batch_title->y);
        EXPECT_LT(batch_title->y, map_title->y);
    }
}

TEST_F(PerformancePanelTest, NarrowLastColumnKeepsEveryMonitorTitleAndNetworkPlots)
{
    performance::PerfNetworkSnapshot network;
    network.enabled = true;
    performance::PerfMapperSnapshot mapper;
    mapper.enabled = mapper.map_loaded = true;
    mapper.level = 2;
    for (int index = 0; index < 20; ++index)
    {
        mapper.properties.push_back({"key" + std::to_string(index), "value"});
    }
    for (int mode : {1, 2, 3})
    {
        SCOPED_TRACE(mode);
        layout->Build(*snapshot, 640, 480, mode, &network, &mapper, 2);
        EXPECT_TRUE(HasText("Performance"));
        EXPECT_TRUE(HasText("Network"));
        EXPECT_TRUE(HasText("Studio batching"));
        EXPECT_TRUE(HasText("Map inspection"));
        EXPECT_TRUE(HasText("Latency (ms)"));
        EXPECT_TRUE(HasText("Packet size (B)"));
    }
}

TEST_F(PerformancePanelTest, CrowdedMonitorsFlowLeftAndKeepEveryNetworkPacketColumn)
{
    performance::PerfNetworkSnapshot network;
    network.enabled = network.connected = true;
    performance::PerfMapperSnapshot mapper;
    mapper.enabled = mapper.map_loaded = mapper.view_measured = mapper.target_hit = true;
    mapper.level = 2;
    mapper.target_name = "func_door";
    for (int index = 0; index < 20; ++index)
    {
        mapper.properties.push_back({"key" + std::to_string(index), "value"});
    }

    for (float scale : {0.75f, 1.0f})
    {
        snapshot->panel_scale = scale;
        for (int mode : {1, 2, 3})
        {
            SCOPED_TRACE(testing::Message() << "scale=" << scale << " mode=" << mode);
            uint32_t count = performance::PerformancePanelLayout::NetworkColumnCount(scale, 1600, 1200);
            ASSERT_GT(count, 0u);
            for (std::vector<performance::PerfPlotBin>& graph : network.graphs)
            {
                graph.assign(count, performance::PerfPlotBin{40, 40, 1});
            }
            layout->Build(*snapshot, 1600, 1200, mode, &network, &mapper, 2);
            ASSERT_TRUE(HasText("Latency (ms)"));
            ASSERT_TRUE(HasText("Packet size (B)"));
            ASSERT_TRUE(HasText("Object origin:"));
            const performance::PerfPanelCommand* performance = FindText("Performance | FPS 0");
            const performance::PerfPanelCommand* map = FindText("Map inspection");
            const performance::PerfPanelCommand* batch = FindText("Studio batching | totals, last 0 frames");
            ASSERT_NE(performance, nullptr);
            ASSERT_NE(map, nullptr);
            ASSERT_NE(batch, nullptr);
            if (scale == 1)
            {
                EXPECT_LT(std::min(map->x, batch->x), performance->x);
            }

            uint32_t packets = 0;
            std::vector<performance::PerfPanelCommand> backings;
            for (const performance::PerfPanelCommand& command : layout->get_commands())
            {
                packets += command.kind == performance::PerfPanelCommandKind::Peak && (command.color & 0xffffff00) == 0xd8cf7a00;
                if (command.kind == performance::PerfPanelCommandKind::Rect && (command.color & 0xffffff00) == 0x11131d00)
                {
                    backings.push_back(command);
                }
                EXPECT_GE(command.x, 0);
                EXPECT_GE(command.y, 0);
                EXPECT_LE(command.x + command.width, 1600);
                EXPECT_LE(command.y + command.height, 1200);
            }
            EXPECT_EQ(packets, count);
            ASSERT_EQ(backings.size(), 4u);
            for (size_t index = 1; index < backings.size(); ++index)
            {
                const performance::PerfPanelCommand& previous = backings[index - 1];
                const performance::PerfPanelCommand& current = backings[index];
                if (std::abs(current.x + current.width - previous.x - previous.width) <= 1)
                {
                    EXPECT_GT(current.y, previous.y + previous.height);
                }
                else
                {
                    EXPECT_LT(current.x + current.width, previous.x);
                    EXPECT_LE(current.y, previous.y);
                }
            }
            for (size_t first = 0; first < backings.size(); ++first)
            {
                for (size_t second = first + 1; second < backings.size(); ++second)
                {
                    const performance::PerfPanelCommand& a = backings[first];
                    const performance::PerfPanelCommand& b = backings[second];
                    EXPECT_TRUE(a.x + a.width <= b.x || b.x + b.width <= a.x || a.y + a.height <= b.y || b.y + b.height <= a.y);
                }
            }

            for (std::vector<performance::PerfPlotBin>& graph : network.graphs)
            {
                graph.assign(8, performance::PerfPlotBin{20, 20, 1});
            }
            layout->RefreshNetwork(network);
            mapper.target_origin = {11, 12, 13};
            layout->RefreshMapper(mapper);
            snapshot->counters[uint32_t(performance::PerfCounterId::StudioPackets)] = 7;
            snapshot->counters[uint32_t(performance::PerfCounterId::StudioSharedPackets)] = 7;
            layout->RefreshBatching(*snapshot, nullptr);
            EXPECT_TRUE(HasText("Network | last 8 packets"));
            EXPECT_TRUE(HasText("Object origin: 11.0 12.0 13.0"));
            EXPECT_TRUE(HasText("E: dump to console"));
            EXPECT_TRUE(RowContains("Model draws", "7"));
        }
    }
}

TEST_F(PerformancePanelTest, StudioLiveWindowUsesMatchingCpuAndCounterSamples)
{
    snapshot->cpu_frame_count = 100;
    snapshot->counters[uint32_t(performance::PerfCounterId::StudioPackets)] = 123;
    snapshot->counters[uint32_t(performance::PerfCounterId::StudioSharedPackets)] = 123;
    std::unique_ptr<performance::PerfSnapshot> captured = std::make_unique<performance::PerfSnapshot>();
    captured->cpu_frame_count = 1;
    captured->cpu_window_ns = 10000000;
    captured->counters[uint32_t(performance::PerfCounterId::StudioPackets)] = 7;
    captured->counters[uint32_t(performance::PerfCounterId::StudioSharedPackets)] = 7;
    captured->cpu_count = 1;
    captured->cpu_tree[0] = {performance::PerfStage::StudioDataPrepare, performance::PerfStage::Studio,
                            2000000, 2000000, 3, ncl_perf::PerfSampleState::Measured, "Model data preparation"};
    layout->Build(*snapshot, 1600, 1200, 2, nullptr, nullptr, 2, captured.get());
    EXPECT_TRUE(HasText("CPU live: averages, last 1 frames"));
    EXPECT_TRUE(HasText("Top detail: Model data preparation 2.000 ms/frame"));
    EXPECT_TRUE(RowContains("Model draws", "7"));
    layout->Build(*snapshot, 640, 480, 2, nullptr, nullptr, 2, captured.get());
    EXPECT_TRUE(HasText("Top detail: Model data preparation 2.000 ms/frame"));
}

TEST_F(PerformancePanelTest, OverviewCpuTotalsDoNotReplaceTheBatchWindow)
{
    snapshot->cpu_frame_count = 10;
    snapshot->cpu_count = 1;
    snapshot->cpu_tree[0] = {performance::PerfStage::Studio, 0, 5000000, 5000000, 10, ncl_perf::PerfSampleState::Measured, "Studio"};
    layout->Build(*snapshot, 1600, 1200, 2, nullptr, nullptr, 2);
    EXPECT_TRUE(HasText("CPU live: waiting for frame"));
    EXPECT_TRUE(HasText("Other Studio: -"));
    EXPECT_FALSE(HasText("CPU dump:"));
}

TEST_F(PerformancePanelTest, StudioDetailsUseTheIndependentBatchWindow)
{
    snapshot->cpu_frame_count = 10;
    snapshot->cpu_count = 1;
    snapshot->cpu_tree[0] = {performance::PerfStage::StudioDataPrepare, performance::PerfStage::Studio,
                             5000000, 5000000, 10, ncl_perf::PerfSampleState::Measured, "Model data preparation"};
    layout->Build(*snapshot, 1600, 1200, 2, nullptr, nullptr, 2);
    EXPECT_TRUE(HasText("CPU live: waiting for frame"));
    EXPECT_FALSE(HasText("CPU dump:"));
    EXPECT_FALSE(HasText("Top detail: Model data preparation"));
}
TEST_F(PerformancePanelTest, EmptyLiveStudioWindowShowsItsFrameCount)
{
    std::unique_ptr<performance::PerfSnapshot> captured = std::make_unique<performance::PerfSnapshot>();
    captured->cpu_frame_count = 1;
    captured->cpu_window_ns = 10000000;
    layout->Build(*snapshot, 1600, 1200, 2, nullptr, nullptr, 2, captured.get());
    EXPECT_TRUE(HasText("CPU live: averages, last 1 frames"));
    EXPECT_FALSE(HasText("CPU live: waiting for frame"));
    layout->Build(*snapshot, 640, 480, 2, nullptr, nullptr, 2, captured.get());
    EXPECT_TRUE(HasText("CPU live: no Studio samples"));
}

TEST_F(PerformancePanelTest, BatchPanelWorksIndependentlyOfPerformanceGraphs)
{
    for (int level : {1, 2})
    {
        for (int width : {640, 1600})
        {
            layout->Build(*snapshot, width, 480, 0, nullptr, nullptr, level);
            EXPECT_TRUE(HasText("Model draws"));
            EXPECT_TRUE(HasText("MultiDraw meshes"));
            EXPECT_FALSE(HasText("FPS"));
            EXPECT_FALSE(HasText("Monitor"));
            for (const performance::PerfPanelCommand& command : layout->get_commands())
            {
                EXPECT_LE(command.x + command.width, width);
                EXPECT_LE(command.y + command.height, 480);
                EXPECT_NE(command.kind, performance::PerfPanelCommandKind::Peak);
            }
        }
    }
}
TEST_F(PerformancePanelTest, IndependentBatchingCoexistsWithNetworkAndMapper)
{
    performance::PerfNetworkSnapshot network;
    network.enabled = true;
    performance::PerfMapperSnapshot mapper;
    mapper.enabled = mapper.map_loaded = true;
    for (int level : {1, 2})
    {
        for (int width : {640, 1600})
        {
            layout->Build(*snapshot, width, 1200, 0, &network, &mapper, level);
            EXPECT_TRUE(HasText("Model draws"));
            EXPECT_TRUE(HasText("Network"));
            EXPECT_TRUE(HasText("Map inspection"));
            EXPECT_FALSE(HasText("FPS"));
            for (const performance::PerfPanelCommand& command : layout->get_commands())
            {
                EXPECT_GE(command.x, 0);
                EXPECT_GE(command.y, 0);
                EXPECT_LE(command.x + command.width, width);
                EXPECT_LE(command.y + command.height, 1200);
            }
        }
    }
}

TEST_F(PerformancePanelTest, RefreshBatchingUsesLiveCountsAndPreservesOverviewCommands)
{
    performance::PerfSnapshot live;
    live.cpu_frame_count = 2;
    live.counters[uint32_t(performance::PerfCounterId::StudioPackets)] = 7;
    live.counters[uint32_t(performance::PerfCounterId::StudioSharedPackets)] = 7;
    snapshot->counters[uint32_t(performance::PerfCounterId::StudioPackets)] = 999;
    for (int mode : {0, 1, 2, 3})
    {
        for (int level : {1, 2})
        {
            for (int width : {640, 1600})
            {
                layout->Build(*snapshot, width, 900, mode, nullptr, nullptr, level, &live);
                EXPECT_TRUE(HasText("Studio batching | totals, last 2 frames"));
                EXPECT_TRUE(RowContains("Model draws", "7"));
                std::vector<performance::PerfPanelCommand> commands(layout->get_commands().begin(), layout->get_commands().end());
                live.cpu_frame_count = 3;
                live.counters[uint32_t(performance::PerfCounterId::StudioPackets)] = 9;
                live.counters[uint32_t(performance::PerfCounterId::StudioSharedPackets)] = 9;
                layout->RefreshBatching(*snapshot, &live);
                EXPECT_TRUE(HasText("Studio batching | totals, last 3 frames"));
                EXPECT_TRUE(RowContains("Model draws", "9"));
                ASSERT_EQ(layout->get_commands().size(), commands.size());
                for (uint32_t index = 0; index < commands.size(); ++index)
                {
                    const performance::PerfPanelCommand& before = commands[index];
                    const performance::PerfPanelCommand& after = layout->get_commands()[index];
                    EXPECT_EQ(before.kind, after.kind);
                    EXPECT_EQ(before.x, after.x);
                    EXPECT_EQ(before.y, after.y);
                    EXPECT_EQ(before.width, after.width);
                    EXPECT_EQ(before.height, after.height);
                    EXPECT_EQ(before.text_index, after.text_index);
                    EXPECT_EQ(before.vertex_y, after.vertex_y);
                    EXPECT_EQ(before.area, after.area);
                }
                live.cpu_frame_count = 2;
                live.counters[uint32_t(performance::PerfCounterId::StudioPackets)] = 7;
                live.counters[uint32_t(performance::PerfCounterId::StudioSharedPackets)] = 7;
            }
        }
    }
}

TEST_F(PerformancePanelTest, BatchingTablesHaveDistinctHeadersAndSeparatedSummaryRows)
{
    snapshot->cpu_frame_count = 1;
    layout->Build(*snapshot, 1600, 1200, 0, nullptr, nullptr, 2, snapshot.get());
    const performance::PerfPanelCommand* meshes = FindText("MultiDraw meshes");
    const performance::PerfPanelCommand* savings = FindText("Draw calls saved: -");
    const performance::PerfPanelCommand* cpu = FindText("CPU live: averages, last 1 frames");
    const performance::PerfPanelCommand* program = FindText("Program bind");
    const performance::PerfPanelCommand* other = FindText("Other Studio: -");
    ASSERT_NE(meshes, nullptr);
    ASSERT_NE(savings, nullptr);
    ASSERT_NE(cpu, nullptr);
    ASSERT_NE(program, nullptr);
    ASSERT_NE(other, nullptr);
    EXPECT_GT(savings->y, meshes->y + meshes->height);
    EXPECT_GT(cpu->y, savings->y + savings->height);
    EXPECT_GT(other->y, program->y + program->height);
    for (const char* title : {"Item", "CPU operation"})
    {
        const performance::PerfPanelCommand* header = FindText(title);
        ASSERT_NE(header, nullptr);
        bool background = false;
        for (const performance::PerfPanelCommand& command : layout->get_commands())
        {
            if (command.kind == performance::PerfPanelCommandKind::Rect && command.y == header->y && command.height == header->height)
            {
                background = true;
            }
        }
        EXPECT_TRUE(background) << title;
    }
}

TEST_F(PerformancePanelTest, NetworkPacketWindowNamesItsOrderAndSeparatesRatesFromSizes)
{
    performance::PerfNetworkSnapshot network;
    network.enabled = network.connected = true;
    network.latest[uint32_t(performance::PerfNetworkSeries::IncomingTraffic)] = {12.5, ncl_perf::PerfSampleState::Measured};
    network.latest[uint32_t(performance::PerfNetworkSeries::IncomingPackets)] = {100, ncl_perf::PerfSampleState::Measured};
    network.graphs[uint32_t(performance::PerfNetworkSeries::IncomingTraffic)][255] = {1536, 1536, 1};
    layout->Build(*snapshot, 1600, 1200, 0, &network);

    EXPECT_TRUE(HasText("Network | last 256 packets"));
    EXPECT_TRUE(HasText("Packet size (B)"));
    EXPECT_TRUE(HasText("In 12.5 KiB/s 100.0 pps"));
    EXPECT_FALSE(HasText("Traffic (KiB/s)"));
    EXPECT_FALSE(HasText("History"));
}

TEST_F(PerformancePanelTest, NetworkStatisticsDescribeVisiblePackets)
{
    performance::PerfNetworkSnapshot network;
    network.enabled = network.connected = true;
    network.packet_summaries[uint32_t(performance::PerfNetworkSeries::Latency)] = {25, 20, 40, 3};
    network.packet_summaries[uint32_t(performance::PerfNetworkSeries::IncomingTraffic)] = {1200, 1000, 1536, 3};
    network.packet_summaries[uint32_t(performance::PerfNetworkSeries::OutgoingTraffic)] = {80, 64, 128, 3};
    network.summaries[uint32_t(performance::PerfNetworkSeries::Latency)] = {999, 999, 999, 1};
    network.summaries[uint32_t(performance::PerfNetworkSeries::IncomingTraffic)] = {10, 10, 10, 1};
    network.summaries[uint32_t(performance::PerfNetworkSeries::OutgoingTraffic)] = {5, 5, 5, 1};
    network.incoming_packet_count = 3;
    network.outgoing_packet_count = 3;
    layout->Build(*snapshot, 1600, 1200, 0, &network);

    EXPECT_NE(FindText("25.00"), nullptr);
    EXPECT_NE(FindText("20.00"), nullptr);
    EXPECT_NE(FindText("40.00"), nullptr);
    EXPECT_TRUE(RowContains("In", "1200.00"));
    EXPECT_TRUE(RowContains("In", "1000.00"));
    EXPECT_TRUE(RowContains("In", "1536.00"));
    EXPECT_TRUE(RowContains("Out", "80.00"));
    EXPECT_TRUE(RowContains("Out", "64.00"));
    EXPECT_TRUE(RowContains("Out", "128.00"));
    EXPECT_FALSE(HasText("999.00"));
    EXPECT_TRUE(HasText("Packets: In 3 | Out 3"));
}

TEST_F(PerformancePanelTest, PacketColumnsKeepNewestPacketsOnTheRightWithoutMeanMarks)
{
    performance::PerfNetworkSnapshot network;
    network.enabled = true;
    network.graphs[uint32_t(performance::PerfNetworkSeries::Latency)][254] = {40, 40, 1};
    network.graphs[uint32_t(performance::PerfNetworkSeries::Latency)][255] = {80, 80, 1};
    for (int mode : {0, 2, 3})
    {
        for (int width : {640, 1600})
        {
            layout->Build(*snapshot, width, 1200, mode, &network);
            std::vector<performance::PerfPanelCommand> peaks;
            std::vector<performance::PerfPanelCommand> means;
            for (const performance::PerfPanelCommand& command : layout->get_commands())
            {
                if ((command.color & 0xffffff00) != 0xd8cf7a00)
                {
                    continue;
                }
                if (command.kind == performance::PerfPanelCommandKind::Peak)
                {
                    peaks.push_back(command);
                }
                if (command.kind == performance::PerfPanelCommandKind::Mean)
                {
                    means.push_back(command);
                }
            }

            ASSERT_EQ(peaks.size(), 2u);
            ASSERT_TRUE(means.empty());
            EXPECT_GT(peaks[1].x, peaks[0].x);
            EXPECT_LE(peaks[1].x - peaks[0].x, 3);
            EXPECT_GT(peaks[0].vertex_y[0], peaks[0].vertex_y[1]);
            for (const performance::PerfPanelCommand& peak : peaks)
            {
                EXPECT_LE(peak.width, 3);
            }
            bool reaches_right_edge = false;
            for (const performance::PerfPanelCommand& command : layout->get_commands())
            {
                if (command.kind == performance::PerfPanelCommandKind::Rect && (command.color & 0xffffff00) == 0x06091100 &&
                    command.y == peaks[1].y)
                {
                    reaches_right_edge = peaks[1].x >= command.x + command.width - 4;
                }
            }
            EXPECT_TRUE(reaches_right_edge);
        }
    }
}

TEST_F(PerformancePanelTest, NetworkRefreshPreservesOtherSectionsAcrossPacketCountChanges)
{
    struct RetainedCommand
    {
        performance::PerfPanelCommand command;
        std::string text;
    };

    snapshot->panel_scale = 1;
    snapshot->graphs[uint32_t(performance::PerfSeries::HostCpu)][100] = {1000000, 500000, 1};
    performance::PerfNetworkSnapshot network;
    network.enabled = network.connected = true;
    performance::PerfMapperSnapshot mapper;
    mapper.enabled = mapper.map_loaded = mapper.view_measured = true;
    mapper.wpoly = 1234;
    performance::PerfSnapshot live;
    live.cpu_frame_count = 2;
    live.counters[uint32_t(performance::PerfCounterId::StudioPackets)] = 7;
    live.counters[uint32_t(performance::PerfCounterId::StudioSharedPackets)] = 7;
    for (int mode : {0, 1, 2, 3})
    {
        for (int width : {640, 1600})
        {
            layout->Build(*snapshot, width, 1800, mode, &network, &mapper, 2, &live);
            ASSERT_NE(FindText("Network | last 256 packets"), nullptr);
            ASSERT_NE(FindText("Map inspection"), nullptr);
            int network_x = FindText("Network | last 256 packets")->x;
            int network_y = FindText("Network | last 256 packets")->y;
            performance::PerfPanelCommand network_bounds;
            for (const performance::PerfPanelCommand& command : layout->get_commands())
            {
                if (command.kind == performance::PerfPanelCommandKind::Rect && (command.color & 0xffffff00) == 0x11131d00 &&
                    command.x <= network_x && network_x < command.x + command.width && command.y <= network_y && network_y < command.y + command.height)
                {
                    network_bounds = command;
                    break;
                }
            }
            ASSERT_GT(network_bounds.width, 0);
            auto capture = [&] {
                std::vector<RetainedCommand> retained;
                for (const performance::PerfPanelCommand& command : layout->get_commands())
                {
                    if (command.x >= network_bounds.x && command.x < network_bounds.x + network_bounds.width && command.y >= network_y &&
                        command.y < network_bounds.y + network_bounds.height)
                    {
                        continue;
                    }
                    retained.push_back({command, command.kind == performance::PerfPanelCommandKind::Text ? layout->get_text(command) : ""});
                }
                return retained;
            };
            std::vector<RetainedCommand> retained = capture();
            for (uint32_t count : {0u, 256u, 1u, 0u})
            {
                for (std::vector<performance::PerfPlotBin>& graph : network.graphs)
                {
                    std::fill(graph.begin(), graph.end(), performance::PerfPlotBin{});
                }
                for (uint32_t index = 256 - count; index < 256; ++index)
                {
                    network.graphs[uint32_t(performance::PerfNetworkSeries::Latency)][index] = {40, 40, 1};
                    network.graphs[uint32_t(performance::PerfNetworkSeries::IncomingTraffic)][index] = {1200, 1200, 1};
                    network.graphs[uint32_t(performance::PerfNetworkSeries::OutgoingTraffic)][index] = {80, 80, 1};
                }
                network.incoming_packet_count = network.outgoing_packet_count = count;
                layout->RefreshNetwork(network);
                std::vector<RetainedCommand> refreshed = capture();
                ASSERT_EQ(refreshed.size(), retained.size()) << mode << ": " << width << ": " << count;
                for (uint32_t index = 0; index < retained.size(); ++index)
                {
                    const performance::PerfPanelCommand& before = retained[index].command;
                    const performance::PerfPanelCommand& after = refreshed[index].command;
                    EXPECT_EQ(before.kind, after.kind);
                    EXPECT_EQ(before.x, after.x);
                    EXPECT_EQ(before.y, after.y);
                    EXPECT_EQ(before.width, after.width);
                    EXPECT_EQ(before.height, after.height);
                    EXPECT_EQ(before.color, after.color);
                    EXPECT_EQ(before.vertex_y, after.vertex_y);
                    EXPECT_EQ(before.area, after.area);
                    EXPECT_EQ(retained[index].text, refreshed[index].text);
                }
                uint32_t packet_means = 0;
                for (const performance::PerfPanelCommand& command : layout->get_commands())
                {
                    packet_means += command.kind == performance::PerfPanelCommandKind::Mean &&
                                    ((command.color & 0xffffff00) == 0xd8cf7a00 || (command.color & 0xffffff00) == 0x65cfff00 ||
                                     (command.color & 0xffffff00) == 0x86de8600);
                    EXPECT_LE(command.x + command.width, width);
                    EXPECT_LE(command.y + command.height, 1800);
                }
                EXPECT_EQ(packet_means, 0u);
                live.cpu_frame_count += 1;
                live.counters[uint32_t(performance::PerfCounterId::StudioPackets)] = 9;
                live.counters[uint32_t(performance::PerfCounterId::StudioSharedPackets)] = 9;
                layout->RefreshBatching(*snapshot, &live);
                EXPECT_TRUE(RowContains("Model draws", "9"));
                EXPECT_TRUE(HasText("wpoly 1234"));
                retained = capture();
            }
        }
    }
}

TEST_F(PerformancePanelTest, CompleteGraphsKeepAdjacentColumnsOnAUniformPixelGrid)
{
    performance::PerfNetworkSnapshot network;
    network.enabled = network.connected = true;
    for (uint32_t index = 0; index < performance::kPerfGraphCapacity; ++index)
    {
        snapshot->graphs[uint32_t(performance::PerfSeries::HostCpu)][index] = {8000000, 4000000, 1};
        network.graphs[uint32_t(performance::PerfNetworkSeries::Latency)][index] = {40, 40, 1};
    }
    for (float scale : {1.0f, 0.75f, 1.3f, 2.5f})
    {
        snapshot->panel_scale = scale;
        for (int mode : {0, 2, 3})
        {
            layout->Build(*snapshot, 1600, 1200, mode, mode ? nullptr : &network);
            std::vector<performance::PerfPanelCommand> columns;
            uint32_t color = mode ? 0xe6ab6300 : 0xd8cf7a00;
            for (const performance::PerfPanelCommand& command : layout->get_commands())
            {
                if (command.kind == performance::PerfPanelCommandKind::Peak && (command.color & 0xffffff00) == color)
                {
                    columns.push_back(command);
                }
            }
            ASSERT_EQ(columns.size(), performance::kPerfGraphCapacity) << mode << ": " << scale;
            const performance::PerfPanelCommand* plot = nullptr;
            for (const performance::PerfPanelCommand& command : layout->get_commands())
            {
                if (command.kind == performance::PerfPanelCommandKind::Rect && (command.color & 0xffffff00) == 0x06091100 &&
                    command.y == columns.front().y && command.height == columns.front().height)
                {
                    plot = &command;
                    break;
                }
            }
            ASSERT_NE(plot, nullptr) << mode << ": " << scale;
            EXPECT_EQ(plot->x, columns.front().x) << mode << ": " << scale;
            EXPECT_EQ(plot->x + plot->width, columns.back().x + columns.back().width) << mode << ": " << scale;
            uint32_t axes = 0;
            uint32_t grid_lines = 0;
            for (const performance::PerfPanelCommand& command : layout->get_commands())
            {
                if (command.kind != performance::PerfPanelCommandKind::Rect || command.y < plot->y || command.y >= plot->y + plot->height)
                {
                    continue;
                }
                if (command.color == 0xc0c6d270 && command.height == plot->height)
                {
                    EXPECT_EQ(command.x + command.width, plot->x);
                    ++axes;
                }
                if (command.color == 0xc0c6d228)
                {
                    EXPECT_EQ(command.x, plot->x);
                    EXPECT_EQ(command.width, plot->width);
                    ++grid_lines;
                }
            }
            EXPECT_EQ(axes, 1u) << mode << ": " << scale;
            EXPECT_EQ(grid_lines, 3u) << mode << ": " << scale;
            int step = columns[1].x - columns[0].x;
            ASSERT_EQ(step, columns[0].width) << mode << ": " << scale;
            for (uint32_t index = 1; index < columns.size(); ++index)
            {
                ASSERT_EQ(columns[index].width, columns[0].width) << mode << ": " << scale << ": " << index;
                ASSERT_EQ(columns[index].x - columns[index - 1].x, step) << mode << ": " << scale << ": " << index;
            }
        }
    }
}

TEST_F(PerformancePanelTest, NarrowPixelGridClipsOnlyTheOldestColumns)
{
    snapshot->panel_scale = 0.5f;
    performance::PerfNetworkSnapshot network;
    network.enabled = network.connected = true;
    for (performance::PerfPlotBin& bin : network.graphs[uint32_t(performance::PerfNetworkSeries::Latency)])
    {
        bin = {40, 40, 1};
    }
    layout->Build(*snapshot, 1600, 1200, 0, &network);
    std::vector<performance::PerfPanelCommand> columns;
    const performance::PerfPanelCommand* plot = nullptr;
    for (const performance::PerfPanelCommand& command : layout->get_commands())
    {
        if (command.kind == performance::PerfPanelCommandKind::Peak && (command.color & 0xffffff00) == 0xd8cf7a00)
        {
            columns.push_back(command);
        }
        if (!plot && command.kind == performance::PerfPanelCommandKind::Rect && (command.color & 0xffffff00) == 0x06091100)
        {
            plot = &command;
        }
    }
    ASSERT_NE(plot, nullptr);
    ASSERT_FALSE(columns.empty());
    EXPECT_EQ(columns.size(), plot->width);
    EXPECT_LT(columns.size(), performance::kPerfGraphCapacity);
    EXPECT_EQ(columns.front().column_index, performance::kPerfGraphCapacity - columns.size());
    EXPECT_EQ(columns.back().column_index, performance::kPerfGraphCapacity - 1);
    EXPECT_EQ(columns.front().x, plot->x);
    EXPECT_EQ(columns.back().x + columns.back().width, plot->x + plot->width);
    for (const performance::PerfPanelCommand& command : columns)
    {
        EXPECT_EQ(command.width, 1);
    }
}

TEST_F(PerformancePanelTest, LossAndChokeMarkersShareTheLatencyPixelGrid)
{
    performance::PerfNetworkSnapshot network;
    network.enabled = network.connected = true;
    network.graphs[uint32_t(performance::PerfNetworkSeries::Latency)][255] = {40, 40, 1};
    network.graphs[uint32_t(performance::PerfNetworkSeries::Loss)][253] = {100, 100, 1};
    network.graphs[uint32_t(performance::PerfNetworkSeries::Choke)][254] = {100, 100, 1};
    for (float scale : {0.5f, 1.0f, 1.3f, 2.5f})
    {
        snapshot->panel_scale = scale;
        layout->Build(*snapshot, 1600, 1200, 0, &network);
        const performance::PerfPanelCommand* latency = nullptr;
        const performance::PerfPanelCommand* loss = nullptr;
        const performance::PerfPanelCommand* choke = nullptr;
        for (const performance::PerfPanelCommand& command : layout->get_commands())
        {
            uint32_t color = command.color & 0xffffff00;
            if (command.kind == performance::PerfPanelCommandKind::Peak && color == 0xd8cf7a00)
            {
                latency = &command;
            }
            if (command.kind == performance::PerfPanelCommandKind::Rect && color == 0xff646400)
            {
                loss = &command;
            }
            if (command.kind == performance::PerfPanelCommandKind::Rect && color == 0xffae5400)
            {
                choke = &command;
            }
        }
        ASSERT_NE(latency, nullptr);
        ASSERT_NE(loss, nullptr);
        ASSERT_NE(choke, nullptr);
        EXPECT_EQ(loss->width, latency->width);
        EXPECT_EQ(choke->width, latency->width);
        EXPECT_EQ(loss->x, latency->x - 2 * latency->width);
        EXPECT_EQ(choke->x, latency->x - latency->width);
    }
}

TEST_F(PerformancePanelTest, NetworkWindowFillsTheAvailablePhysicalWidthWithOnePixelPerPacket)
{
    performance::PerfNetworkSnapshot network;
    network.enabled = network.connected = true;
    for (float scale : {0.5f, 0.75f, 1.0f, 1.3f, 2.5f, 3.0f})
    {
        snapshot->panel_scale = scale;
        for (int width : {640, 1600})
        {
            for (int mode : {0, 1, 2, 3})
            {
                uint32_t count = performance::PerformancePanelLayout::NetworkColumnCount(scale, width, 6900);
                if (!performance::PerformancePanelLayout::CanShowNetwork(scale, width, 6900))
                {
                    EXPECT_EQ(count, 0u);
                    continue;
                }
                ASSERT_GT(count, 0u);
                for (std::vector<performance::PerfPlotBin>& graph : network.graphs)
                {
                    graph.assign(count, performance::PerfPlotBin{});
                }
                std::fill(
                    network.graphs[uint32_t(performance::PerfNetworkSeries::Latency)].begin(),
                    network.graphs[uint32_t(performance::PerfNetworkSeries::Latency)].end(),
                    performance::PerfPlotBin{40, 40, 1}
                );
                layout->Build(*snapshot, width, 6900, mode, &network);
                std::string title = "Network | last " + std::to_string(count) + " packets";
                ASSERT_NE(FindText(title.c_str()), nullptr);
                std::vector<performance::PerfPanelCommand> columns;
                const performance::PerfPanelCommand* plot = nullptr;
                for (const performance::PerfPanelCommand& command : layout->get_commands())
                {
                    if (command.kind == performance::PerfPanelCommandKind::Peak && (command.color & 0xffffff00) == 0xd8cf7a00)
                    {
                        columns.push_back(command);
                    }
                }
                ASSERT_EQ(columns.size(), count) << scale << ": " << width << ": " << mode;
                for (const performance::PerfPanelCommand& command : layout->get_commands())
                {
                    if (command.kind == performance::PerfPanelCommandKind::Rect && (command.color & 0xffffff00) == 0x06091100 &&
                        command.y == columns.front().y && command.height == columns.front().height)
                    {
                        plot = &command;
                        break;
                    }
                }
                ASSERT_NE(plot, nullptr);
                EXPECT_EQ(plot->width, int(count));
                EXPECT_EQ(plot->x, columns.front().x);
                EXPECT_EQ(plot->x + plot->width, columns.back().x + 1);
                for (uint32_t index = 0; index < count; ++index)
                {
                    EXPECT_EQ(columns[index].width, 1);
                    EXPECT_EQ(columns[index].x, plot->x + int(index));
                    EXPECT_EQ(columns[index].column_index, index);
                }
            }
        }
    }
}

TEST_F(PerformancePanelTest, MaximumScaleRetainsAllSectionsWhenThePacketWindowIsFull)
{
    snapshot->panel_scale = 3;
    performance::PerfNetworkSnapshot network;
    network.enabled = network.connected = true;
    uint32_t count = performance::PerformancePanelLayout::NetworkColumnCount(3, 4000, 6000);
    performance::PerfFrameGraphs frames;
    frames.frames.resize(count);
    for (performance::PerfFrameGraph& frame : frames.frames)
    {
        for (performance::PerfValue& value : frame.values)
        {
            value = {2000000, ncl_perf::PerfSampleState::Measured};
        }
        frame.values[uint32_t(performance::PerfSeries::HostCpu)].duration_ns = 8000000;
        frame.values[uint32_t(performance::PerfSeries::RenderGpu)].duration_ns = 6000000;
    }
    auto fill = [&](bool available) {
        for (std::vector<performance::PerfPlotBin>& graph : network.graphs)
        {
            graph.assign(count, performance::PerfPlotBin{});
        }
        for (performance::PerfNetworkSeries series :
             {performance::PerfNetworkSeries::Latency,
              performance::PerfNetworkSeries::IncomingTraffic,
              performance::PerfNetworkSeries::OutgoingTraffic})
        {
            std::fill(
                network.graphs[uint32_t(series)].begin(),
                network.graphs[uint32_t(series)].end(),
                available ? performance::PerfPlotBin{40, 40, 1} : performance::PerfPlotBin{}
            );
        }
    };
    fill(true);
    performance::PerfMapperSnapshot mapper;
    mapper.enabled = mapper.map_loaded = mapper.view_measured = true;
    layout->Build(*snapshot, 4000, 6000, 3, &network, &mapper, 2, nullptr, nullptr, &frames);
    ASSERT_TRUE(HasText("Detailed counters: perf_dump"));
    EXPECT_GT(layout->get_commands().size(), 8192u);
    size_t full_count = layout->get_commands().size();

    fill(false);
    layout->Build(*snapshot, 4000, 6000, 3, &network, &mapper, 2, nullptr, nullptr, &frames);
    fill(true);
    layout->RefreshNetwork(network);
    EXPECT_EQ(layout->get_commands().size(), full_count);
    EXPECT_TRUE(HasText("Detailed counters: perf_dump"));
    EXPECT_TRUE(HasText("Map inspection"));
}

TEST_F(PerformancePanelTest, PerFramePlotsFillThePhysicalWidthWithoutCombiningSamples)
{
    performance::PerfFrameGraphs frames;
    for (float scale : {0.5f, 1.0f, 1.3f, 3.0f})
    {
        snapshot->panel_scale = scale;
        for (int mode : {2, 3})
        {
            uint32_t count = performance::PerformancePanelLayout::GraphColumnCount(scale, 4000, 6900, mode);
            ASSERT_GT(count, 256u * (scale >= 1));
            frames.frames.assign(count, performance::PerfFrameGraph{});
            for (performance::PerfFrameGraph& frame : frames.frames)
            {
                for (performance::PerfValue& value : frame.values)
                {
                    value = {1000000, ncl_perf::PerfSampleState::Measured};
                }
                frame.values[uint32_t(performance::PerfSeries::FrameInterval)].duration_ns = 10000000;
                frame.values[uint32_t(performance::PerfSeries::HostCpu)].duration_ns = 8000000;
                frame.values[uint32_t(performance::PerfSeries::RenderGpu)].duration_ns = 6000000;
            }
            layout->Build(*snapshot, 4000, 6900, mode, nullptr, nullptr, 0, nullptr, nullptr, &frames);
            EXPECT_TRUE(HasText(("History | last " + std::to_string(count) + " frames").c_str()));
            for (uint32_t color : {0xe6ab6300u, 0xff707000u})
            {
                std::vector<performance::PerfPanelCommand> columns;
                const performance::PerfPanelCommand* plot = nullptr;
                for (const performance::PerfPanelCommand& command : layout->get_commands())
                {
                    if (command.kind == performance::PerfPanelCommandKind::Peak && (command.color & 0xffffff00) == color)
                    {
                        columns.push_back(command);
                    }
                }
                ASSERT_EQ(columns.size(), count) << mode << ": " << scale;
                for (const performance::PerfPanelCommand& command : layout->get_commands())
                {
                    if (command.kind == performance::PerfPanelCommandKind::Rect && (command.color & 0xffffff00) == 0x06091100 &&
                        command.y <= columns.front().y && command.y + command.height == columns.front().y + columns.front().height)
                    {
                        plot = &command;
                        break;
                    }
                }
                ASSERT_NE(plot, nullptr);
                EXPECT_EQ(plot->width, int(count));
                EXPECT_EQ(plot->x, columns.front().x);
                EXPECT_EQ(plot->x + plot->width, columns.back().x + 1);
                for (uint32_t index = 0; index < count; ++index)
                {
                    EXPECT_EQ(columns[index].width, 1);
                    EXPECT_EQ(columns[index].x, plot->x + int(index));
                }
            }
        }
    }
}

TEST_F(PerformancePanelTest, PerFrameValuesAndPendingGpuKeepDistinctFramePositions)
{
    snapshot->panel_scale = 1;
    uint32_t count = performance::PerformancePanelLayout::GraphColumnCount(1, 1600, 1200, 2);
    performance::PerfFrameGraphs frames;
    frames.frames.resize(count);
    for (uint32_t index = count - 3; index < count; ++index)
    {
        performance::PerfFrameGraph& frame = frames.frames[index];
        frame.values[uint32_t(performance::PerfSeries::FrameInterval)] = {10000000, ncl_perf::PerfSampleState::Measured};
        frame.values[uint32_t(performance::PerfSeries::HostCpu)] = {
            (index == count - 2 ? 8u : 2u) * 1000000, ncl_perf::PerfSampleState::Measured
        };
        frame.values[uint32_t(performance::PerfSeries::RenderGpu)] = {
            3000000, index == count - 2 ? ncl_perf::PerfSampleState::Pending : ncl_perf::PerfSampleState::Measured
        };
    }
    for (int mode : {2, 3})
    {
        layout->Build(*snapshot, 1600, 1200, mode, nullptr, nullptr, 0, nullptr, nullptr, &frames);
        std::vector<performance::PerfPanelCommand> hosts;
        std::vector<performance::PerfPanelCommand> gpu;
        for (const performance::PerfPanelCommand& command : layout->get_commands())
        {
            if (command.kind == performance::PerfPanelCommandKind::Peak && (command.color & 0xffffff00) == 0xe6ab6300)
            {
                hosts.push_back(command);
            }
            if (command.kind == performance::PerfPanelCommandKind::Peak && (command.color & 0xffffff00) == 0xff707000)
            {
                gpu.push_back(command);
            }
        }
        ASSERT_EQ(hosts.size(), 3u);
        ASSERT_EQ(gpu.size(), 2u);
        for (const performance::PerfPanelCommand& column : hosts)
        {
            EXPECT_EQ(column.vertex_y[0], column.vertex_y[1]);
            EXPECT_EQ(column.vertex_y[2], column.vertex_y[3]);
        }
        EXPECT_EQ(hosts[0].column_index, count - 3);
        EXPECT_EQ(hosts[1].column_index, count - 2);
        EXPECT_EQ(hosts[2].column_index, count - 1);
        EXPECT_GT(hosts[0].vertex_y[0], hosts[1].vertex_y[0]);
        EXPECT_EQ(hosts[0].vertex_y[0], hosts[2].vertex_y[0]);
        EXPECT_EQ(gpu[0].column_index, count - 3);
        EXPECT_EQ(gpu[1].column_index, count - 1);
        EXPECT_EQ(gpu[1].x - gpu[0].x, 2);
    }
}

TEST_F(PerformancePanelTest, EveryPerformanceModeNamesTheSharedFrameWindow)
{
    snapshot->coverage_ns = 10000000000;
    snapshot->panel_scale = 1;
    performance::PerfFrameGraphs frames;
    for (int mode : {1, 2, 3})
    {
        uint32_t count = performance::PerformancePanelLayout::GraphColumnCount(1, 1600, 1200, mode);
        frames.frames.resize(count);
        layout->Build(*snapshot, 1600, 1200, mode, nullptr, nullptr, 0, nullptr, nullptr, &frames);
        EXPECT_TRUE(HasText(("History | last " + std::to_string(count) + " frames").c_str()));
        EXPECT_FALSE(HasText("10.00 s"));
    }
}

TEST_F(PerformancePanelTest, CyrillicTitleHasTheSameCharacterCapacityAsAscii)
{
    performance::PerfPanelLabels labels;
    std::snprintf(labels.title, sizeof(labels.title), "1234567890123456789012345678901234567890");
    layout->SetLabels(labels);
    layout->Build(*snapshot, 180, 240, 1);

    size_t ascii_characters = 0;
    for (const performance::PerfPanelCommand& command : layout->get_commands())
    {
        if (command.kind == performance::PerfPanelCommandKind::Text)
        {
            ascii_characters = std::strlen(layout->get_text(command));
            break;
        }
    }
    ASSERT_GT(ascii_characters, 0u);
    ASSERT_LT(ascii_characters, 40u);

    std::string cyrillic;
    for (int index = 0; index < 40; ++index)
    {
        cyrillic += "\xd0\x9f";
    }
    std::snprintf(labels.title, sizeof(labels.title), "%s", cyrillic.c_str());
    layout->SetLabels(labels);
    layout->Build(*snapshot, 180, 240, 1);

    for (const performance::PerfPanelCommand& command : layout->get_commands())
    {
        if (command.kind == performance::PerfPanelCommandKind::Text)
        {
            EXPECT_EQ(std::string(layout->get_text(command)), cyrillic.substr(0, ascii_characters * 2));
            return;
        }
    }
    FAIL() << "No title command";
}

TEST_F(PerformancePanelTest, CatalogControlsTimingLabelsOrderIndentationColorsAndFooterVisibility)
{
    layout->Build(*snapshot, 1920, 1600, 1);

    for (performance::SeriesPlacement placement : {performance::SeriesPlacement::Cpu, performance::SeriesPlacement::Gpu})
    {
        int previous_y = -1;

        for (uint32_t order = 0; order < performance::CountSeries(placement); ++order)
        {
            const performance::SeriesDescriptor* series = performance::OrderedSeries(placement, order);
            ASSERT_NE(series, nullptr);
            std::string label(series->presentation.indentation * 2, ' ');
            label += series->presentation.label;
            const performance::PerfPanelCommand* command = FindText(label.c_str());
            ASSERT_NE(command, nullptr) << series->name;
            EXPECT_EQ(command->color, series->presentation.color);
            EXPECT_GT(command->y, previous_y);
            previous_y = command->y;
        }
    }

    EXPECT_EQ(FindText("Monitor CPU"), nullptr);
    const performance::PerfPanelCommand* footer = FindText("Monitor Unavailable | GPU age", false);
    ASSERT_NE(footer, nullptr);
    EXPECT_EQ(footer->color, performance::Series(performance::PerfSeries::MonitorCpu)->presentation.color);
    EXPECT_EQ(performance::CountOverviewSeries(), 8u);
}

TEST_F(PerformancePanelTest, StudioPresentationFlagsSelectOtherCostsAndTheLargestDetail)
{
    snapshot->cpu_frame_count = 1;
    snapshot->cpu_count = 4;
    const ncl_perf::PerfScopeId_t ids[] = {
        performance::PerfStage::Studio,
        performance::PerfStage::StudioDataPrepare,
        performance::PerfStage::StudioModelStateCapture,
        performance::PerfStage::ShaderLink
    };
    const ncl_perf::PerfDuration_t durations[] = {9000000, 2000000, 3000000, 90000000};

    for (uint32_t index = 0; index < snapshot->cpu_count; ++index)
    {
        ncl_perf::PerfCpuSample& sample = snapshot->cpu_tree[index];
        sample.scope_id = ids[index];
        sample.inclusive_ns = durations[index];
        sample.self_ns = index ? 0 : 1000000;
        sample.calls = 1;
        sample.state = ncl_perf::PerfSampleState::Measured;
    }

    layout->Build(*snapshot, 1600, 1200, 0, nullptr, nullptr, 2, snapshot.get());
    EXPECT_TRUE(HasText("Other Studio: 3.000 ms/frame"));
    EXPECT_TRUE(HasText("Top detail: Model state capture 3.000 ms/frame"));
}

TEST_F(PerformancePanelTest, CatalogFallbackUsesOnlyDeclaredOverviewTotals)
{
    for (const performance::SeriesDescriptor& series : performance::kSeries)
    {
        snapshot->graphs[uint32_t(series.id)].back() = {1000000, 1000000, 1};
    }

    layout->Build(*snapshot, 1920, 1600, 2);

    for (const performance::SeriesDescriptor& series : performance::kSeries)
    {
        bool drawn = false;

        for (const performance::PerfPanelCommand& command : layout->get_commands())
        {
            drawn = drawn || (command.kind == performance::PerfPanelCommandKind::Peak &&
                              (command.color & 0xffffff00) == (series.presentation.color & 0xffffff00));
        }

        EXPECT_EQ(drawn, series.presentation.fallback) << series.name;
    }
}

TEST_F(PerformancePanelTest, GaugeStudioRowUsesPacketUnitsAndSelectedWindowMaximum)
{
    snapshot->gauges[0] = {0, ncl_perf::PerfSampleState::Measured};
    snapshot->gauge_summaries[0] = {3, 0, 5, 12, ncl_perf::PerfSampleState::Measured};
    performance::PerfSnapshot batch;
    batch.cpu_frame_count = 8;
    batch.gauges[0] = snapshot->gauges[0];
    batch.gauge_summaries[0] = {4, 0, 3, 7, ncl_perf::PerfSampleState::Measured};
    for (int level : {1, 2})
    {
        layout->Build(*snapshot, 1600, 1200, 0, nullptr, nullptr, level, &batch);
        EXPECT_TRUE(HasText("Queue peak (last traversal)"));
        EXPECT_TRUE(HasText("last=0 max=7 packets"));
        EXPECT_FALSE(HasText("max=12 packets"));
        EXPECT_FALSE(HasText("max=7 ms"));
    }
    snapshot->gauges[0].state = ncl_perf::PerfSampleState::Inactive;
    batch.gauges[0].state = ncl_perf::PerfSampleState::Inactive;
    layout->RefreshBatching(*snapshot, &batch);
    EXPECT_TRUE(HasText("last=Inactive max=7 packets"));
    EXPECT_FALSE(HasText("last=0 max=7 packets"));
}
