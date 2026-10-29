#include "performance/PerformanceMonitor.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>

#include "performance/performance_stages.h"
#include "performance/PerformanceMapper.h"
#include "performance/PerformanceMetrics.h"
#include "performance/GpuQueryCollector.h"
#include "performance/PerformanceTraceBackend.h"

namespace performance
{
    namespace
    {
        class PerformanceClock : public ncl_perf::PerfClockInterface
        {
        public:
            ncl_perf::PerfTimestamp_t Now() override
            {
                return std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
            }
        };

        class CollectionMeasurement
        {
        private:
            ncl_perf::PerfClockInterface* clock_{};
            ncl_perf::PerfDuration_t& total_;
            ncl_perf::PerfTimestamp_t start_{};

        public:
            CollectionMeasurement(ncl_perf::PerfClockInterface& clock, ncl_perf::PerfDuration_t& total, bool enabled) :
                clock_(enabled ? &clock : nullptr),
                total_(total),
                start_(enabled ? clock.Now() : 0)
            {}

            ~CollectionMeasurement()
            {
                if (clock_)
                {
                    total_ += clock_->Now() - start_;
                }
            }
        };

        struct PerformanceCore
        {
            PerformanceClock clock;
            PerformanceMonitor monitor{clock};
            PerformanceMapper mapper;
        };

        PerformanceCore& GetCore()
        {
            static PerformanceCore core;
            return core;
        }

        void AccumulateOutputCost(PerfFrameRecord& record, ncl_perf::PerfDuration_t duration, ncl_perf::PerfScopeId_t scope_id = 0)
        {
            for (const performance::SeriesDescriptor& series : performance::kSeries)
            {
                if (series.source == performance::SeriesSource::MonitorOutputCost && (!scope_id || series.scope_id == scope_id))
                {
                    PerfValue& value = record.values[uint32_t(series.id)];
                    value.duration_ns += duration;
                    value.state = ncl_perf::PerfSampleState::Measured;
                }
            }
        }

        void ApplyGpuValue(PerfValue& value, const PerfGpuResult& result)
        {
            if (result.state != ncl_perf::PerfSampleState::Measured)
            {
                value.state = result.state;
                value.duration_ns = 0;
            }
            else if (value.state != ncl_perf::PerfSampleState::Incomplete && value.state != ncl_perf::PerfSampleState::Dropped)
            {
                value.duration_ns += result.duration_ns;
                value.state = ncl_perf::PerfSampleState::Measured;
            }
        }
    } // namespace

    PerformanceMonitor::PerformanceMonitor(ncl_perf::PerfClockInterface& clock) :
        clock_(clock),
        accounting_(clock)
    {
        stages::Register(accounting_);
        stage_available_.fill(true);
    }

    PerformanceMonitor::~PerformanceMonitor() = default;

    ncl_perf::PerfScopeToken_t PerformanceMonitor::BeginCpu(ncl_perf::PerfScopeId_t scope_id, std::source_location location)
    {
        const performance::StageDescriptor* stage = performance::Stage(scope_id);

        if (stage && stage->cpu_level == performance::CollectionLevel::StudioDetail && !get_studio_detail_monitor())
        {
            return 0;
        }

        if (shutdown_ || (!cpu_enabled_ && (!trace_backend_ || !trace_backend_->IsConnected())))
        {
            return 0;
        }

        CollectionMeasurement measurement(clock_, cpu_collection_ns_, cpu_enabled_);

        if (!stage || !stage_available_[scope_id] || !performance::HasCapability(*stage, ncl_perf::PerfScopeCapability::Cpu))
        {
            return 0;
        }

        ScopeSlot* slot = ReserveSlot(cpu_slots_);

        if (!slot)
        {
            return 0;
        }

        slot->collector = accounting_.Begin(scope_id);
        slot->trace =
            scope_id != PerfStage::Host && trace_backend_ && trace_backend_->IsConnected() ? trace_backend_->BeginCpu(scope_id, location) : 0;

        if (!slot->collector && !slot->trace)
        {
            *slot = {};
        }

        return slot->token;
    }

    void PerformanceMonitor::EndCpu(ncl_perf::PerfScopeToken_t token)
    {
        CollectionMeasurement measurement(clock_, cpu_collection_ns_, cpu_enabled_ && token != 0);
        ScopeSlot* slot = FindSlot(cpu_slots_, token);

        if (!slot)
        {
            return;
        }

        if (slot->trace)
        {
            trace_backend_->EndCpu(slot->trace);
        }

        ncl_perf::PerfScopeToken_t collector = slot->collector;
        *slot = {};

        if (!collector)
        {
            return;
        }

        accounting_.End(collector);

        if (accounting_.last_scope_id() == PerfStage::ShaderCompile)
        {
            shader_compile_ns_ = accounting_.last_duration_ns();
            shader_compile_state_ = ncl_perf::PerfSampleState::Measured;
        }
        else if (accounting_.last_scope_id() == PerfStage::ShaderLink)
        {
            shader_link_ns_ = accounting_.last_duration_ns();
            shader_link_state_ = ncl_perf::PerfSampleState::Measured;
        }
        else if (output_draft_.output_id && accounting_.last_scope_id())
        {
            AccumulateOutputCost(output_draft_, accounting_.last_duration_ns(), accounting_.last_scope_id());
        }
    }

