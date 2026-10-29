#include "performance/performance_stages.h"

#include "performance/PerformanceMetrics.h"

namespace performance
{
    namespace
    {
        constexpr std::array kStateNames = {"Measured", "Inactive", "Pending", "Unavailable", "Dropped", "Incomplete"};
    } // namespace

    void stages::Register(ncl_perf::PerfCpuAccounting& accounting)
    {
        for (const performance::StageDescriptor& stage : performance::kStages)
        {
            accounting.RegisterScope(stage.scope);
        }
    }

    PerfSeries stages::GpuSeries(ncl_perf::PerfScopeId_t scope_id)
    {
        const performance::StageDescriptor* stage = performance::Stage(scope_id);

        return stage ? stage->gpu_series : PerfSeries::Count;
    }

    const char* stages::SeriesName(PerfSeries series)
    {
        const performance::SeriesDescriptor* descriptor = performance::Series(series);

        return descriptor ? descriptor->name : "Unregistered";
    }

    const char* stages::StateName(ncl_perf::PerfSampleState state)
    {
        return uint32_t(state) < kStateNames.size() ? kStateNames[uint32_t(state)] : "Unregistered";
    }

    const char* stages::CounterName(PerfCounterId counter)
    {
        const performance::CounterDescriptor* descriptor = performance::Counter(counter);

        return descriptor ? descriptor->name : "Unregistered";
    }

    const char* stages::ScopeName(ncl_perf::PerfScopeId_t scope_id)
    {
        const performance::StageDescriptor* stage = performance::Stage(scope_id);

        return stage ? stage->scope.name : "Unregistered";
    }

    bool stages::HasCapability(ncl_perf::PerfScopeId_t scope_id, ncl_perf::PerfScopeCapability capability)
    {
        const performance::StageDescriptor* stage = performance::Stage(scope_id);

        return stage && performance::HasCapability(*stage, capability);
    }
} // namespace performance
