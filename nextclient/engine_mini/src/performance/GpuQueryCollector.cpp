#include "performance/GpuQueryCollector.h"

#include <cstdlib>

#include "performance/PerformanceMetrics.h"
#include "performance/performance_stages.h"

namespace performance
{
    namespace
    {
        constexpr uint32_t kProtectedRadarPairs = 16;
        constexpr uint32_t kGpuScopeCount = [] {
            uint32_t count = 0;

            for (const StageDescriptor& stage : kStages)
            {
                count += HasCapability(stage, ncl_perf::PerfScopeCapability::Gpu);
            }

            return count;
        }();

        uint32_t g_NextTokenEpoch{};

        uint32_t NextTokenEpoch()
        {
            if (++g_NextTokenEpoch == 0)
            {
                std::abort();
            }
            return g_NextTokenEpoch;
        }
    } // namespace

    GpuQueryCollector::GpuQueryCollector(GpuQueryBackendInterface& backend, uint32_t collector_generation, uint32_t context_generation) :
        backend_(backend),
        generation_(collector_generation),
        context_generation_(context_generation),
        token_epoch_(NextTokenEpoch())
    {
        if (backend_.IsTimestampSupported() && backend_.counter_bits() > 0)
        {
            backend_.Allocate(uint32_t(names_.size()), names_.data());
            alive_ = true;
        }
    }

    ncl_perf::PerfScopeToken_t GpuQueryCollector::Begin(
        PerfFrameId_t frame_id,
        PerfOutputId_t output_id,
        ncl_perf::PerfScopeId_t scope_id,
        PerfSeries* effective_series
    )
    {
        PerfSeries original_series = stages::GpuSeries(scope_id);
        PerfSeries series = original_series;

        if (effective_series)
        {
            *effective_series = series;
        }

        if (!alive_)
        {
            return 0;
        }

        uint32_t selected = uint32_t(sets_.size());

        for (uint32_t index = 0; index < sets_.size(); ++index)
        {
            const QuerySet& set = sets_[index];

            if (set.pair_count && set.output_id == output_id && set.generation == generation_ && !set.discard)
            {
                selected = index;
                break;
            }

            if (!set.pair_count && selected == sets_.size())
            {
                selected = index;
            }
        }

        if (selected == sets_.size())
        {
            ++dropped_scopes_;
            return 0;
        }

        QuerySet& set = sets_[selected];

        for (uint32_t index = 0; index < set.pair_count; ++index)
        {
            const Pair& parent = set.pairs[index];

            if (!parent.closed && parent.series == series)
            {
                series = PerfSeries::Count;
                break;
            }
        }

        if (effective_series)
        {
            *effective_series = series;
        }

        bool consumes_screen = scope_id == PerfStage::Screen && !set.screen_count;
        bool consumes_monitor = scope_id == PerfStage::Monitor && !set.monitor_count;
        bool consumes_radar = original_series == PerfSeries::RadarGpu && set.radar_count < kProtectedRadarPairs;
        uint32_t reserved = uint32_t(!set.screen_count) + uint32_t(!set.monitor_count) +
                            (set.radar_count < kProtectedRadarPairs ? kProtectedRadarPairs - set.radar_count : 0);
        reserved -= uint32_t(consumes_screen) + uint32_t(consumes_monitor) + uint32_t(consumes_radar);

        if ((set.pair_count && !set.open_count) || set.pair_count + 1 + reserved > set.pairs.size())
        {
            ++dropped_scopes_;
            RecordDrop(set, scope_id, series);
            return 0;
        }

        if (!set.pair_count)
        {
            set.frame_id = frame_id;
            set.output_id = output_id;
            set.generation = generation_;
        }

        if (++sequence_ == 0)
        {
            std::abort();
        }

        ncl_perf::PerfScopeToken_t token = (uint64_t(token_epoch_) << 32) | sequence_;
        uint32_t pair_index = set.pair_count++;

        set.pairs[pair_index] = {token, scope_id, series};
        ++set.open_count;
        set.screen_count += scope_id == PerfStage::Screen;
        set.monitor_count += scope_id == PerfStage::Monitor;
        set.radar_count += original_series == PerfSeries::RadarGpu;

        backend_.Write(names_[NameIndex(selected, pair_index)]);

        return token;
    }

    void GpuQueryCollector::RecordDrop(QuerySet& set, ncl_perf::PerfScopeId_t scope_id, PerfSeries series)
    {
        static_assert(kGpuScopeCount * 2 == QuerySet{}.dropped.size());

        if (!stages::HasCapability(scope_id, ncl_perf::PerfScopeCapability::Gpu))
        {
            return;
        }

        for (uint32_t index = 0; index < set.dropped_count; ++index)
        {
            const DroppedScope& dropped = set.dropped[index];

            if (dropped.scope_id == scope_id && dropped.series == series)
            {
                return;
            }
        }

        if (set.dropped_count == set.dropped.size())
        {
            std::abort();
        }

        set.dropped[set.dropped_count++] = {scope_id, series};
    }