    ncl_perf::PerfScopeToken_t PerformanceMonitor::BeginGpu(ncl_perf::PerfScopeId_t scope_id, std::source_location location)
    {
        if (shutdown_ || !stages::HasCapability(scope_id, ncl_perf::PerfScopeCapability::Gpu) ||
            (!mode_ && (!trace_backend_ || !trace_backend_->IsConnected())) || (scope_id == PerfStage::Screen && screen_token_))
        {
            return 0;
        }

        ScopeSlot* slot = ReserveSlot(gpu_slots_);

        if (!slot)
        {
            return 0;
        }

        slot->collector = BeginCollectorGpu(scope_id);
        slot->trace = trace_backend_ && trace_backend_->IsConnected() ? trace_backend_->BeginGpu(scope_id, location) : 0;

        if (!slot->collector && !slot->trace)
        {
            *slot = {};
        }

        if (scope_id == PerfStage::Screen && slot->token)
        {
            screen_token_ = render_token_ = slot->token;
            screen_output_ = reserved_output_;
        }

        return slot->token;
    }

    ncl_perf::PerfScopeToken_t PerformanceMonitor::BeginCollectorGpu(ncl_perf::PerfScopeId_t scope_id)
    {
        if (!gpu_ || !mode_ || !main_window_)
        {
            return 0;
        }

        if (!reserved_output_)
        {
            reserved_output_ = output_draft_.output_id ? output_draft_.output_id : ++output_id_;
            staged_gpu_ = {};

            for (const performance::SeriesDescriptor& series : performance::kSeries)
            {
                if (series.source == performance::SeriesSource::GpuRollup &&
                    series.availability == performance::AvailabilityPolicy::DelayedGpuInactiveWhenUnused)
                {
                    staged_gpu_[uint32_t(series.id)].state = ncl_perf::PerfSampleState::Inactive;
                }
            }
        }

        uint64_t dropped_before = gpu_->dropped_scopes();
        ncl_perf::PerfScopeToken_t token = gpu_->Begin(accepted_ ? frame_id_ : 0, reserved_output_, scope_id);
        gpu_dropped_ += gpu_->dropped_scopes() - dropped_before;
        PerfSeries series = stages::GpuSeries(scope_id);

        if (series != PerfSeries::Count)
        {
            staged_gpu_[uint32_t(series)].state = token ? ncl_perf::PerfSampleState::Pending : ncl_perf::PerfSampleState::Dropped;
        }

        return token;
    }

    void PerformanceMonitor::EndGpu(ncl_perf::PerfScopeToken_t token)
    {
        ScopeSlot* slot = FindSlot(gpu_slots_, token);

        if (!slot)
        {
            return;
        }

        if (slot->trace)
        {
            trace_backend_->EndGpu(slot->trace);
        }

        if (gpu_ && slot->collector)
        {
            gpu_->End(slot->collector);
        }
        *slot = {};

        if (token == screen_token_)
        {
            render_token_ = 0;
            screen_token_ = 0;

            if (last_output_.output_id != screen_output_ && output_draft_.output_id != screen_output_)
            {
                if (gpu_)
                {
                    gpu_->CancelOutput(screen_output_);
                }
            }

            if (reserved_output_ == screen_output_)
            {
                reserved_output_ = 0;
            }

            screen_output_ = 0;
        }
    }

    void PerformanceMonitor::AddCounter(PerfCounterId counter, uint64_t value)
    {
        if (performance::Counter(counter) && get_counter_monitor())
        {
            counters_[uint32_t(counter)] += value;
        }
    }

    bool PerformanceMonitor::SetGauge(PerfGaugeId gauge, double value)
    {
        if (!frame_accepted_ || host_aborted_ || !performance::Gauge(gauge) || !std::isfinite(value) || !get_counter_monitor())
        {
            return false;
        }

        gauges_[uint32_t(gauge)] = {value, ncl_perf::PerfSampleState::Measured};

        return true;
    }

    bool PerformanceMonitor::GetSnapshot(PerfSnapshot& output) const
    {
        if (publisher_)
        {
            publisher_->CopySnapshot(output);
        }
        else
        {
            output = {};
        }

        CopySnapshotMetadata(output);

        return !shutdown_;
    }

