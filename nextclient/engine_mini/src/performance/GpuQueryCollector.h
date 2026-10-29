#pragma once

#include <array>

#include <next_engine_mini/PerformanceData.h>

#include "performance/GpuQueryBackend.h"

namespace performance
{
    class GpuQueryCollector
    {
    private:
        struct Pair
        {
            ncl_perf::PerfScopeToken_t token{};
            ncl_perf::PerfScopeId_t scope_id{};
            PerfSeries series = PerfSeries::Count;
            bool closed{};
            bool reported{};
        };

        struct QuerySet
        {
            std::array<Pair, 128> pairs{};
            PerfFrameId_t frame_id{};
            PerfOutputId_t output_id{};
            uint32_t generation{};
            uint32_t pair_count{};
            uint32_t open_count{};
            bool incomplete{};
            bool discard{};
        };

        GpuQueryBackendInterface& backend_;
        std::array<uint32_t, 2048> names_{};
        std::array<QuerySet, 8> sets_{};
        uint32_t generation_{};
        uint32_t context_generation_{};
        uint32_t token_epoch_{};
        uint32_t sequence_{};
        uint64_t dropped_scopes_{};
        bool alive_{};

    public:
        GpuQueryCollector(GpuQueryBackendInterface& backend, uint32_t collector_generation, uint32_t context_generation);
        ncl_perf::PerfScopeToken_t Begin(PerfFrameId_t frame_id, PerfOutputId_t output_id, ncl_perf::PerfScopeId_t scope_id);
        void End(ncl_perf::PerfScopeToken_t token);
        uint32_t Poll(PerfGpuResult* output, uint32_t capacity);
        void Reset(uint32_t collector_generation);
        void ReleaseGl();
        void AbandonGl();
        void CancelOutput(PerfOutputId_t output_id);
        uint64_t dropped_scopes() const;

    private:
        uint32_t NameIndex(uint32_t set_index, uint32_t pair_index) const;
    };
} // namespace performance
