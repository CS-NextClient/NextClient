#include "performance/PerformanceFrameHistory.h"

#include <algorithm>
#include <cstdlib>

#include "performance/PerformanceMetrics.h"

namespace performance
{
    namespace
    {
        static_assert(PerfGroupedGraphBin{}.parts.size() == performance::kGroupPartCapacity);

        void CompleteSummary(
            const performance::SeriesDescriptor& series,
            ncl_perf::PerfDuration_t total,
            std::array<ncl_perf::PerfDuration_t, kPerfHistoryCapacity>& samples,
            PerfSummary& summary
        )
        {
            if (!summary.count)
            {
                return;
            }

            summary.mean_ns = total / summary.count;
            summary.fps =
                series.source == performance::SeriesSource::PresentedInterval ? double(summary.count) * 1000000000.0 / double(total) : 0;
            auto middle = samples.begin() + summary.count / 2;
            auto end = samples.begin() + summary.count;

            if (summary.count >= 100)
            {
                std::sort(samples.begin(), end);
                summary.percentiles_available = true;
                summary.p95_ns = samples[(summary.count * 95 + 99) / 100 - 1];
                summary.p99_ns = samples[(summary.count * 99 + 99) / 100 - 1];
            }
            else
            {
                std::nth_element(samples.begin(), middle, end);
            }

            summary.median_ns = *middle;
            if (summary.count % 2 == 0)
            {
                ncl_perf::PerfDuration_t lower = *std::max_element(samples.begin(), middle);
                summary.median_ns = lower + (*middle - lower) / 2;
            }
        }
    } // namespace

    uint32_t history::BinIndex(ncl_perf::PerfTimestamp_t now, ncl_perf::PerfTimestamp_t sample, uint32_t capacity)
    {
        ncl_perf::PerfDuration_t offset = kPerfHistoryDuration - (now - sample);

        return uint32_t(std::min<uint64_t>(offset * capacity / kPerfHistoryDuration, capacity - 1));
    }

    PerfFrameHistory::PerfFrameHistory() :
        records_(std::make_unique<std::array<PerfFrameRecord, kPerfHistoryCapacity>>()),
        scratch_(std::make_unique<std::array<ncl_perf::PerfDuration_t, kPerfHistoryCapacity>>())
    {}

    void PerfFrameHistory::Append(const PerfFrameRecord& record)
    {
        if (record.collector_generation != generation_)
        {
            return;
        }

        if (record.completed_at < newest_)
        {
            std::abort();
        }

        newest_ = record.completed_at;

        if (count_ == kPerfHistoryCapacity)
        {
            first_ = (first_ + 1) % kPerfHistoryCapacity;
            --count_;
        }

        uint32_t slot = (first_ + count_) % kPerfHistoryCapacity;

        if (gpu_record_valid_ && gpu_record_index_ == slot)
        {
            gpu_record_valid_ = false;
        }

        (*records_)[slot] = record;
        ++count_;
    }

    bool PerfFrameHistory::ApplyGpu(const PerfGpuResult& result)
    {
        if (result.collector_generation != generation_)
        {
            return false;
        }

        PerfFrameRecord* target = nullptr;

        if (gpu_record_valid_)
        {
            uint32_t offset = (gpu_record_index_ + kPerfHistoryCapacity - first_) % kPerfHistoryCapacity;
            PerfFrameRecord& record = (*records_)[gpu_record_index_];

            if (offset < count_ && IsGpuTarget(record, result))
            {
                target = &record;
            }
        }

        if (!target)
        {
            for (uint32_t index = 0; index < count_; ++index)
            {
                uint32_t slot = (first_ + index) % kPerfHistoryCapacity;
                PerfFrameRecord& record = (*records_)[slot];

                if (IsGpuTarget(record, result))
                {
                    target = &record;
                    gpu_record_index_ = slot;
                    gpu_record_valid_ = true;
                    break;
                }
            }
        }

        if (!target)
        {
            return false;
        }

        if (result.series == PerfSeries::Count)
        {
            return true;
        }

        const performance::SeriesDescriptor* series = performance::Series(result.series);

        if (!series || series->source != performance::SeriesSource::GpuRollup)
        {
            return false;
        }

        PerfValue& value = target->values[uint32_t(result.series)];

        if (result.state != ncl_perf::PerfSampleState::Measured)
        {
            value.state = result.state;
        }
        else if (value.state != ncl_perf::PerfSampleState::Incomplete && value.state != ncl_perf::PerfSampleState::Dropped)
        {
            value.duration_ns += result.duration_ns;
            value.state = ncl_perf::PerfSampleState::Measured;
        }

        return true;
    }