    void PerformanceMonitor::CopySnapshotMetadata(PerfSnapshot& output) const
    {
        output.mode = get_mode();
        output.collector_generation = generation_;
        output.context_generation = context_generation_;
        output.map_generation = map_generation_;
        output.gpu_requested = gpu_requested_;
        output.presenter_available = main_window_ != nullptr;
        output.trace_active = trace_active_;
        output.gpu_supported = context_alive_ && gpu_timestamp_supported_ && gpu_timer_bits_ > 0;
        output.timer_bits = gpu_timer_bits_;
        std::memcpy(output.gl_vendor, gl_vendor_, sizeof(gl_vendor_));
        std::memcpy(output.gl_renderer, gl_renderer_, sizeof(gl_renderer_));
        std::memcpy(output.gl_version, gl_version_, sizeof(gl_version_));
        output.fps_cap = fps_cap_;
        output.vsync = vsync_;
        output.r_speeds = r_speeds_;
        output.studio_shader = studio_shader_;
        output.engine_build = engine_build_;
        output.panel_scale = panel_scale_;
        output.gpu_output_id = gpu_output_;
        output.gpu_age = gpu_age_;
        output.gpu_count = gpu_pass_count_;
        output.gpu_passes = gpu_passes_;
        output.shader_compile_ns = shader_compile_ns_;
        output.shader_link_ns = shader_link_ns_;
        output.shader_compile_state = shader_compile_state_;
        output.shader_link_state = shader_link_state_;
        output.cpu_collection_ns = completed_cpu_collection_ns_;
        output.gpu_poll_ns = completed_gpu_poll_ns_;

        for (const performance::CounterDescriptor& counter : performance::kCounters)
        {
            if (counter.source == performance::CounterSource::GpuExpiredResults)
            {
                output.counters[uint32_t(counter.id)] = gpu_expired_;
            }
            else if (counter.source == performance::CounterSource::GpuDroppedScopes)
            {
                output.counters[uint32_t(counter.id)] = gpu_dropped_;
            }
        }

        output.gauges = completed_gauges_;
        output.worker = worker_;
        output.latest = last_output_;

        for (uint32_t index = 0; index < output.cpu_count; ++index)
        {
            if (!stage_available_[output.cpu_tree[index].scope_id])
            {
                output.cpu_tree[index].state = ncl_perf::PerfSampleState::Unavailable;
            }
        }

        std::strcpy(
            output.coverage_reason,
            main_window_ ? "Whole commands and prediction boundaries unresolved" : "SDL main-window presentation unavailable"
        );
    }

    bool PerformanceMonitor::GetPanelSnapshot(
        PerfSnapshot& output,
        PerfGroupedGraphs* groups,
        PerfFrameGraphs* frames,
        uint32_t frame_columns
    ) const
    {
        bool available = GetSnapshot(output);

        if (groups)
        {
            if (available && publisher_)
            {
                publisher_->CopyGroupedGraphs(*groups);
            }
            else
            {
                *groups = {};
            }
        }

        if (available && publisher_ && history_)
        {
            panel_projection_.Project(output, *history_, publisher_->get_revision(), publisher_->get_panel_fps(), frames, frame_columns);
        }
        else if (frames)
        {
            *frames = {};
        }

        return available;
    }

    uint32_t PerformanceMonitor::map_generation() const
    {
        return map_generation_;
    }

    void PerformanceMonitor::SetGraphicsInfo(const char* vendor, const char* renderer, const char* version)
    {
        std::snprintf(gl_vendor_, sizeof(gl_vendor_), "%s", vendor ? vendor : "Unavailable");
        std::snprintf(gl_renderer_, sizeof(gl_renderer_), "%s", renderer ? renderer : "Unavailable");
        std::snprintf(gl_version_, sizeof(gl_version_), "%s", version ? version : "Unavailable");
    }

    void PerformanceMonitor::SetEngineInfo(float fps_cap, int vsync, bool r_speeds, bool studio_shader, int engine_build)
    {
        fps_cap_ = fps_cap;
        vsync_ = vsync;
        r_speeds_ = r_speeds;
        studio_shader_ = studio_shader;
        engine_build_ = engine_build;
    }

    void PerformanceMonitor::ReportWorker(const PerfWorkerReport& report)
    {
        if (report.map_generation == map_generation_)
        {
            worker_ = report;
        }
    }

    int PerformanceMonitor::get_mode() const
    {
        return dump_.is_active() ? requested_mode_ : mode_;
    }

    int PerformanceMonitor::get_batch_level() const
    {
        return batch_.get_level();
    }

    bool PerformanceMonitor::RequestDumpCapture()
    {
        return !shutdown_ && dump_.Request();
    }

    bool PerformanceMonitor::TakeDumpCapture(PerfSnapshot& output)
    {
        return dump_.Take(output);
    }

    bool PerformanceMonitor::is_dump_ready() const
    {
        return dump_.is_ready();
    }

    const PerfSnapshot* PerformanceMonitor::get_dump_snapshot() const
    {
        return dump_.get_snapshot();
    }

    const PerfSnapshot* PerformanceMonitor::get_batch_snapshot() const
    {
        return !shutdown_ ? batch_.get_snapshot() : nullptr;
    }

    PerformanceMonitor* PerformanceMonitor::get_scope_monitor()
    {
        return !shutdown_ && (cpu_enabled_ || (trace_backend_ && trace_backend_->IsConnected())) ? this : nullptr;
    }

    PerformanceMonitor* PerformanceMonitor::get_counter_monitor()
    {
        return !shutdown_ && (mode_ || batch_.get_level() || (trace_backend_ && trace_backend_->IsConnected())) ? this : nullptr;
    }

    PerformanceMonitor* PerformanceMonitor::get_studio_detail_monitor()
    {
        return !shutdown_ &&
                       (dump_.is_active() || (batch_.get_level() == 2 && cpu_enabled_) || (trace_backend_ && trace_backend_->IsConnected()))
                   ? this
                   : nullptr;
    }

    void PerformanceMonitor::CancelDumpCapture()
    {
        dump_.Cancel();
        ApplyMode();
    }

