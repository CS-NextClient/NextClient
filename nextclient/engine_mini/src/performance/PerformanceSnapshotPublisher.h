#pragma once

#include <array>
#include <memory>
#include <span>

#include "performance/PerformanceFrameHistory.h"

namespace performance
{
    class PerfSnapshotPublisher
    {
    private:
        std::array<ncl_perf::PerfCpuSample, ncl_perf::kPerfScopeCapacity> pending_tree_{};
        std::array<ncl_perf::PerfCpuSample, ncl_perf::kPerfScopeCapacity> numeric_tree_{};
        std::unique_ptr<PerfSnapshot> snapshot_ = std::make_unique<PerfSnapshot>();
        PerfGroupedGraphs grouped_graphs_;
        ncl_perf::PerfTimestamp_t numeric_start_{};
        ncl_perf::PerfTimestamp_t completed_at_{};
        PerfFrameId_t completed_frame_id_{};
        PerfFrameId_t numeric_frame_id_{};
        ncl_perf::PerfDuration_t numeric_window_ns_{};
        uint32_t pending_tree_count_{};
        uint32_t pending_frame_count_{};
        uint32_t numeric_tree_count_{};
        uint32_t numeric_frame_count_{};
        uint64_t revision_{};
        double panel_fps_{};
        bool numeric_published_{};
        bool collection_published_{};
        bool history_dirty_ = true;

    public:
        void CompleteCpuFrame(
            PerfFrameId_t frame_id,
            std::span<const ncl_perf::PerfCpuSample> samples,
            ncl_perf::PerfTimestamp_t completed_at
        );
        void Advance(ncl_perf::PerfTimestamp_t now, const PerfFrameHistory& history);
        void InvalidateHistory();
        void Reset(uint32_t generation, ncl_perf::PerfTimestamp_t now);
        void CopySnapshot(PerfSnapshot& output) const;
        void CopyGroupedGraphs(PerfGroupedGraphs& output) const;
        double get_panel_fps() const;
        uint64_t get_revision() const;
    };

    class PerfPanelProjection
    {
    private:
        mutable PerfFrameGraphs frames_;
        mutable uint64_t revision_{};
        mutable bool valid_{};

    public:
        void Project(
            PerfSnapshot& snapshot,
            const PerfFrameHistory& history,
            uint64_t revision,
            double panel_fps,
            PerfFrameGraphs* frames,
            uint32_t columns
        ) const;
    };
} // namespace performance