    bool PerfFrameHistory::IsGpuTarget(const PerfFrameRecord& record, const PerfGpuResult& result)
    {
        return record.kind == PerfRecordKind::Output && record.output_id == result.output_id &&
               record.frame_id == result.frame_id && record.context_generation == result.context_generation;
    }

    void PerfFrameHistory::Reset(uint32_t generation)
    {
        generation_ = generation;
        first_ = 0;
        count_ = 0;
        newest_ = 0;
        gpu_record_valid_ = false;
    }

    void PerfFrameHistory::InvalidatePendingGpu()
    {
        for (uint32_t index = 0; index < count_; ++index)
        {
            PerfFrameRecord& record = (*records_)[(first_ + index) % kPerfHistoryCapacity];

            for (PerfValue& value : record.values)
            {
                if (value.state == ncl_perf::PerfSampleState::Pending)
                {
                    value.state = ncl_perf::PerfSampleState::Incomplete;
                    value.duration_ns = 0;
                }
            }
        }
    }

    uint32_t PerfFrameHistory::CountOutputsAfter(PerfOutputId_t output_id) const
    {
        uint32_t count = 0;

        for (uint32_t index = count_; index > 0; --index)
        {
            const PerfFrameRecord& record = Record(index - 1);

            if (record.kind == PerfRecordKind::Output)
            {
                if (record.output_id <= output_id)
                {
                    break;
                }

                ++count;
            }
        }

        return count;
    }

    double PerfFrameHistory::CalculateFps(ncl_perf::PerfTimestamp_t now, ncl_perf::PerfDuration_t window_ns) const
    {
        ncl_perf::PerfTimestamp_t cutoff = now >= window_ns ? now - window_ns : 0;
        ncl_perf::PerfDuration_t total = 0;
        uint32_t count = 0;

        for (uint32_t index = count_; index > 0; --index)
        {
            const PerfFrameRecord& record = Record(index - 1);

            if (now >= window_ns && record.completed_at <= cutoff)
            {
                break;
            }
            if (record.completed_at > now || record.kind != PerfRecordKind::Output)
            {
                continue;
            }

            const PerfValue& interval = record.values[uint32_t(PerfSeries::FrameInterval)];

            if (performance::IsSummarySample(*performance::Series(PerfSeries::FrameInterval), interval))
            {
                total += interval.duration_ns;
                ++count;
            }
        }

        return count ? double(count) * 1000000000.0 / double(total) : 0;
    }

