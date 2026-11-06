#pragma once

#include <array>
#include <memory>

#include <ncl_perf/PerfCpuAccounting.h>
#include <next_engine_mini/PerformanceMonitorInterface.h>
#include <nitroapi/FrameLifecycleInterface.h>

#include "performance/PerformanceBatchWindow.h"
#include "performance/PerformanceDumpCapture.h"
#include "performance/PerformanceFrameHistory.h"
#include "performance/PerformanceSnapshotPublisher.h"

namespace performance
{
    class GpuQueryBackendInterface;
    class GpuQueryCollector;
    class PerformanceTraceBackendInterface;

    class PerformanceMonitor final : public PerformanceMonitorInterface, public nitroapi::FrameLifecycleObserverInterface
    {
    private:
        struct ScopeSlot
        {
            ncl_perf::PerfScopeToken_t token{};
            ncl_perf::PerfScopeToken_t collector{};
            ncl_perf::PerfScopeToken_t trace{};
        };

        std::array<ScopeSlot, ncl_perf::kPerfScopeCapacity> cpu_slots_{};
        std::array<ScopeSlot, ncl_perf::kPerfScopeCapacity> gpu_slots_{};
        PerformanceTraceBackendInterface* trace_backend_{};
        ncl_perf::PerfScopeToken_t next_scope_token_{};
        ncl_perf::PerfScopeToken_t swap_token_{};
        ncl_perf::PerfScopeToken_t trace_host_token_{};
        PerfOutputId_t trace_output_{};
        bool frame_accepted_{};
        bool host_aborted_{};
        void* context_identity_{};
        PerfFrameId_t completed_frame_id_{};
        ncl_perf::PerfSampleState shader_compile_state_ = ncl_perf::PerfSampleState::Unavailable;
        ncl_perf::PerfSampleState shader_link_state_ = ncl_perf::PerfSampleState::Unavailable;
        ncl_perf::PerfClockInterface& clock_;
        ncl_perf::PerfCpuAccounting accounting_;
        std::unique_ptr<PerfFrameHistory> history_;
        GpuQueryBackendInterface* gpu_backend_{};
        int gpu_timer_bits_{};
        bool gpu_timestamp_supported_{};
        bool trace_active_{};
        std::unique_ptr<GpuQueryCollector> gpu_;
        std::array<PerfValue, uint32_t(PerfSeries::Count)> staged_gpu_{};
        std::array<PerfGpuSample, ncl_perf::kPerfScopeCapacity> gpu_passes_{};
        std::unique_ptr<performance::PerfSnapshotPublisher> publisher_;
        performance::PerfPanelProjection panel_projection_;
        std::array<ncl_perf::PerfCpuSample, ncl_perf::kPerfScopeCapacity> completed_tree_{};
        ncl_perf::PerfDuration_t cpu_collection_ns_{};
        ncl_perf::PerfDuration_t completed_cpu_collection_ns_{};
        ncl_perf::PerfDuration_t gpu_poll_ns_{};
        ncl_perf::PerfDuration_t output_poll_start_{};
        ncl_perf::PerfDuration_t completed_gpu_poll_ns_{};
        std::array<uint64_t, uint32_t(PerfCounterId::Count)> counters_{};
        std::array<PerfScalarValue, uint32_t(PerfGaugeId::Count)> gauges_{};
        std::array<PerfScalarValue, uint32_t(PerfGaugeId::Count)> completed_gauges_{};
        std::array<bool, ncl_perf::kPerfScopeCapacity> stage_available_{};
        PerfFrameRecord output_draft_;
        PerfFrameRecord last_output_;
        PerfWorkerReport worker_;
        char gl_vendor_[128]{};
        char gl_renderer_[128]{};
        char gl_version_[64]{};
        float fps_cap_{};
        float panel_scale_ = 1.0f;
        int vsync_{};
        int engine_build_{};
        bool r_speeds_{};
        bool studio_shader_{};
        void* main_window_{};
        ncl_perf::PerfScopeToken_t host_token_{};
        ncl_perf::PerfScopeToken_t render_token_{};
        ncl_perf::PerfScopeToken_t screen_token_{};
        PerfOutputId_t reserved_output_{};
        PerfOutputId_t screen_output_{};
        PerfOutputId_t gpu_output_{};
        uint32_t gpu_age_{};
        uint64_t gpu_expired_{};
        uint64_t gpu_dropped_{};
        ncl_perf::PerfDuration_t shader_compile_ns_{};
        ncl_perf::PerfDuration_t shader_link_ns_{};
        ncl_perf::PerfTimestamp_t swap_start_{};
        ncl_perf::PerfTimestamp_t last_swap_{};
        PerfFrameId_t frame_id_{};
        PerfOutputId_t output_id_{};
        uint32_t generation_ = 1;
        uint32_t cpu_generation_ = 1;
        uint32_t context_generation_{};
        uint32_t map_generation_ = 1;
        uint32_t host_depth_{};
        uint32_t completed_tree_count_{};
        uint32_t gpu_pass_count_{};
        int mode_{};
        int requested_mode_{};
        bool cpu_enabled_{};
        PerfBatchWindow batch_;
        PerfDumpCapture dump_;
        bool accepted_{};
        bool has_swap_baseline_{};
        bool gpu_requested_ = true;
        bool context_alive_{};
        bool shutdown_{};

