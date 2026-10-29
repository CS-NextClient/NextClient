#pragma once

#include <cstddef>
#include <cstdint>

#include <next_engine_mini/PerformanceScopes.h>

#include "performance/PerformanceMonitor.h"

namespace performance
{
    class LocalCpuScope
    {
    private:
        PerformanceMonitor* monitor_{};
        ncl_perf::PerfScopeToken_t token_{};

    public:
        LocalCpuScope(
            PerformanceMonitor* monitor,
            ncl_perf::PerfScopeId_t scope_id,
            std::source_location location = std::source_location::current()
        ) :
            monitor_(monitor),
            token_(monitor ? monitor->BeginCpu(scope_id, location) : 0)
        {}
        ~LocalCpuScope()
        {
            if (token_)
            {
                monitor_->EndCpu(token_);
            }
        }
        LocalCpuScope(const LocalCpuScope&) = delete;
        LocalCpuScope& operator=(const LocalCpuScope&) = delete;
    };

    class LocalGpuScope
    {
    private:
        PerformanceMonitor* monitor_{};
        ncl_perf::PerfScopeToken_t token_{};

    public:
        LocalGpuScope(
            PerformanceMonitor* monitor,
            ncl_perf::PerfScopeId_t scope_id,
            std::source_location location = std::source_location::current()
        ) :
            monitor_(monitor),
            token_(monitor ? monitor->BeginGpu(scope_id, location) : 0)
        {}
        ~LocalGpuScope()
        {
            if (token_)
            {
                monitor_->EndGpu(token_);
            }
        }
        LocalGpuScope(const LocalGpuScope&) = delete;
        LocalGpuScope& operator=(const LocalGpuScope&) = delete;
    };

    namespace reporting
    {
        void ReportDraw(PerformanceMonitorInterface* monitor, uint64_t index_count, size_t range_count);
        void StudioFlush(PerformanceMonitorInterface* monitor, PerfCounterId reason, size_t packets);
        void StudioImmediate(PerformanceMonitorInterface* monitor, PerfCounterId reason);
        void StudioQueuePeak(PerformanceMonitorInterface* monitor, size_t packets);
        void ReportDraw(PerformanceMonitor& monitor, uint64_t index_count, size_t range_count);
        void StudioFlush(PerformanceMonitor& monitor, PerfCounterId reason, size_t packets);
        void StudioImmediate(PerformanceMonitor& monitor, PerfCounterId reason);
        void StudioQueuePeak(PerformanceMonitor& monitor, size_t packets);
    } // namespace reporting
} // namespace performance
