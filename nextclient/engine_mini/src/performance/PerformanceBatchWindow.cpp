#include "performance/PerformanceBatchWindow.h"

#include <algorithm>
#include <array>

#include "performance/PerformanceMetrics.h"

namespace performance
{
    namespace
    {
        constexpr uint32_t kBatchFrameCapacity = 8;
    }

    struct PerfBatchWindow::BatchState
    {
        struct CpuValue
        {
            ncl_perf::PerfDuration_t inclusive_ns{};
            ncl_perf::PerfDuration_t self_ns{};
            uint64_t calls{};
            ncl_perf::PerfScopeId_t parent_id{};
            ncl_perf::PerfSampleState state = ncl_perf::PerfSampleState::Inactive;
        };

        struct Frame
        {
            std::array<CpuValue, ncl_perf::kPerfScopeCapacity> cpu{};
            std::array<uint64_t, uint32_t(PerfCounterId::Count)> counters{};
            std::array<PerfScalarValue, uint32_t(PerfGaugeId::Count)> gauges{};
            ncl_perf::PerfTimestamp_t completed_at{};
        };

        std::array<Frame, kBatchFrameCapacity> frames{};
        PerfSnapshot snapshot;
        uint32_t next{};
        uint32_t count{};
    };

    PerfBatchWindow::PerfBatchWindow() = default;

    PerfBatchWindow::~PerfBatchWindow() = default;

    bool PerfBatchWindow::SetLevel(int level)
    {
        level = std::clamp(level, 0, 2);

        if (level_ == level)
        {
            return false;
        }

        level_ = level;

        if (level && !batch_)
        {
            batch_ = std::make_unique<BatchState>();
        }

        Reset();

        return true;
    }

    void PerfBatchWindow::CompleteFrame(const PerfFrameRecord& record, std::span<const ncl_perf::PerfCpuSample> samples)
    {
        if (!level_ || frame_level_ != level_)
        {
            return;
        }

        BatchState::Frame& frame = batch_->frames[batch_->next];
        frame.counters = record.counters;
        frame.gauges = record.gauges;
        frame.completed_at = record.completed_at;

        for (uint32_t index = 0; index < samples.size(); ++index)
        {
            const ncl_perf::PerfCpuSample& sample = samples[index];
            frame.cpu[index] = {sample.inclusive_ns, sample.self_ns, sample.calls, sample.parent_id, sample.state};
        }

        batch_->next = (batch_->next + 1) % kBatchFrameCapacity;
        batch_->count = std::min(batch_->count + 1, kBatchFrameCapacity);

        PerfSnapshot& output = batch_->snapshot;
        output.captured_at = record.completed_at;
        output.collector_generation = record.collector_generation;
        output.cpu_frame_id = record.frame_id;
        output.cpu_frame_count = batch_->count;
        output.cpu_count = level_ == 2 ? uint32_t(samples.size()) : 0;
        uint32_t oldest = (batch_->next + kBatchFrameCapacity - batch_->count) % kBatchFrameCapacity;
        output.cpu_window_ns = record.completed_at - batch_->frames[oldest].completed_at;
        output.coverage_ns = output.cpu_window_ns;
        output.counters = {};
        output.gauges = record.gauges;
        output.gauge_summaries = {};

        for (uint32_t index = 0; index < output.cpu_count; ++index)
        {
            output.cpu_tree[index] = samples[index];
            output.cpu_tree[index].inclusive_ns = 0;
            output.cpu_tree[index].self_ns = 0;
            output.cpu_tree[index].calls = 0;
            output.cpu_tree[index].state = ncl_perf::PerfSampleState::Inactive;
        }

        for (uint32_t frame_index = 0; frame_index < batch_->count; ++frame_index)
        {
            const BatchState::Frame& source = batch_->frames[frame_index];
            const BatchState::Frame& scalar_source = batch_->frames[(oldest + frame_index) % kBatchFrameCapacity];

            for (const performance::GaugeDescriptor& gauge : performance::kGauges)
            {
                uint32_t gauge_index = uint32_t(gauge.id);
                performance::ReduceScalar(output.gauge_summaries[gauge_index], scalar_source.gauges[gauge_index]);
            }

            for (uint32_t index = 0; index < output.counters.size(); ++index)
            {
                output.counters[index] += source.counters[index];
            }

            for (uint32_t index = 0; index < output.cpu_count; ++index)
            {
                const BatchState::CpuValue& sample = source.cpu[index];
                ncl_perf::PerfCpuSample& total = output.cpu_tree[index];

                if (sample.calls)
                {
                    total.parent_id = !total.calls ? sample.parent_id : total.parent_id == sample.parent_id ? total.parent_id : 0;
                    total.inclusive_ns += sample.inclusive_ns;
                    total.self_ns += sample.self_ns;
                    total.calls += sample.calls;
                }

                if (sample.state == ncl_perf::PerfSampleState::Incomplete || total.state != ncl_perf::PerfSampleState::Incomplete)
                {
                    if (sample.state != ncl_perf::PerfSampleState::Inactive)
                    {
                        total.state = sample.state;
                    }
                }
            }
        }
    }

    void PerfBatchWindow::Reset()
    {
        frame_level_ = -1;

        if (batch_)
        {
            batch_->next = 0;
            batch_->count = 0;
            batch_->snapshot.cpu_count = 0;
            batch_->snapshot.cpu_frame_count = 0;
            batch_->snapshot.cpu_frame_id = 0;
            batch_->snapshot.counters = {};
            batch_->snapshot.gauges = {};
            batch_->snapshot.gauge_summaries = {};
        }
    }

    const PerfSnapshot* PerfBatchWindow::get_snapshot() const
    {
        return level_ && batch_ ? &batch_->snapshot : nullptr;
    }
} // namespace performance
