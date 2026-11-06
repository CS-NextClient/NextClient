#include "performance/PerformanceDumpCapture.h"

namespace performance
{
    bool PerfDumpCapture::Request()
    {
        if (state_ != State::Idle)
        {
            return false;
        }

        if (!snapshot_)
        {
            snapshot_ = std::make_unique<PerfSnapshot>();
        }

        state_ = State::Pending;

        return true;
    }

    bool PerfDumpCapture::ActivatePending()
    {
        if (state_ != State::Pending)
        {
            return false;
        }

        state_ = State::Active;

        return true;
    }

    void PerfDumpCapture::Complete(
        ncl_perf::PerfTimestamp_t completed_at,
        PerfFrameId_t frame_id,
        const std::array<ncl_perf::PerfCpuSample, ncl_perf::kPerfScopeCapacity>& samples,
        uint32_t sample_count
    )
    {
        snapshot_->cpu_tree = samples;
        snapshot_->cpu_count = sample_count;
        snapshot_->cpu_frame_id = frame_id;
        snapshot_->cpu_frame_count = 1;
        snapshot_->cpu_window_ns = completed_at - started_at_;
        state_ = State::Ready;
    }

    void PerfDumpCapture::RetryActive()
    {
        if (state_ == State::Active)
        {
            state_ = State::Pending;
        }
    }

    void PerfDumpCapture::Cancel()
    {
        state_ = State::Idle;
        snapshot_.reset();
    }

    bool PerfDumpCapture::Take(PerfSnapshot& output)
    {
        if (state_ != State::Ready)
        {
            return false;
        }

        output = *snapshot_;
        state_ = State::Idle;

        return true;
    }
} // namespace performance