    void PerformanceMonitor::CompleteDumpCapture(ncl_perf::PerfTimestamp_t completed_at)
    {
        history_->BuildSnapshot(completed_at, dump_.get_capture_buffer());
        CopySnapshotMetadata(dump_.get_capture_buffer());
        dump_.Complete(completed_at, completed_frame_id_, completed_tree_, completed_tree_count_);
        ApplyMode();
        EnsureGpu();
    }

    void PerformanceMonitor::SetBatchLevel(int level)
    {
        if (shutdown_ || !batch_.SetLevel(level))
        {
            return;
        }

        if (!host_depth_)
        {
            ApplyMode();
        }

        EnsureHistory();

        if (publisher_)
        {
            publisher_->InvalidateHistory();
        }

        if (!mode_ && !batch_.get_level())
        {
            accepted_ = false;
            counters_ = {};

            if (!get_counter_monitor())
            {
                gauges_ = {};
                completed_gauges_ = {};
            }
        }
    }

    void PerformanceMonitor::SetMode(int mode)
    {
        requested_mode_ = std::clamp(mode, 0, 3);

        if (!host_depth_ || !requested_mode_)
        {
            ApplyMode();
        }
    }

    void PerformanceMonitor::SetPanelScale(float scale)
    {
        panel_scale_ = std::isfinite(scale) && scale > 0 ? std::clamp(scale, 0.5f, 3.0f) : 0;
    }

    void PerformanceMonitor::SetGpuEnabled(bool enabled)
    {
        gpu_requested_ = enabled;

        if (enabled)
        {
            EnsureGpu();
        }
        else
        {
            ReleaseGpu();
        }
    }

    void PerformanceMonitor::Reset()
    {
        CancelDumpCapture();
        batch_.Reset();
        ++generation_;
        shader_compile_ns_ = 0;
        shader_link_ns_ = 0;
        shader_compile_state_ = ncl_perf::PerfSampleState::Unavailable;
        shader_link_state_ = ncl_perf::PerfSampleState::Unavailable;
        completed_frame_id_ = 0;
        cpu_collection_ns_ = 0;
        completed_cpu_collection_ns_ = 0;
        gpu_poll_ns_ = 0;
        completed_gpu_poll_ns_ = 0;

        if (gpu_)
        {
            gpu_->Reset(generation_);
        }

        for (ScopeSlot& slot : gpu_slots_)
        {
            slot.collector = 0;
        }

        reserved_output_ = 0;
        screen_output_ = 0;
        gpu_output_ = 0;
        gpu_age_ = 0;
        gpu_pass_count_ = 0;
        gpu_passes_ = {};
        gpu_expired_ = 0;
        gpu_dropped_ = 0;
        accounting_.Reset(++cpu_generation_);

        for (ScopeSlot& slot : cpu_slots_)
        {
            slot.collector = 0;
        }

        if (history_)
        {
            history_->Reset(generation_);
        }

        accepted_ = false;
        output_draft_ = {};
        last_output_ = {};
        has_swap_baseline_ = false;
        completed_tree_count_ = 0;
        completed_tree_ = {};

        if (publisher_)
        {
            ncl_perf::PerfTimestamp_t now = mode_ || batch_.get_level() ? clock_.Now() : 0;
            publisher_->Reset(generation_, now);
        }

        counters_ = {};
        gauges_ = {};
        completed_gauges_ = {};
    }

    void PerformanceMonitor::OnMapChanged()
    {
        CancelDumpCapture();
        ++map_generation_;
        batch_.Reset();
        gauges_ = {};
        completed_gauges_ = {};
        worker_ = {};
        completed_tree_count_ = 0;
        completed_tree_ = {};

        if (publisher_)
        {
            ncl_perf::PerfTimestamp_t now = mode_ || batch_.get_level() ? clock_.Now() : 0;
            publisher_->Reset(generation_, now);
        }

        completed_frame_id_ = 0;
        InvalidatePendingGpu();

        if (gpu_)
        {
            gpu_->Reset(generation_);
        }

        for (ScopeSlot& slot : gpu_slots_)
        {
            slot.collector = 0;
        }

        reserved_output_ = 0;
        staged_gpu_ = {};
    }

    void PerformanceMonitor::OnContextCreated(void* context)
    {
        if (context_alive_ && context && context_identity_ != context)
        {
            AbandonGpu();
        }

        if (!context_alive_)
        {
            ++context_generation_;
            context_alive_ = true;
        }

        context_identity_ = context;
        RefreshGpuCapabilities();
        EnsureGpu();
    }

    void PerformanceMonitor::OnContextDestroyed()
    {
        ReleaseGpu();
        InvalidatePendingGpu();
        context_alive_ = false;
        context_identity_ = nullptr;
    }

    void PerformanceMonitor::Shutdown()
    {
        AbandonGpu();
        SetMode(0);
        Reset();
        SetMainWindow(nullptr);
        shutdown_ = true;
    }

    void PerformanceMonitor::OnHostEntry()
    {
        if (!host_depth_)
        {
            ApplyMode();
            accepted_ = false;
            frame_accepted_ = false;
            host_aborted_ = false;
            // The native parent must begin outside the Host_FilterTime hook's zones.
            if (!shutdown_ && trace_backend_ && trace_backend_->IsConnected())
            {
                trace_host_token_ = trace_backend_->BeginCpu(PerfStage::Host);
            }
        }
        ++host_depth_;
    }

