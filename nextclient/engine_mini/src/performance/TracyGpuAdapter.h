#pragma once

#include <memory>

#include "performance/PerformanceTraceBackend.h"

namespace performance
{
    class TracyGpuAdapter
    {
    private:
        struct State;
        std::unique_ptr<State> state_;
        ncl_perf::PerfScopeToken_t sequence_{};

    public:
        TracyGpuAdapter();
        ~TracyGpuAdapter();
        void Initialize();
        void Collect();
        void ReleaseGl();
        void AbandonGl();
        ncl_perf::PerfScopeToken_t Begin(ncl_perf::PerfScopeId_t scope_id, std::source_location location = std::source_location::current());
        void End(ncl_perf::PerfScopeToken_t token);
    };

    class TracyPerformanceBackend : public PerformanceTraceBackendInterface
    {
    private:
        struct State;
        std::unique_ptr<State> state_;
        TracyGpuAdapter gpu_;
        ncl_perf::PerfScopeToken_t sequence_{};

    public:
        TracyPerformanceBackend();
        ~TracyPerformanceBackend() override;
        void InitializeGpu();
        void ReleaseGl();
        void AbandonGl();
        bool IsConnected() const override;
        ncl_perf::PerfScopeToken_t BeginCpu(
            ncl_perf::PerfScopeId_t scope_id,
            std::source_location location = std::source_location::current()
        ) override;
        void EndCpu(ncl_perf::PerfScopeToken_t token) override;
        ncl_perf::PerfScopeToken_t BeginGpu(
            ncl_perf::PerfScopeId_t scope_id,
            std::source_location location = std::source_location::current()
        ) override;
        void EndGpu(ncl_perf::PerfScopeToken_t token) override;
        void Collect() override;
        void MarkFrame(bool output, uint64_t id) override;
        void RecordCounter(PerfCounterId counter, uint64_t value) override;
        void RecordGauge(PerfGaugeId gauge, double value) override;
    };
} // namespace performance