    void GpuQueryCollector::End(ncl_perf::PerfScopeToken_t token)
    {
        if (!token || !alive_ || uint32_t(token >> 32) != token_epoch_)
        {
            return;
        }

        for (uint32_t set_index = 0; set_index < sets_.size(); ++set_index)
        {
            QuerySet& set = sets_[set_index];
            for (uint32_t pair_index = 0; pair_index < set.pair_count; ++pair_index)
            {
                Pair& pair = set.pairs[pair_index];
                if (pair.token == token)
                {
                    if (!pair.closed)
                    {
                        backend_.Write(names_[NameIndex(set_index, pair_index) + 1]);
                        pair.closed = true;
                        --set.open_count;
                    }
                    return;
                }
            }
        }
    }

    uint32_t GpuQueryCollector::Poll(PerfGpuResult* output, uint32_t capacity)
    {
        if (!alive_)
        {
            return 0;
        }

        uint32_t count = 0;

        for (uint32_t set_index = 0; set_index < sets_.size(); ++set_index)
        {
            QuerySet& set = sets_[set_index];

            if (!set.pair_count || set.open_count)
            {
                continue;
            }

            bool ready = true;

            for (uint32_t index = 0; index < set.pair_count; ++index)
            {
                uint32_t name_index = NameIndex(set_index, index);

                if (!backend_.IsReady(names_[name_index]) || !backend_.IsReady(names_[name_index + 1]))
                {
                    ready = false;
                    break;
                }
            }

            if (!ready)
            {
                continue;
            }

            if (set.discard || set.generation != generation_)
            {
                set = {};
                continue;
            }

            bool all_reported = true;

            for (uint32_t index = 0; index < set.pair_count; ++index)
            {
                Pair& pair = set.pairs[index];

                if (pair.reported)
                {
                    continue;
                }

                if (count == capacity)
                {
                    all_reported = false;
                    break;
                }

                uint32_t name_index = NameIndex(set_index, index);
                uint64_t start = backend_.Read(names_[name_index]);
                uint64_t end = backend_.Read(names_[name_index + 1]);
                uint64_t mask = backend_.counter_bits() >= 64 ? UINT64_MAX : (uint64_t(1) << backend_.counter_bits()) - 1;

                output[count++] = {
                    set.frame_id,
                    set.output_id,
                    pair.scope_id,
                    set.generation,
                    context_generation_,
                    (end - start) & mask,
                    pair.series,
                    ncl_perf::PerfSampleState::Measured
                };
                pair.reported = true;
            }

            for (uint32_t index = 0; all_reported && index < set.dropped_count; ++index)
            {
                DroppedScope& dropped = set.dropped[index];

                if (dropped.reported)
                {
                    continue;
                }

                if (count == capacity)
                {
                    all_reported = false;
                    break;
                }

                output[count++] = {
                    set.frame_id,
                    set.output_id,
                    dropped.scope_id,
                    set.generation,
                    context_generation_,
                    0,
                    dropped.series,
                    ncl_perf::PerfSampleState::Incomplete
                };
                dropped.reported = true;
            }

            if (all_reported)
            {
                set = {};
            }
        }
        return count;
    }

    void GpuQueryCollector::Reset(uint32_t collector_generation)
    {
        generation_ = collector_generation;
        token_epoch_ = NextTokenEpoch();
        if (!alive_)
        {
            return;
        }

        for (uint32_t set_index = 0; set_index < sets_.size(); ++set_index)
        {
            QuerySet& set = sets_[set_index];
            set.discard = set.pair_count != 0;

            for (uint32_t index = 0; index < set.pair_count; ++index)
            {
                if (!set.pairs[index].closed)
                {
                    backend_.Write(names_[NameIndex(set_index, index) + 1]);
                    set.pairs[index].closed = true;
                }
            }
            set.open_count = 0;
        }
    }

    void GpuQueryCollector::ReleaseGl()
    {
        if (alive_)
        {
            backend_.Delete(uint32_t(names_.size()), names_.data());
        }
        AbandonGl();
    }

    void GpuQueryCollector::AbandonGl()
    {
        alive_ = false;
        names_ = {};
        sets_ = {};
        token_epoch_ = NextTokenEpoch();
    }

    void GpuQueryCollector::CancelOutput(PerfOutputId_t output_id)
    {
        for (QuerySet& set : sets_)
        {
            if (set.output_id == output_id)
            {
                set.discard = true;
            }
        }
    }

    uint64_t GpuQueryCollector::dropped_scopes() const
    {
        return dropped_scopes_;
    }

    uint32_t GpuQueryCollector::NameIndex(uint32_t set_index, uint32_t pair_index) const
    {
        return set_index * 256 + pair_index * 2;
    }
} // namespace performance
