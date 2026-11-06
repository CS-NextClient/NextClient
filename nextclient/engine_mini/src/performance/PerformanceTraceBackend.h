#pragma once

#include <source_location>

#include <next_engine_mini/PerformanceData.h>

namespace performance
{
    class PerformanceTraceBackendInterface
    {
    public:
        virtual ~PerformanceTraceBackendInterface() = default;
        virtual bool IsConnected() const = 0;
        virtual ncl_perf::PerfScopeToken_t BeginCpu(
            ncl_perf::PerfScopeId_t scope_id,
            std::source_location location = std::source_location::current()
        ) = 0;
        virtual void EndCpu(ncl_perf::PerfScopeToken_t token) = 0;
        virtual ncl_perf::PerfScopeToken_t BeginGpu(
            ncl_perf::PerfScopeId_t scope_id,
            std::source_location location = std::source_location::current()
        ) = 0;
        virtual void EndGpu(ncl_perf::PerfScopeToken_t token) = 0;
        virtual void Collect() = 0;
        virtual void MarkFrame(bool output, uint64_t id) = 0;
        virtual void RecordCounter(PerfCounterId counter, uint64_t value) = 0;
        virtual void RecordGauge(PerfGaugeId gauge, double value) = 0;
    };
} // namespace performance
