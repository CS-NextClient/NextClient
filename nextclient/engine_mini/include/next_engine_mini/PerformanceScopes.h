#pragma once

#include <next_engine_mini/PerformanceMonitorInterface.h>

namespace performance
{
    class PerformanceCpuScope
    {
    private:
        PerformanceMonitorInterface* monitor_{};
        ncl_perf::PerfScopeToken_t token_{};

    public:
        PerformanceCpuScope(
            PerformanceMonitorInterface* monitor,
            ncl_perf::PerfScopeId_t scope_id,
            std::source_location location = std::source_location::current()
        ) :
            monitor_(monitor)
        {
            if (monitor_)
            {
                token_ = monitor_->BeginCpu(scope_id, location);
            }
        }

        ~PerformanceCpuScope()
        {
            if (monitor_ && token_)
            {
                monitor_->EndCpu(token_);
            }
        }

        PerformanceCpuScope(const PerformanceCpuScope&) = delete;
        PerformanceCpuScope& operator=(const PerformanceCpuScope&) = delete;
    };

    class PerformanceGpuScope
    {
    private:
        PerformanceMonitorInterface* monitor_{};
        ncl_perf::PerfScopeToken_t token_{};

    public:
        PerformanceGpuScope(
            PerformanceMonitorInterface* monitor,
            ncl_perf::PerfScopeId_t scope_id,
            std::source_location location = std::source_location::current()
        ) :
            monitor_(monitor)
        {
            if (monitor_)
            {
                token_ = monitor_->BeginGpu(scope_id, location);
            }
        }

        ~PerformanceGpuScope()
        {
            if (monitor_ && token_)
            {
                monitor_->EndGpu(token_);
            }
        }

        PerformanceGpuScope(const PerformanceGpuScope&) = delete;
        PerformanceGpuScope& operator=(const PerformanceGpuScope&) = delete;
    };
} // namespace performance
