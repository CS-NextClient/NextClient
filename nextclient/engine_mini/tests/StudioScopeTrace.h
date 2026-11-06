#pragma once

#include <array>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "performance/PerformanceMonitor.h"
#include "performance/PerformanceTraceBackend.h"

class StudioScopeTrace : public performance::PerformanceTraceBackendInterface
{
private:
    performance::PerformanceMonitor& monitor_;
    std::vector<std::pair<ncl_perf::PerfScopeId_t, ncl_perf::PerfScopeToken_t>> stack_;
    ncl_perf::PerfScopeToken_t next_{};

public:
    std::array<uint32_t, ncl_perf::kPerfScopeCapacity> calls{};
    std::array<ncl_perf::PerfScopeId_t, ncl_perf::kPerfScopeCapacity> parents{};

    explicit StudioScopeTrace(performance::PerformanceMonitor& monitor) :
        monitor_(monitor)
    {
        monitor_.SetBatchLevel(2);
        monitor_.SetTraceBackend(this);
    }

    ~StudioScopeTrace()
    {
        monitor_.SetTraceBackend(nullptr);
        monitor_.SetBatchLevel(0);
        EXPECT_TRUE(stack_.empty());
    }

    bool IsConnected() const override
    {
        return true;
    }

    ncl_perf::PerfScopeToken_t BeginCpu(ncl_perf::PerfScopeId_t id, std::source_location) override
    {
        ++calls[id];
        parents[id] = current();
        stack_.emplace_back(id, ++next_);
        return next_;
    }

    void EndCpu(ncl_perf::PerfScopeToken_t token) override
    {
        ASSERT_FALSE(stack_.empty());
        EXPECT_EQ(stack_.back().second, token);
        stack_.pop_back();
    }

    ncl_perf::PerfScopeId_t current() const
    {
        return stack_.empty() ? 0 : stack_.back().first;
    }

    ncl_perf::PerfScopeToken_t BeginGpu(ncl_perf::PerfScopeId_t, std::source_location) override
    {
        return 0;
    }

    void EndGpu(ncl_perf::PerfScopeToken_t) override {}

    void Collect() override {}

    void MarkFrame(bool, uint64_t) override {}

    void RecordCounter(performance::PerfCounterId, uint64_t) override {}

    void RecordGauge(performance::PerfGaugeId, double) override {}
};