    void PerformanceMonitor::OnFrameAccepted()
    {
        if (shutdown_ || host_depth_ != 1 || frame_accepted_ || host_aborted_)
        {
            return;
        }

        frame_accepted_ = true;
        gauges_ = {};

        if (dump_.ActivatePending())
        {
            ApplyMode();
            dump_.SetStartedAt(clock_.Now());
        }
        ++frame_id_;
        batch_.BeginFrame();

        if (cpu_enabled_)
        {
            accounting_.Reset(++cpu_generation_);
            cpu_collection_ns_ = 0;
        }

        if (get_counter_monitor())
        {
            counters_ = {};
            accepted_ = mode_ || batch_.get_level();
        }

        host_token_ = BeginCpu(PerfStage::Host);
    }

    void PerformanceMonitor::OnHostExit(bool completed)
    {
        if (!host_depth_)
        {
            return;
        }

        if (!completed)
        {
            DiscardHostFrame();
        }

        if (--host_depth_ || host_aborted_)
        {
            return;
        }

        EndCpu(host_token_);
        host_token_ = 0;

        if (trace_backend_ && trace_host_token_)
        {
            trace_backend_->EndCpu(trace_host_token_);
            trace_host_token_ = 0;
        }

        if (trace_backend_ && frame_accepted_)
        {
            if (trace_backend_->IsConnected())
            {
                for (const performance::CounterDescriptor& counter : performance::kCounters)
                {
                    uint64_t value = counters_[uint32_t(counter.id)];

                    switch (counter.source)
                    {
                        case performance::CounterSource::CpuDroppedScopes:
                            value = cpu_enabled_ ? accounting_.dropped_scopes() : 0;
                            break;
                        case performance::CounterSource::GpuDroppedScopes:
                            value = gpu_dropped_;
                            break;
                        case performance::CounterSource::GpuExpiredResults:
                            value = gpu_expired_;
                            break;
                        default:
                            break;
                    }

                    trace_backend_->RecordCounter(counter.id, value);
                }

                for (const performance::GaugeDescriptor& gauge : performance::kGauges)
                {
                    const PerfScalarValue& value = gauges_[uint32_t(gauge.id)];

                    if (value.state == ncl_perf::PerfSampleState::Measured)
                    {
                        trace_backend_->RecordGauge(gauge.id, value.value);
                    }
                }
            }

            trace_backend_->MarkFrame(false, frame_id_);
        }

        if (frame_accepted_)
        {
            completed_gauges_ = gauges_;
        }

        frame_accepted_ = false;

        if (!accepted_)
        {
            trace_active_ = trace_backend_ && trace_backend_->IsConnected();
            counters_ = {};
            gauges_ = {};
            return;
        }

        if (cpu_enabled_)
        {
            completed_tree_count_ = accounting_.CopySamples(completed_tree_.data(), uint32_t(completed_tree_.size()));
            completed_frame_id_ = frame_id_;
            completed_cpu_collection_ns_ = cpu_collection_ns_;

            counters_[uint32_t(PerfCounterId::CpuDroppedScopes)] = accounting_.dropped_scopes();
        }
        else
        {
            completed_tree_count_ = 0;
        }

        PerfFrameRecord record;
        record.kind = PerfRecordKind::Host;
        record.frame_id = frame_id_;
        record.completed_at = clock_.Now();
        record.collector_generation = generation_;
        record.context_generation = context_generation_;
        record.map_generation = map_generation_;
        record.counters = counters_;
        record.gauges = gauges_;

        for (uint32_t index = 0; index < completed_tree_count_; ++index)
        {
            const ncl_perf::PerfCpuSample& sample = completed_tree_[index];

            for (const performance::SeriesDescriptor& series : performance::kSeries)
            {
                if (series.source == performance::SeriesSource::CpuInclusive && series.scope_id == sample.scope_id)
                {
                    PerfValue& value = record.values[uint32_t(series.id)];
                    value.duration_ns += sample.inclusive_ns;
                    value.state = stage_available_[sample.scope_id] ? sample.state : ncl_perf::PerfSampleState::Unavailable;
                }
            }
        }

        history_->Append(record);

        if (cpu_enabled_)
        {
            publisher_->CompleteCpuFrame(
                completed_frame_id_,
                std::span<const ncl_perf::PerfCpuSample>(completed_tree_.data(), completed_tree_count_),
                record.completed_at
            );
        }

        AdvanceSnapshot(record.completed_at);
        batch_.CompleteFrame(record, std::span<const ncl_perf::PerfCpuSample>(completed_tree_.data(), completed_tree_count_));

        counters_ = {};
        gauges_ = {};
        accepted_ = false;
        host_token_ = 0;
        accounting_.ClearFrame();

        if (dump_.is_active())
        {
            CompleteDumpCapture(record.completed_at);
        }
    }

    void PerformanceMonitor::SetMainWindow(void* window)
    {
        if (main_window_ == window)
        {
            return;
        }

        main_window_ = window;
        has_swap_baseline_ = false;
    }