    public:
        explicit PerformanceMonitor(ncl_perf::PerfClockInterface& clock);
        ~PerformanceMonitor() override;
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
        void AddCounter(PerfCounterId counter, uint64_t value) override;
        bool SetGauge(PerfGaugeId gauge, double value) override;
        bool GetSnapshot(PerfSnapshot& output) const override;
        bool GetPanelSnapshot(
            PerfSnapshot& output,
            PerfGroupedGraphs* groups = nullptr,
            PerfFrameGraphs* frames = nullptr,
            uint32_t frame_columns = kPerfGraphCapacity
        ) const;
        uint32_t map_generation() const override;
        void ReportWorker(const PerfWorkerReport& report) override;
        int get_mode() const;
        int get_batch_level() const;
        bool RequestDumpCapture();
        bool TakeDumpCapture(PerfSnapshot& output);
        bool is_dump_ready() const;
        const PerfSnapshot* get_dump_snapshot() const;
        const PerfSnapshot* get_batch_snapshot() const;
        PerformanceMonitor* get_scope_monitor();
        PerformanceMonitor* get_counter_monitor();
        PerformanceMonitor* get_studio_detail_monitor();
        void SetMode(int mode);
        void SetBatchLevel(int level);
        void SetPanelScale(float scale);
        void SetGpuEnabled(bool enabled);
        void Reset();
        void OnMapChanged();
        void OnContextCreated(void* context = nullptr);
        void OnContextDestroyed();
        void Shutdown();
        void OnHostEntry() override;
        void OnFrameAccepted() override;
        void OnHostExit(bool completed = true) override;
        void SetMainWindow(void* window);
        void OnWindowCreated(void* window, bool opengl);
        void OnWindowDestroyed(void* window);
        PerfOutputId_t BeginOutput(void* window);
        void MarkSwapStart();
        void EndOutput(PerfOutputId_t output_id);
        void SetTraceBackend(PerformanceTraceBackendInterface* backend);
        bool is_inside_host() const;
        bool is_main_window(void* window) const;
        bool is_context(void* context) const;
        void SetStageAvailable(ncl_perf::PerfScopeId_t scope_id, bool available);
        void SetGpuBackend(GpuQueryBackendInterface* backend);
        void CloseRenderGpu();
        void PollGpu();
        void AdvanceSnapshot();
        void AbandonGpu();
        void SetGraphicsInfo(const char* vendor, const char* renderer, const char* version);
        void SetEngineInfo(float fps_cap, int vsync, bool r_speeds, bool studio_shader, int engine_build);

    private:
        ScopeSlot* ReserveSlot(std::array<ScopeSlot, ncl_perf::kPerfScopeCapacity>& slots);
        ScopeSlot* FindSlot(std::array<ScopeSlot, ncl_perf::kPerfScopeCapacity>& slots, ncl_perf::PerfScopeToken_t token);
        ncl_perf::PerfScopeToken_t BeginCollectorGpu(ncl_perf::PerfScopeId_t scope_id);
        void DiscardHostFrame();
        void CancelDumpCapture();
        void CompleteDumpCapture(ncl_perf::PerfTimestamp_t completed_at);
        void EnsureHistory();
        void AdvanceSnapshot(ncl_perf::PerfTimestamp_t now);
        void CopySnapshotMetadata(PerfSnapshot& output) const;
        void RefreshGpuCapabilities();
        void ApplyMode();
        void EnsureGpu();
        void ReleaseGpu();
        void SaveGpuPass(const PerfGpuResult& result);
        void AdvanceGpuAge(PerfOutputId_t output_id);
        void InvalidatePendingGpuValues();
        void InvalidatePendingGpu();
    };

    ncl_perf::PerfClockInterface& get_clock();
    PerformanceMonitor& GetMonitor();

} // namespace performance
