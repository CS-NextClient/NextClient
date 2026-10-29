#pragma once

#include <array>
#include <memory>
#include <vector>

#include <next_engine_mini/PerformanceData.h>

namespace performance
{
    inline constexpr uint32_t kPerfGroupedGraphCapacity = kPerfGraphCapacity;

    struct PerfGroupedGraphBin
    {
        PerfGraphBin interval;
        PerfGraphBin total;
        std::array<ncl_perf::PerfDuration_t, 3> parts{};
        bool parts_available{};
    };

    struct PerfGroupedGraphs
    {
        std::array<PerfGroupedGraphBin, kPerfGroupedGraphCapacity> cpu{};
        std::array<PerfGroupedGraphBin, kPerfGroupedGraphCapacity> gpu{};
    };

    struct PerfFrameGraph
    {
        PerfFrameId_t frame_id{};
        PerfOutputId_t output_id{};
        std::array<PerfValue, uint32_t(PerfSeries::Count)> values{};
    };

    struct PerfFrameGraphs
    {
        ncl_perf::PerfTimestamp_t captured_at{};
        uint32_t collector_generation{};
        uint32_t count{};
        std::vector<PerfFrameGraph> frames;
        std::array<PerfSummary, uint32_t(PerfSeries::Count)> summaries{};
    };

    namespace history
    {
        uint32_t BinIndex(ncl_perf::PerfTimestamp_t now, ncl_perf::PerfTimestamp_t sample, uint32_t capacity);
    } // namespace history

    class PerfFrameHistory
    {
    private:
        std::unique_ptr<std::array<PerfFrameRecord, kPerfHistoryCapacity>> records_;
        mutable std::unique_ptr<std::array<ncl_perf::PerfDuration_t, kPerfHistoryCapacity>> scratch_;
        uint32_t first_{};
        uint32_t count_{};
        uint32_t generation_ = 1;
        ncl_perf::PerfTimestamp_t newest_{};
        uint32_t gpu_record_index_{};
        bool gpu_record_valid_{};

    public:
        PerfFrameHistory();
        void Append(const PerfFrameRecord& record);
        bool ApplyGpu(const PerfGpuResult& result);
        void Reset(uint32_t generation);
        void BuildSnapshot(ncl_perf::PerfTimestamp_t now, PerfSnapshot& output, PerfGroupedGraphs* groups = nullptr) const;
        void BuildFrameGraphs(ncl_perf::PerfTimestamp_t now, uint32_t columns, PerfFrameGraphs& output) const;
        double CalculateFps(ncl_perf::PerfTimestamp_t now, ncl_perf::PerfDuration_t window_ns) const;
        void InvalidatePendingGpu();
        uint32_t CountOutputsAfter(PerfOutputId_t output_id) const;

    private:
        void BuildGroups(uint32_t first_valid, ncl_perf::PerfTimestamp_t now, PerfGroupedGraphs& groups) const;
        const PerfFrameRecord& Record(uint32_t index) const;

    private:
        static bool IsGpuTarget(const PerfFrameRecord& record, const PerfGpuResult& result);
    };
} // namespace performance
