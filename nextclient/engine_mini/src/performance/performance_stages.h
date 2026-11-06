#pragma once

#include <ncl_perf/PerfCpuAccounting.h>
#include <next_engine_mini/PerformanceMonitorInterface.h>

namespace performance
{
    namespace stages
    {
        void Register(ncl_perf::PerfCpuAccounting& accounting);
        PerfSeries GpuSeries(ncl_perf::PerfScopeId_t scope_id);
        const char* SeriesName(PerfSeries series);
        const char* StateName(ncl_perf::PerfSampleState state);
        const char* CounterName(PerfCounterId counter);
        const char* ScopeName(ncl_perf::PerfScopeId_t scope_id);
        bool HasCapability(ncl_perf::PerfScopeId_t scope_id, ncl_perf::PerfScopeCapability capability);
    } // namespace stages
} // namespace performance
