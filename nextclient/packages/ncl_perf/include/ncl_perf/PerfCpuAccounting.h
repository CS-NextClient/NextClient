#pragma once

#include <array>

#include <ncl_perf/performance_data.h>

namespace ncl_perf
{
    class PerfCpuAccounting
    {
        friend struct PerfCpuAccountingTestAccess;

    private:
        struct RegisteredScope
        {
            PerfCpuSample sample;
            bool parent_seen{};
            PerfScopeCapability capabilities = PerfScopeCapability::Cpu;
        };

        struct ActiveScope
        {
            PerfScopeToken_t token{};
            PerfTimestamp_t start{};
            PerfDuration_t children{};
            uint32_t registry_index{};
        };

        PerfClockInterface& clock_;
        std::array<RegisteredScope, kPerfScopeCapacity> registry_{};
        std::array<ActiveScope, kPerfStackCapacity> stack_{};
        uint32_t registry_count_{};
        uint32_t stack_count_{};
        uint32_t generation_ = 1;
        uint32_t sequence_{};
        PerfScopeId_t last_scope_id_{};
        PerfDuration_t last_duration_ns_{};
        bool enabled_{};
        bool incomplete_{};
        uint64_t dropped_scopes_{};

    public:
        explicit PerfCpuAccounting(PerfClockInterface& clock);
        PerfScopeId_t RegisterScope(const PerfScopeDesc& descriptor);
        PerfScopeToken_t Begin(PerfScopeId_t scope_id);
        void End(PerfScopeToken_t token);
        uint32_t CopySamples(PerfCpuSample* output, uint32_t capacity) const;
        void ClearFrame();
        void Reset(uint32_t generation);
        void SetEnabled(bool enabled);
        PerfScopeId_t last_scope_id() const;
        PerfDuration_t last_duration_ns() const;
        uint64_t dropped_scopes() const;

    private:
        uint32_t FindScope(PerfScopeId_t scope_id) const;
    };
} // namespace ncl_perf