    void PerformanceMonitor::OnWindowCreated(void* window, bool opengl)
    {
        if (window && opengl && !main_window_)
        {
            SetMainWindow(window);
        }
    }

    void PerformanceMonitor::OnWindowDestroyed(void* window)
    {
        if (window && window == main_window_)
        {
            OnContextDestroyed();
            SetMainWindow(nullptr);
        }
    }

    PerfOutputId_t PerformanceMonitor::BeginOutput(void* window)
    {
        if (shutdown_ || (!mode_ && (!trace_backend_ || !trace_backend_->IsConnected())) || !main_window_ || window != main_window_)
        {
            return 0;
        }

        trace_output_ = reserved_output_ ? reserved_output_ : ++output_id_;

        if (!mode_)
        {
            return trace_output_;
        }

        output_draft_ = {};
        output_poll_start_ = gpu_poll_ns_;
        output_draft_.frame_id = accepted_ ? frame_id_ : 0;
        output_draft_.output_id = trace_output_;
        output_draft_.collector_generation = generation_;
        output_draft_.context_generation = context_generation_;
        output_draft_.map_generation = map_generation_;

        if (reserved_output_)
        {
            output_draft_.values = staged_gpu_;
        }

        swap_start_ = clock_.Now();

        return output_draft_.output_id;
    }

    void PerformanceMonitor::MarkSwapStart()
    {
        swap_token_ = BeginCpu(PerfStage::Swap);

        if (output_draft_.output_id)
        {
            swap_start_ = clock_.Now();
        }
    }

    void PerformanceMonitor::EndOutput(PerfOutputId_t output_id)
    {
        bool presented = output_id && output_id == trace_output_;

        if (presented)
        {
            EndCpu(swap_token_);
            swap_token_ = 0;

            if (trace_backend_)
            {
                trace_backend_->MarkFrame(true, output_id);
            }

            trace_output_ = 0;
        }

        if (!output_id || output_id != output_draft_.output_id || !mode_)
        {
            if (presented)
            {
                AdvanceGpuAge(output_id);
            }

            return;
        }

        ncl_perf::PerfTimestamp_t now = clock_.Now();
        output_draft_.completed_at = now;
        output_draft_.values[uint32_t(PerfSeries::SwapCpu)] = {now - swap_start_, ncl_perf::PerfSampleState::Measured};
        output_draft_.values[0] = has_swap_baseline_ ? PerfValue{now - last_swap_, ncl_perf::PerfSampleState::Measured} : PerfValue{};
        last_swap_ = now;
        has_swap_baseline_ = true;
        ncl_perf::PerfDuration_t poll_before = gpu_poll_ns_;
        PollGpu();
        ncl_perf::PerfDuration_t poll_cost = gpu_poll_ns_ - poll_before;
        completed_gpu_poll_ns_ = gpu_poll_ns_ - output_poll_start_;

        if (poll_cost)
        {
            AccumulateOutputCost(output_draft_, poll_cost);
        }

        last_output_ = output_draft_;

        if (!accepted_)
        {
            counters_[uint32_t(PerfCounterId::CpuDroppedScopes)] = accounting_.dropped_scopes();
            output_draft_.counters = counters_;
            counters_ = {};
        }

        history_->Append(output_draft_);

        if (!accepted_)
        {
            AdvanceSnapshot(now);
        }

        AdvanceGpuAge(output_id);
        output_draft_ = {};
        reserved_output_ = 0;
    }

    void PerformanceMonitor::SetStageAvailable(ncl_perf::PerfScopeId_t scope_id, bool available)
    {
        stage_available_[scope_id] = available;
    }

    void PerformanceMonitor::EnsureHistory()
    {
        if (!history_ && (mode_ || batch_.get_level()))
        {
            history_ = std::make_unique<PerfFrameHistory>();
            history_->Reset(generation_);
            publisher_ = std::make_unique<performance::PerfSnapshotPublisher>();
            publisher_->Reset(generation_, 0);
        }
    }

    void PerformanceMonitor::ApplyMode()
    {
        int effective_mode = dump_.is_active() ? std::max(requested_mode_, 1) : requested_mode_;
        bool cpu_enabled = effective_mode || batch_.get_level() == 2;

        if (shutdown_ || (mode_ == effective_mode && cpu_enabled_ == cpu_enabled))
        {
            return;
        }

        bool collection_changed = cpu_enabled_ != cpu_enabled;
        mode_ = effective_mode;
        cpu_enabled_ = cpu_enabled;
        EnsureHistory();

        if (publisher_ && collection_changed)
        {
            ncl_perf::PerfTimestamp_t now = cpu_enabled_ ? clock_.Now() : 0;
            publisher_->Reset(generation_, now);

            if (cpu_enabled_)
            {
                publisher_->Advance(now, *history_);
            }
        }

        accounting_.SetEnabled(cpu_enabled_);

        if (!get_counter_monitor())
        {
            gauges_ = {};
            completed_gauges_ = {};
        }

        if (mode_)
        {
            EnsureGpu();
        }

        if (!mode_)
        {
            ReleaseGpu();
            output_draft_ = {};
            has_swap_baseline_ = false;
        }

        if (!cpu_enabled_)
        {
            accounting_.Reset(++cpu_generation_);

            for (ScopeSlot& slot : cpu_slots_)
            {
                slot.collector = 0;
            }

            accepted_ = accepted_ && batch_.get_level() != 0;
            output_draft_ = {};
            has_swap_baseline_ = false;
        }
    }

