#include "performance/PerformanceSnapshotPublisher.h"

#include <algorithm>
#include <cassert>

namespace performance
{
    namespace
    {
        constexpr ncl_perf::PerfDuration_t kRefreshDuration = 250000000;
        constexpr ncl_perf::PerfDuration_t kPanelFpsWindow = 500000000;
    } // namespace
    void PerfSnapshotPublisher::CompleteCpuFrame(
        PerfFrameId_t frame_id,
        std::span<const ncl_perf::PerfCpuSample> samples,
        ncl_perf::PerfTimestamp_t completed_at
    )
    {
        assert(samples.size() <= pending_tree_.size());
        assert(completed_at >= completed_at_);

        if (!pending_frame_count_)
        {
            std::copy(samples.begin(), samples.end(), pending_tree_.begin());
            pending_tree_count_ = uint32_t(samples.size());
        }
        else
        {
            assert(samples.size() == pending_tree_count_);

            for (uint32_t index = 0; index < samples.size(); ++index)
            {
                const ncl_perf::PerfCpuSample& sample = samples[index];
                ncl_perf::PerfCpuSample& total = pending_tree_[index];

                if (!sample.calls)
                {
                    continue;
                }

                if (!total.calls)
                {
                    total.parent_id = sample.parent_id;
                }
                else if (total.parent_id != sample.parent_id)
                {
                    total.parent_id = 0;
                }

                total.inclusive_ns += sample.inclusive_ns;
                total.self_ns += sample.self_ns;
                total.calls += sample.calls;

                if (total.state != ncl_perf::PerfSampleState::Incomplete)
                {
                    total.state = sample.state;
                }
            }
        }

        ++pending_frame_count_;
        completed_frame_id_ = frame_id;
        completed_at_ = completed_at;
    }

    void PerfSnapshotPublisher::Advance(ncl_perf::PerfTimestamp_t now, const PerfFrameHistory& history)
    {
        assert(now >= numeric_start_);
        assert(now >= completed_at_);

        bool numeric_due = now - numeric_start_ >= kRefreshDuration;
        bool initial_frame = !numeric_published_ && pending_frame_count_ != 0;
        bool history_due = !collection_published_ || history_dirty_ || now - snapshot_->captured_at >= kRefreshDuration;

        if (numeric_due || initial_frame)
        {
            numeric_tree_ = pending_tree_;
            numeric_tree_count_ = pending_tree_count_;
            numeric_frame_count_ = pending_frame_count_;
            numeric_frame_id_ = completed_frame_id_;
            numeric_window_ns_ = now - numeric_start_;
            numeric_published_ = numeric_published_ || pending_frame_count_ != 0;
            pending_tree_ = {};
            pending_tree_count_ = 0;
            pending_frame_count_ = 0;
            numeric_start_ = now;
        }

        if (history_due)
        {
            history.BuildSnapshot(now, *snapshot_, &grouped_graphs_);
            panel_fps_ = history.CalculateFps(now, kPanelFpsWindow);
            collection_published_ = collection_published_ || snapshot_->record_count != 0;
            history_dirty_ = false;
        }

        if (numeric_due || initial_frame || history_due)
        {
            snapshot_->cpu_tree = numeric_tree_;
            snapshot_->cpu_count = numeric_tree_count_;
            snapshot_->cpu_frame_id = numeric_frame_id_;
            snapshot_->cpu_frame_count = numeric_frame_count_;
            snapshot_->cpu_window_ns = numeric_window_ns_;
            ++revision_;
        }
    }

    void PerfSnapshotPublisher::InvalidateHistory()
    {
        history_dirty_ = true;
    }

    void PerfSnapshotPublisher::Reset(uint32_t generation, ncl_perf::PerfTimestamp_t now)
    {
        pending_tree_ = {};
        numeric_tree_ = {};
        pending_tree_count_ = 0;
        pending_frame_count_ = 0;
        numeric_tree_count_ = 0;
        numeric_frame_count_ = 0;
        completed_frame_id_ = 0;
        numeric_frame_id_ = 0;
        numeric_window_ns_ = 0;
        numeric_start_ = now;
        completed_at_ = now;
        panel_fps_ = 0;
        numeric_published_ = false;
        collection_published_ = false;
        history_dirty_ = true;
        *snapshot_ = {};
        snapshot_->captured_at = now;
        snapshot_->collector_generation = generation;
        grouped_graphs_ = {};
        ++revision_;
    }

    void PerfSnapshotPublisher::CopySnapshot(PerfSnapshot& output) const
    {
        output = *snapshot_;
    }

    void PerfSnapshotPublisher::CopyGroupedGraphs(PerfGroupedGraphs& output) const
    {
        output = grouped_graphs_;
    }

    double PerfSnapshotPublisher::get_panel_fps() const
    {
        return panel_fps_;
    }

    uint64_t PerfSnapshotPublisher::get_revision() const
    {
        return revision_;
    }

    void PerfPanelProjection::Project(
        PerfSnapshot& snapshot,
        const PerfFrameHistory& history,
        uint64_t revision,
        double panel_fps,
        PerfFrameGraphs* frames,
        uint32_t columns
    ) const
    {
        columns = std::min(columns, kPerfHistoryCapacity);

        if (!valid_ || revision_ != revision || frames_.frames.size() != columns)
        {
            history.BuildFrameGraphs(snapshot.captured_at, columns, frames_);
            revision_ = revision;
            valid_ = true;
        }

        snapshot.summaries = frames_.summaries;
        snapshot.summaries[uint32_t(PerfSeries::FrameInterval)].fps = panel_fps;

        if (frames)
        {
            *frames = frames_;
        }
    }
} // namespace performance