    void PerfFrameHistory::BuildSnapshot(ncl_perf::PerfTimestamp_t now, PerfSnapshot& output, PerfGroupedGraphs* groups) const
    {
        output = {};

        if (groups)
        {
            *groups = {};
        }

        output.captured_at = now;
        output.collector_generation = generation_;

        ncl_perf::PerfTimestamp_t cutoff = now > kPerfHistoryDuration ? now - kPerfHistoryDuration : 0;
        uint32_t first_valid = 0;

        while (first_valid < count_ && Record(first_valid).completed_at < cutoff)
        {
            ++first_valid;
        }

        output.record_count = count_ - first_valid;
        if (!output.record_count)
        {
            return;
        }

        const PerfFrameRecord& oldest = Record(first_valid);
        ncl_perf::PerfTimestamp_t start = oldest.completed_at;
        const PerfValue& interval = oldest.values[uint32_t(PerfSeries::FrameInterval)];

        if (oldest.kind == PerfRecordKind::Output && interval.state == ncl_perf::PerfSampleState::Measured)
        {
            start = interval.duration_ns > start ? 0 : start - interval.duration_ns;
        }

        start = std::max(start, cutoff);
        output.coverage_ns = now - start;
        output.capacity_limited = output.record_count == kPerfHistoryCapacity && start > cutoff;
        output.latest = Record(count_ - 1);

        for (uint32_t series_index = 0; series_index < uint32_t(PerfSeries::Count); ++series_index)
        {
            const performance::SeriesDescriptor& series = *performance::Series(PerfSeries(series_index));
            PerfSummary& summary = output.summaries[series_index];
            ncl_perf::PerfDuration_t total = 0;
            std::array<ncl_perf::PerfDuration_t, kPerfGraphCapacity> bin_totals{};

            for (uint32_t index = first_valid; index < count_; ++index)
            {
                const PerfFrameRecord& record = Record(index);

                if (series.domain != record.kind)
                {
                    continue;
                }

                const PerfValue& value = record.values[series_index];

                if (!performance::IsSummarySample(series, value))
                {
                    if (!summary.count)
                    {
                        summary.state = value.state;
                    }

                    continue;
                }

                (*scratch_)[summary.count++] = value.duration_ns;
                total += value.duration_ns;
                summary.maximum_ns = std::max(summary.maximum_ns, value.duration_ns);
                summary.state = ncl_perf::PerfSampleState::Measured;

                uint32_t bin_index = history::BinIndex(now, record.completed_at, kPerfGraphCapacity);
                PerfGraphBin& bin = output.graphs[series_index][bin_index];

                ++bin.count;
                bin.maximum_ns = std::max(bin.maximum_ns, value.duration_ns);
                bin_totals[bin_index] += value.duration_ns;
            }

            CompleteSummary(series, total, *scratch_, summary);

            for (uint32_t index = 0; index < kPerfGraphCapacity; ++index)
            {
                PerfGraphBin& bin = output.graphs[series_index][index];

                if (bin.count)
                {
                    bin.mean_ns = bin_totals[index] / bin.count;
                }
            }
        }

        for (uint32_t index = first_valid; index < count_; ++index)
        {
            const PerfFrameRecord& record = Record(index);

            for (const performance::GaugeDescriptor& gauge : performance::kGauges)
            {
                if (gauge.domain == record.kind)
                {
                    uint32_t gauge_index = uint32_t(gauge.id);
                    output.gauges[gauge_index] = record.gauges[gauge_index];
                    performance::ReduceScalar(output.gauge_summaries[gauge_index], record.gauges[gauge_index]);
                }
            }

            for (const performance::CounterDescriptor& counter : performance::kCounters)
            {
                if (counter.reduction == performance::CounterReduction::SumAcceptedFrames)
                {
                    output.counters[uint32_t(counter.id)] += record.counters[uint32_t(counter.id)];
                }
            }
        }

        if (groups)
        {
            BuildGroups(first_valid, now, *groups);
        }
    }

    void PerfFrameHistory::BuildFrameGraphs(ncl_perf::PerfTimestamp_t now, uint32_t columns, PerfFrameGraphs& output) const
    {
        output.captured_at = now;
        output.collector_generation = generation_;
        output.count = 0;
        output.summaries = {};
        output.frames.resize(std::min(columns, kPerfHistoryCapacity));
        std::fill(output.frames.begin(), output.frames.end(), PerfFrameGraph{});

        uint32_t remaining = uint32_t(output.frames.size());
        auto append_frame = [&](const PerfFrameRecord* host, const PerfFrameRecord* presented) {
            PerfFrameGraph& frame = output.frames[--remaining];

            frame.frame_id = presented ? presented->frame_id : host->frame_id;
            frame.output_id = presented ? presented->output_id : 0;

            for (uint32_t series_index = 0; series_index < uint32_t(PerfSeries::Count); ++series_index)
            {
                const performance::SeriesDescriptor& series = *performance::Series(PerfSeries(series_index));
                const PerfFrameRecord* source = series.domain == PerfRecordKind::Host ? host : presented;

                if (source)
                {
                    frame.values[series_index] = source->values[series_index];
                }
            }

            ++output.count;
        };

        const PerfFrameRecord* pending_host = nullptr;

        for (uint32_t index = count_; index > 0 && remaining; --index)
        {
            const PerfFrameRecord& record = Record(index - 1);

            if (record.completed_at > now)
            {
                continue;
            }

            // Each Host follows its Outputs, so the first matching Output is its last presentation.
            if (record.kind == PerfRecordKind::Host)
            {
                if (pending_host)
                {
                    append_frame(pending_host, nullptr);
                }

                pending_host = &record;
                continue;
            }

            if (pending_host && pending_host->frame_id != record.frame_id)
            {
                append_frame(pending_host, nullptr);
                pending_host = nullptr;

                if (!remaining)
                {
                    break;
                }
            }

            append_frame(pending_host, &record);
            pending_host = nullptr;
        }

        if (pending_host && remaining)
        {
            append_frame(pending_host, nullptr);
        }

        for (uint32_t series_index = 0; series_index < uint32_t(PerfSeries::Count); ++series_index)
        {
            const performance::SeriesDescriptor& series = *performance::Series(PerfSeries(series_index));
            PerfSummary& summary = output.summaries[series_index];
            ncl_perf::PerfDuration_t total = 0;

            for (uint32_t index = remaining; index < output.frames.size(); ++index)
            {
                const PerfValue& value = output.frames[index].values[series_index];

                if (!performance::IsSummarySample(series, value))
                {
                    if (!summary.count)
                    {
                        summary.state = value.state;
                    }

                    continue;
                }

                (*scratch_)[summary.count++] = value.duration_ns;
                total += value.duration_ns;
                summary.maximum_ns = std::max(summary.maximum_ns, value.duration_ns);
                summary.state = ncl_perf::PerfSampleState::Measured;
            }

            CompleteSummary(series, total, *scratch_, summary);
        }
    }