    ncl_perf::PerfClockInterface& get_clock()
    {
        return GetCore().clock;
    }

    PerformanceMapper& GetMapper()
    {
        return GetCore().mapper;
    }

    PerformanceMonitor& GetMonitor()
    {
        return GetCore().monitor;
    }

    void PerformanceMonitor::SetGpuBackend(GpuQueryBackendInterface* backend)
    {
        if (gpu_backend_ != backend)
        {
            ReleaseGpu();
            gpu_backend_ = backend;
        }

        RefreshGpuCapabilities();
        EnsureGpu();
    }

    void PerformanceMonitor::CloseRenderGpu()
    {
        EndGpu(render_token_);
        render_token_ = 0;
    }

    void PerformanceMonitor::PollGpu()
    {
        CollectionMeasurement measurement(clock_, gpu_poll_ns_, mode_ != 0);

        if (trace_backend_)
        {
            trace_backend_->Collect();
        }

        if (!gpu_)
        {
            return;
        }

        std::array<PerfGpuResult, 128> results;
        uint32_t count = 0;
        do
        {
            count = gpu_->Poll(results.data(), uint32_t(results.size()));

            for (uint32_t index = 0; index < count; ++index)
            {
                const PerfGpuResult& result = results[index];
                bool applied = false;

                if (output_draft_.output_id == result.output_id)
                {
                    applied = true;

                    if (result.series != PerfSeries::Count)
                    {
                        ApplyGpuValue(output_draft_.values[uint32_t(result.series)], result);
                    }
                }
                else if (reserved_output_ == result.output_id)
                {
                    applied = true;

                    if (result.series != PerfSeries::Count)
                    {
                        ApplyGpuValue(staged_gpu_[uint32_t(result.series)], result);
                    }
                }
                else if (history_->ApplyGpu(result))
                {
                    applied = true;
                    publisher_->InvalidateHistory();

                    if (last_output_.output_id == result.output_id && result.series != PerfSeries::Count)
                    {
                        ApplyGpuValue(last_output_.values[uint32_t(result.series)], result);
                    }
                }
                else if (last_output_.output_id >= result.output_id)
                {
                    ++gpu_expired_;
                }

                if (applied)
                {
                    SaveGpuPass(result);
                }
            }
        } while (count == results.size());
    }

    void PerformanceMonitor::AdvanceSnapshot()
    {
        if (!shutdown_ && (mode_ || batch_.get_level()) && publisher_)
        {
            AdvanceSnapshot(clock_.Now());
        }
        else
        {
            trace_active_ = trace_backend_ && trace_backend_->IsConnected();
        }
    }

    void PerformanceMonitor::AdvanceSnapshot(ncl_perf::PerfTimestamp_t now)
    {
        trace_active_ = trace_backend_ && trace_backend_->IsConnected();
        publisher_->Advance(now, *history_);
    }

    void PerformanceMonitor::RefreshGpuCapabilities()
    {
        gpu_timestamp_supported_ = gpu_backend_ && gpu_backend_->IsTimestampSupported();
        gpu_timer_bits_ = gpu_backend_ ? gpu_backend_->counter_bits() : 0;
    }

    void PerformanceMonitor::EnsureGpu()
    {
        if (!gpu_ && requested_mode_ && mode_ && gpu_requested_ && context_alive_ && gpu_backend_ && gpu_timestamp_supported_ &&
            gpu_timer_bits_ > 0)
        {
            gpu_ = std::make_unique<GpuQueryCollector>(*gpu_backend_, generation_, context_generation_);
        }
    }

    void PerformanceMonitor::ReleaseGpu()
    {
        if (gpu_)
        {
            if (context_alive_)
            {
                gpu_->ReleaseGl();
            }
            else
            {
                gpu_->AbandonGl();
            }

            gpu_.reset();
        }

        for (ScopeSlot& slot : gpu_slots_)
        {
            slot.collector = 0;
        }

        reserved_output_ = 0;
        InvalidatePendingGpuValues();
    }

    void PerformanceMonitor::AbandonGpu()
    {
        context_alive_ = false;
        ReleaseGpu();
        InvalidatePendingGpu();
        context_identity_ = nullptr;
    }

    void PerformanceMonitor::SaveGpuPass(const PerfGpuResult& result)
    {
        if (result.output_id < gpu_output_)
        {
            return;
        }

        if (result.output_id != gpu_output_)
        {
            gpu_output_ = result.output_id;
            gpu_age_ = history_->CountOutputsAfter(result.output_id);
            gpu_pass_count_ = 0;
            gpu_passes_ = {};
        }

        uint32_t index = 0;

        while (index < gpu_pass_count_ && gpu_passes_[index].scope_id != result.scope_id)
        {
            ++index;
        }

        if (index == gpu_pass_count_)
        {
            if (gpu_pass_count_ == gpu_passes_.size())
            {
                ++gpu_dropped_;
                return;
            }

            gpu_passes_[gpu_pass_count_++].scope_id = result.scope_id;
        }

        PerfGpuSample& sample = gpu_passes_[index];
        ++sample.calls;

        if (result.state != ncl_perf::PerfSampleState::Measured)
        {
            sample.state = result.state;
            sample.duration_ns = 0;
        }
        else if (sample.state != ncl_perf::PerfSampleState::Incomplete && sample.state != ncl_perf::PerfSampleState::Dropped)
        {
            sample.state = ncl_perf::PerfSampleState::Measured;
            sample.duration_ns += result.duration_ns;
        }
    }

