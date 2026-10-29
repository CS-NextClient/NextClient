#pragma once

#include <array>
#include <memory>

#include <next_engine_mini/PerformanceData.h>

namespace performance
{
    class PerfDumpCapture
    {
    private:
        enum class State
        {
            Idle,
            Pending,
            Active,
            Ready
        };

        std::unique_ptr<PerfSnapshot> snapshot_;
        ncl_perf::PerfTimestamp_t started_at_{};
        State state_{};

    public:
        bool Request();
        bool ActivatePending();

        void SetStartedAt(ncl_perf::PerfTimestamp_t started_at)
        {
            started_at_ = started_at;
        }

        PerfSnapshot& get_capture_buffer()
        {
            return *snapshot_;
        }

        void Complete(
            ncl_perf::PerfTimestamp_t completed_at,
            PerfFrameId_t frame_id,
            const std::array<ncl_perf::PerfCpuSample, ncl_perf::kPerfScopeCapacity>& samples,
            uint32_t sample_count
        );
        void RetryActive();
        void Cancel();
        bool Take(PerfSnapshot& output);

        bool is_active() const
        {
            return state_ == State::Active;
        }

        bool is_ready() const
        {
            return state_ == State::Ready;
        }

        const PerfSnapshot* get_snapshot() const
        {
            return snapshot_ && snapshot_->cpu_frame_count ? snapshot_.get() : nullptr;
        }
    };
} // namespace performance
