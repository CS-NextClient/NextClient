#pragma once

#include <array>
#include <vector>

#include <next_engine_mini/PerformanceMonitorInterface.h>

class RadarRecordingMonitor : public performance::PerformanceMonitorInterface
{
public:
    uint32_t generation = 1;
    std::vector<ncl_perf::PerfScopeId_t> gpu_begins;
    uint32_t gpu_ends{};
    std::array<uint64_t, uint32_t(performance::PerfCounterId::Count)> counters{};
    std::vector<performance::PerfWorkerReport> workers;

    ncl_perf::PerfScopeToken_t BeginCpu(ncl_perf::PerfScopeId_t, std::source_location) override
    {
        return 0;
    }
    void EndCpu(ncl_perf::PerfScopeToken_t) override {}
    ncl_perf::PerfScopeToken_t BeginGpu(ncl_perf::PerfScopeId_t id, std::source_location) override
    {
        gpu_begins.push_back(id);
        return gpu_begins.size();
    }
    void EndGpu(ncl_perf::PerfScopeToken_t) override
    {
        ++gpu_ends;
    }
    void AddCounter(performance::PerfCounterId id, uint64_t value) override
    {
        counters[uint32_t(id)] += value;
    }
    bool SetGauge(performance::PerfGaugeId, double) override
    {
        return false;
    }
    bool GetSnapshot(performance::PerfSnapshot&) const override
    {
        return false;
    }
    uint32_t map_generation() const override
    {
        return generation;
    }
    void ReportWorker(const performance::PerfWorkerReport& report) override
    {
        workers.push_back(report);
    }
};
