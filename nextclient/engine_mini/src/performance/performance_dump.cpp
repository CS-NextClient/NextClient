#include "performance/performance_dump.h"

#include <algorithm>
#include <cstdarg>
#include <cstdio>

#if USE_PROFILER
#include <client/TracyQueueMemory.hpp>
#endif

#include "performance/performance_stages.h"
#include "performance/PerformanceMetrics.h"
#include "performance/PerformanceNetwork.h"
#include "performance/PerformanceMapper.h"

namespace performance
{
    namespace
    {
        constexpr const char* kNetworkSeriesNames[] = {
            "Latency ms", "Incoming KiB/s", "Outgoing KiB/s", "Loss percent", "Choke percent", "Incoming pps", "Outgoing pps"
        };
        constexpr const char* kNetworkPacketNames[] = {"Latency ms", "Incoming B", "Outgoing B", "Loss percent", "Choke percent"};

        class DumpWriter
        {
        private:
            char* output_;
            uint32_t capacity_;
            uint32_t required_{};

        public:
            DumpWriter(char* output, uint32_t capacity) :
                output_(output),
                capacity_(capacity)
            {}

            void Print(const char* format, ...)
            {
                va_list arguments;
                va_start(arguments, format);

                uint32_t available = required_ < capacity_ ? capacity_ - required_ : 0;
                int written = std::vsnprintf(available ? output_ + required_ : nullptr, available, format, arguments);
                va_end(arguments);

                if (written > 0)
                {
                    required_ += uint32_t(written);
                }
            }

            uint32_t size() const
            {
                return required_ + 1;
            }
        };

        void WriteMapper(DumpWriter& writer, const PerfMapperSnapshot& mapper)
        {
            writer.Print(
                "Mapper: level=%d map=%d view=%d wpoly=%llu epoly=%llu\n",
                mapper.level,
                mapper.map_loaded,
                mapper.view_measured,
                mapper.wpoly,
                mapper.epoly
            );
            if (mapper.map_entities_available)
            {
                writer.Print("Brush entities: map=%u pvs=%u\n", mapper.brush_entities, mapper.pvs_brush_entities);
            }
            else
            {
                writer.Print("Brush entities: map=- pvs=%u\n", mapper.pvs_brush_entities);
            }
            writer.Print(
                "Position: %.1f %.1f %.1f | Angles: %.1f %.1f %.1f\n",
                mapper.position.x,
                mapper.position.y,
                mapper.position.z,
                mapper.angles.x,
                mapper.angles.y,
                mapper.angles.z
            );
            if (mapper.target_hit)
            {
                writer.Print("Look: %.1f %.1f %.1f", mapper.target_position.x, mapper.target_position.y, mapper.target_position.z);
            }
            else
            {
                writer.Print("Look: -");
            }
            if (mapper.level < 2)
            {
                writer.Print("\n");
                return;
            }

            if (mapper.target_hit)
            {
                writer.Print(" | Object: %s%s\n", mapper.target_name.c_str(), mapper.target_bounds ? " [bounds]" : "");
                writer.Print("Object origin: %.1f %.1f %.1f\n", mapper.target_origin.x, mapper.target_origin.y, mapper.target_origin.z);
                for (const PerfMapProperty& property : mapper.properties)
                {
                    writer.Print("  %s: %s\n", property.name.c_str(), property.value.c_str());
                }
            }
            else
            {
                writer.Print(" | Object: -\n");
                writer.Print("Object origin: -\n");
            }
        }
    } // namespace