    void PerformanceMonitor::SetTraceBackend(PerformanceTraceBackendInterface* backend)
    {
        if (trace_backend_ == backend)
        {
            return;
        }

        if (trace_backend_)
        {
            for (ScopeSlot& slot : cpu_slots_)
            {
                if (slot.trace)
                {
                    trace_backend_->EndCpu(slot.trace);
                    slot.trace = 0;
                }
            }

            for (ScopeSlot& slot : gpu_slots_)
            {
                if (slot.trace)
                {
                    trace_backend_->EndGpu(slot.trace);
                    slot.trace = 0;
                }
            }
        }

        trace_backend_ = backend;
        trace_active_ = trace_backend_ && trace_backend_->IsConnected();
    }

    PerformanceMonitor::ScopeSlot* PerformanceMonitor::ReserveSlot(std::array<ScopeSlot, ncl_perf::kPerfScopeCapacity>& slots)
    {
        for (ScopeSlot& slot : slots)
        {
            if (!slot.token)
            {
                slot.token = ++next_scope_token_;

                return &slot;
            }
        }

        return nullptr;
    }

    PerformanceMonitor::ScopeSlot* PerformanceMonitor::FindSlot(
        std::array<ScopeSlot, ncl_perf::kPerfScopeCapacity>& slots,
        ncl_perf::PerfScopeToken_t token
    )
    {
        if (token)
        {
            for (ScopeSlot& slot : slots)
            {
                if (slot.token == token)
                {
                    return &slot;
                }
            }
        }

        return nullptr;
    }

    bool PerformanceMonitor::is_inside_host() const
    {
        return host_depth_ != 0;
    }

    bool PerformanceMonitor::is_main_window(void* window) const
    {
        return window && window == main_window_;
    }

    bool PerformanceMonitor::is_context(void* context) const
    {
        return context_alive_ && context && context == context_identity_;
    }

    void PerformanceMonitor::AdvanceGpuAge(PerfOutputId_t output_id)
    {
        if (gpu_output_ && gpu_output_ != output_id && gpu_age_ < UINT32_MAX)
        {
            ++gpu_age_;
        }
    }

    void PerformanceMonitor::InvalidatePendingGpu()
    {
        gpu_output_ = 0;
        gpu_age_ = 0;
        gpu_pass_count_ = 0;
        gpu_passes_ = {};
        InvalidatePendingGpuValues();
    }

    void PerformanceMonitor::InvalidatePendingGpuValues()
    {
        if (history_)
        {
            history_->InvalidatePendingGpu();
        }

        for (std::array<PerfValue, uint32_t(PerfSeries::Count)>* values : {&last_output_.values, &output_draft_.values, &staged_gpu_})
        {
            for (PerfValue& value : *values)
            {
                if (value.state == ncl_perf::PerfSampleState::Pending)
                {
                    value = {0, ncl_perf::PerfSampleState::Incomplete};
                }
            }
        }

        if (publisher_)
        {
            publisher_->InvalidateHistory();
            AdvanceSnapshot();
        }
    }

    void PerformanceMonitor::DiscardHostFrame()
    {
        // A stock Host abort can skip C++ scope destructors inside the frame.
        host_aborted_ = true;
        accepted_ = false;
        frame_accepted_ = false;
        dump_.RetryActive();
        accounting_.Reset(++cpu_generation_);

        for (ScopeSlot& slot : cpu_slots_)
        {
            slot.collector = 0;
        }

        auto close_scopes = [&](std::array<ScopeSlot, ncl_perf::kPerfScopeCapacity>& slots, bool gpu) {
            for (;;)
            {
                const ScopeSlot* latest = &*std::max_element(slots.begin(), slots.end(), [](const ScopeSlot& left, const ScopeSlot& right) {
                    return left.token < right.token;
                });
                ncl_perf::PerfScopeToken_t token = latest->token;

                if (!token)
                {
                    break;
                }

                if (gpu)
                {
                    EndGpu(token);
                }
                else
                {
                    EndCpu(token);
                }
            }
        };

        if (gpu_ && reserved_output_)
        {
            gpu_->CancelOutput(reserved_output_);
        }

        close_scopes(gpu_slots_, true);
        close_scopes(cpu_slots_, false);

        if (trace_backend_ && trace_host_token_)
        {
            trace_backend_->EndCpu(trace_host_token_);
        }

        trace_host_token_ = 0;
        host_token_ = 0;
        swap_token_ = 0;
        render_token_ = 0;
        screen_token_ = 0;
        screen_output_ = 0;
        reserved_output_ = 0;
        staged_gpu_ = {};
        output_draft_ = {};
        counters_ = {};
        gauges_ = {};
        cpu_collection_ns_ = 0;

        if (publisher_)
        {
            publisher_->InvalidateHistory();
        }
    }
} // namespace performance