    void PerfFrameHistory::BuildGroups(uint32_t first_valid, ncl_perf::PerfTimestamp_t now, PerfGroupedGraphs& groups) const
    {
        for (uint32_t index = first_valid; index < count_; ++index)
        {
            const PerfFrameRecord& record = Record(index);
            uint32_t bin_index = history::BinIndex(now, record.completed_at, kPerfGroupedGraphCapacity);
            bool cpu = record.kind == PerfRecordKind::Host;
            performance::SeriesPlacement placement = cpu ? performance::SeriesPlacement::Cpu : performance::SeriesPlacement::Gpu;
            PerfGroupedGraphBin& bin = cpu ? groups.cpu[bin_index] : groups.gpu[bin_index];

            const performance::SeriesDescriptor* interval_series =
                performance::GroupSeries(performance::SeriesPlacement::Cpu, performance::GroupRole::Interval);

            if (interval_series && record.kind == interval_series->domain)
            {
                const PerfValue& interval = record.values[uint32_t(interval_series->id)];

                if (performance::IsSummarySample(*interval_series, interval))
                {
                    PerfGraphBin& target = groups.cpu[bin_index].interval;
                    ++target.count;
                    target.maximum_ns = std::max(target.maximum_ns, interval.duration_ns);
                    target.mean_ns += interval.duration_ns;
                }
            }

            const PerfValue& total = record.values[uint32_t(performance::GroupSeries(placement, performance::GroupRole::Total)->id)];

            if (total.state != ncl_perf::PerfSampleState::Measured)
            {
                continue;
            }

            if (!bin.total.count || total.duration_ns > bin.total.maximum_ns)
            {
                bin.total.maximum_ns = total.duration_ns;
                bin.parts = {};
                bin.parts_available = true;

                ncl_perf::PerfDuration_t sum = 0;

                for (uint32_t part = 0; part < performance::CountParts(placement); ++part)
                {
                    const performance::SeriesDescriptor& series = *performance::GroupSeries(placement, performance::GroupRole::Part, part);
                    const PerfValue& value = record.values[uint32_t(series.id)];

                    if (value.state == ncl_perf::PerfSampleState::Measured)
                    {
                        bin.parts[part] = value.duration_ns;
                        sum += value.duration_ns;
                    }
                    else if (value.state != ncl_perf::PerfSampleState::Inactive)
                    {
                        bin.parts_available = false;
                    }
                }

                // Overlapping or incomplete scopes cannot describe a disjoint composition.
                bin.parts_available = bin.parts_available && sum <= total.duration_ns;
            }

            ++bin.total.count;
            bin.total.mean_ns += total.duration_ns;
        }

        for (std::array<PerfGroupedGraphBin, kPerfGroupedGraphCapacity>* bins : {&groups.cpu, &groups.gpu})
        {
            for (PerfGroupedGraphBin& bin : *bins)
            {
                for (PerfGraphBin* value : {&bin.interval, &bin.total})
                {
                    if (value->count)
                    {
                        value->mean_ns /= value->count;
                    }
                }
            }
        }
    }

    const PerfFrameRecord& PerfFrameHistory::Record(uint32_t index) const
    {
        return (*records_)[(first_ + index) % kPerfHistoryCapacity];
    }
} // namespace performance