    uint32_t dump::Format(
        const PerfSnapshot& snapshot,
        char* output,
        uint32_t capacity,
        const PerfNetworkSnapshot* network,
        const PerfMapperSnapshot* mapper
    )
    {
        DumpWriter writer(output, capacity);
#ifdef _WIN32
        const char* platform = "Windows";
#else
        const char* platform = "Linux";
#endif
#if USE_PROFILER
        const char* build = "diagnostic";
#else
        const char* build = "ordinary";
#endif
        writer.Print("Platform=%s pointer_bits=%u build=%s engine=%d\n", platform, uint32_t(sizeof(void*) * 8), build, snapshot.engine_build);
#if USE_PROFILER
        const tracy::QueueMemoryStatus queue = tracy::QueueMemory_GetStatus();
        const char* stop_reason = queue.stopped == 1 ? "memory limit" : (queue.stopped == 2 ? "allocation failure" : "none");
        writer.Print(
            "Tracy queues: used=%llu peak=%llu limit=%llu bytes stopped=%u reason=%s\n",
            uint64_t(queue.used),
            uint64_t(queue.peak),
            uint64_t(queue.limit),
            queue.stopped,
            stop_reason
        );
#endif
        writer.Print(
            "GL vendor=%s renderer=%s version=%s timer_bits=%d\n",
            snapshot.gl_vendor,
            snapshot.gl_renderer,
            snapshot.gl_version,
            snapshot.timer_bits
        );
        writer.Print(
            "fps_max=%.2f gl_vsync=%d r_speeds=%d r_studio_shader=%d presenter=%d\n",
            snapshot.fps_cap,
            snapshot.vsync,
            snapshot.r_speeds,
            snapshot.studio_shader,
            snapshot.presenter_available
        );
        writer.Print(
            "NextClient performance: mode=%d gpu=%d supported=%d trace=%d\n",
            snapshot.mode,
            snapshot.gpu_requested,
            snapshot.gpu_supported,
            snapshot.trace_active
        );
        writer.Print(
            "Output=%llu Host=%llu generation=%u context=%u map=%u\n",
            snapshot.latest.output_id,
            snapshot.latest.frame_id,
            snapshot.collector_generation,
            snapshot.context_generation,
            snapshot.map_generation
        );
        writer.Print(
            "History: %.3f s, %u records, capacity limited=%d\n",
            double(snapshot.coverage_ns) / 1e9,
            snapshot.record_count,
            snapshot.capacity_limited
        );
        writer.Print("Coverage: %s\n", snapshot.coverage_reason);
        writer.Print("Panel scale=%.2f (0=automatic)\n", snapshot.panel_scale);
        for (const performance::SeriesDescriptor& series : performance::kSeries)
        {
            const PerfSummary& summary = snapshot.summaries[uint32_t(series.id)];
            const char* unit = performance::UnitLabel(series.unit);
            writer.Print(
                "%s: %s count=%u mean=%.3f %s median=%.3f %s max=%.3f %s\n",
                series.name,
                stages::StateName(summary.state),
                summary.count,
                double(summary.mean_ns) / 1e6,
                unit,
                double(summary.median_ns) / 1e6,
                unit,
                double(summary.maximum_ns) / 1e6,
                unit
            );
        }
        for (const performance::GaugeDescriptor& gauge : performance::kGauges)
        {
            uint32_t index = uint32_t(gauge.id);
            const PerfScalarValue& value = snapshot.gauges[index];
            const PerfScalarSummary& summary = snapshot.gauge_summaries[index];
            const char* unit = performance::UnitLabel(gauge.unit);
            writer.Print("%s: last=", gauge.name);

            if (value.state == ncl_perf::PerfSampleState::Measured)
            {
                writer.Print("%g %s (%s)", value.value, unit, stages::StateName(value.state));
            }
            else
            {
                writer.Print("%s", stages::StateName(value.state));
            }

            if (summary.state == ncl_perf::PerfSampleState::Measured)
            {
                writer.Print(" count=%u mean=%g %s max=%g %s\n", summary.count, summary.mean, unit, summary.maximum, unit);
            }
            else
            {
                writer.Print(
                    " count=0 mean=%s max=%s\n",
                    stages::StateName(summary.state),
                    stages::StateName(summary.state)
                );
            }
        }
        const PerfSummary& intervals = snapshot.summaries[0];
        writer.Print(
            "FPS=%.2f percentiles=%d p95=%.3f ms p99=%.3f ms\n",
            intervals.fps,
            intervals.percentiles_available,
            double(intervals.p95_ns) / 1e6,
            double(intervals.p99_ns) / 1e6
        );
        writer.Print(
            "CPU totals: %u frames over %.3f ms; CPU collection (latest Host)=%.3f ms; GPU poll (latest output)=%.3f ms\n",
            snapshot.cpu_frame_count,
            double(snapshot.cpu_window_ns) / 1e6,
            double(snapshot.cpu_collection_ns) / 1e6,
            double(snapshot.gpu_poll_ns) / 1e6
        );
        for (uint32_t index = 0; index < snapshot.cpu_count; ++index)
        {
            const ncl_perf::PerfCpuSample& sample = snapshot.cpu_tree[index];
            writer.Print(
                "CPU %u parent=%u %s: %s incl=%.3f self=%.3f ms calls=%llu\n",
                sample.scope_id,
                sample.parent_id,
                sample.name,
                stages::StateName(sample.state),
                double(sample.inclusive_ns) / 1e6,
                double(sample.self_ns) / 1e6,
                sample.calls
            );
        }
        writer.Print(
            "Worker: status=%u map=%u duration=%.3f ms\n",
            uint32_t(snapshot.worker.status),
            snapshot.worker.map_generation,
            double(snapshot.worker.duration_ns) / 1e6
        );
        writer.Print("GPU output=%llu age=%u\n", snapshot.gpu_output_id, snapshot.gpu_age);
        for (uint32_t index = 0; index < snapshot.gpu_count; ++index)
        {
            const PerfGpuSample& sample = snapshot.gpu_passes[index];
            writer.Print(
                "GPU %u %s: %s duration=%.3f ms calls=%u\n",
                sample.scope_id,
                stages::ScopeName(sample.scope_id),
                stages::StateName(sample.state),
                double(sample.duration_ns) / 1e6,
                sample.calls
            );
        }
        for (const performance::CounterDescriptor& counter : performance::kCounters)
        {
            writer.Print("%s=%llu\n", counter.name, snapshot.counters[uint32_t(counter.id)]);
        }
        writer.Print(
            "CPU frame=%llu\nLast shader compile=%.3f ms (%s) link=%.3f ms (%s)\n",
            snapshot.cpu_frame_id,
            double(snapshot.shader_compile_ns) / 1e6,
            stages::StateName(snapshot.shader_compile_state),
            double(snapshot.shader_link_ns) / 1e6,
            stages::StateName(snapshot.shader_link_state)
        );
        if (network)
        {
            writer.Print(
                "Network: enabled=%d connected=%d demo=%d records=%u coverage=%.3f s packet_age=%.3f ms\n",
                network->enabled,
                network->connected,
                network->demo_playback,
                network->count,
                double(network->coverage_ns) / 1e9,
                network->packet_age_ms
            );

            for (uint32_t index = 0; index < uint32_t(PerfNetworkSeries::Count); ++index)
            {
                const PerfNetworkValue& value = network->latest[index];
                const PerfNetworkSummary& summary = network->summaries[index];
                writer.Print(
                    "Network %s: %s latest=%.3f count=%u avg=%.3f median=%.3f max=%.3f\n",
                    kNetworkSeriesNames[index],
                    stages::StateName(value.state),
                    value.value,
                    summary.count,
                    summary.mean,
                    summary.median,
                    summary.maximum
                );
            }
            writer.Print(
                "Network packet window: capacity=%u incoming=%u outgoing=%u revision=%llu\n",
                uint32_t(network->graphs[0].size()),
                network->incoming_packet_count,
                network->outgoing_packet_count,
                static_cast<unsigned long long>(network->revision)
            );

            for (uint32_t index = 0; index < std::size(kNetworkPacketNames); ++index)
            {
                const PerfNetworkSummary& summary = network->packet_summaries[index];
                writer.Print(
                    "Network packets %s: count=%u avg=%.3f median=%.3f max=%.3f\n",
                    kNetworkPacketNames[index],
                    summary.count,
                    summary.mean,
                    summary.median,
                    summary.maximum
                );
            }
        }
        if (mapper)
        {
            WriteMapper(writer, *mapper);
        }
        return writer.size();
    }

    uint32_t dump::FormatMapper(const PerfMapperSnapshot& mapper, char* output, uint32_t capacity)
    {
        DumpWriter writer(output, capacity);
        WriteMapper(writer, mapper);
        return writer.size();
    }
} // namespace performance
