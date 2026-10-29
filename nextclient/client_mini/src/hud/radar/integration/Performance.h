#pragma once

#include <next_engine_mini/PerformanceScopes.h>
#include <ncl_gl/program.h>

namespace radar::integration
{
    class Performance
    {
    private:
        performance::PerformanceMonitorInterface* monitor_{};

    public:
        explicit Performance(performance::PerformanceMonitorInterface* monitor) :
            monitor_(monitor)
        {}
        performance::PerformanceMonitorInterface* get_monitor() const
        {
            return monitor_;
        }
        uint32_t get_map_generation() const;
        void ReportWorker(const performance::PerfWorkerReport& report);
        void AddCounter(performance::PerfCounterId counter, uint64_t value);
        ncl_gl::ProgramBuildCallbacks ProgramCallbacks() const;
    };
} // namespace radar::integration

namespace radar::integration
{
    class CpuScope
    {
    private:
        performance::PerformanceCpuScope total_;
        performance::PerformanceCpuScope stage_;

    public:
        explicit CpuScope(
            performance::PerformanceMonitorInterface* monitor,
            ncl_perf::PerfScopeId_t stage,
            bool visible = true,
            std::source_location location = std::source_location::current()
        ) :
            total_(visible ? monitor : nullptr, performance::PerfStage::Radar, location),
            stage_(visible ? monitor : nullptr, stage, location)
        {}
    };

} // namespace radar::integration
