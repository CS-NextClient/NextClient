#pragma once

#include <memory>
#include <span>

#include <next_engine_mini/PerformanceData.h>

namespace performance
{
    class PerfBatchWindow
    {
    private:
        struct BatchState;

        std::unique_ptr<BatchState> batch_;
        int level_{};
        int frame_level_{};

    public:
        PerfBatchWindow();
        ~PerfBatchWindow();
        bool SetLevel(int level);

        int get_level() const
        {
            return level_;
        }

        void BeginFrame()
        {
            frame_level_ = level_;
        }

        void CompleteFrame(const PerfFrameRecord& record, std::span<const ncl_perf::PerfCpuSample> samples);
        void Reset();
        const PerfSnapshot* get_snapshot() const;
    };
} // namespace performance
