#pragma once

#include <array>

#include <ncl_perf/performance_data.h>

namespace performance
{
    using PerfFrameId_t = uint64_t;
    using PerfOutputId_t = uint64_t;

    inline constexpr uint32_t kPerfHistoryCapacity = 16384;
    inline constexpr uint32_t kPerfGraphCapacity = 256;
    inline constexpr ncl_perf::PerfDuration_t kPerfHistoryDuration = 10000000000;

    enum class PerfSeries : uint32_t
    {
        FrameInterval,
        HostCpu,
        SwapCpu,
        RenderGpu,
        StudioCpu,
        StudioGpu,
        RadarCpu,
        RadarGpu,
        MonitorCpu,
        Count
    };

    enum class PerfRecordKind : uint32_t
    {
        Host,
        Output
    };

    enum class PerfCounterId : uint32_t
    {
        CpuDroppedScopes,
        GpuDroppedScopes,
        GpuExpiredResults,
        StudioEngineSubmissions,
        StudioShaderSubmissions,
        StudioDrawCalls,
        StudioConstituentDraws,
        StudioTriangles,
        ShaderCompiles,
        ShaderLinks,
        RadarBatches,
        RadarQuads,
        RadarVisibilityTraces,
        RadarEntitySweeps,
        RadarLayerCacheHits,
        RadarLayerImages,
        ShaderBuildFailures,
        StudioPackets,
        StudioSharedRuns,
        StudioSharedPackets,
        StudioSinglePackets,
        StudioProgramBinds,
        StudioMultiDrawBatches,
        StudioBatchedMeshes,
        StudioSingleMeshes,
        StudioFlushBrush,
        StudioFlushFallback,
        StudioFlushClient,
        StudioFlushStageEnd,
        StudioFlushCapacity,
        StudioFlushResource,
        StudioFlushImmediate,
        StudioFlushMutableSkin,
        StudioImmediateOutsideOpaque,
        StudioImmediateQueueDisabled,
        StudioImmediateCallback,
        StudioImmediateMutableSkin,
        Count
    };

    enum class PerfGaugeId : uint32_t
    {
        StudioQueuePeakPackets,
        Count
    };

    struct PerfScalarValue
    {
        double value{};
        ncl_perf::PerfSampleState state = ncl_perf::PerfSampleState::Inactive;
    };

    struct PerfScalarSummary
    {
        uint32_t count{};
        double last{};
        double mean{};
        double maximum{};
        ncl_perf::PerfSampleState state = ncl_perf::PerfSampleState::Inactive;
    };

    struct PerfValue
    {
        ncl_perf::PerfDuration_t duration_ns{};
        ncl_perf::PerfSampleState state = ncl_perf::PerfSampleState::Unavailable;
    };

    struct PerfFrameRecord
    {
        PerfFrameId_t frame_id{};
        PerfOutputId_t output_id{};
        ncl_perf::PerfTimestamp_t completed_at{};
        uint32_t collector_generation{};
        uint32_t context_generation{};
        uint32_t map_generation{};
        PerfRecordKind kind = PerfRecordKind::Output;
        std::array<PerfValue, uint32_t(PerfSeries::Count)> values{};
        std::array<uint64_t, uint32_t(PerfCounterId::Count)> counters{};
        std::array<PerfScalarValue, uint32_t(PerfGaugeId::Count)> gauges{};
    };

    struct PerfGpuResult
    {
        PerfFrameId_t frame_id{};
        PerfOutputId_t output_id{};
        ncl_perf::PerfScopeId_t scope_id{};
        uint32_t collector_generation{};
        uint32_t context_generation{};
        ncl_perf::PerfDuration_t duration_ns{};
        PerfSeries series = PerfSeries::RenderGpu;
        ncl_perf::PerfSampleState state = ncl_perf::PerfSampleState::Measured;
    };

    struct PerfSummary
    {
        uint32_t count{};
        ncl_perf::PerfDuration_t mean_ns{};
        ncl_perf::PerfDuration_t median_ns{};
        ncl_perf::PerfDuration_t maximum_ns{};
        ncl_perf::PerfDuration_t p95_ns{};
        ncl_perf::PerfDuration_t p99_ns{};
        double fps{};
        bool percentiles_available{};
        ncl_perf::PerfSampleState state = ncl_perf::PerfSampleState::Unavailable;
    };

    struct PerfGraphBin
    {
        ncl_perf::PerfDuration_t maximum_ns{};
        ncl_perf::PerfDuration_t mean_ns{};
        uint32_t count{};
    };

    enum class PerfWorkerStatus : uint32_t
    {
        Inactive,
        Pending,
        Completed,
        Canceled,
        Failed
    };

    struct PerfWorkerReport
    {
        uint32_t map_generation{};
        ncl_perf::PerfDuration_t duration_ns{};
        PerfWorkerStatus status = PerfWorkerStatus::Inactive;
    };

    struct PerfGpuSample
    {
        ncl_perf::PerfScopeId_t scope_id{};
        ncl_perf::PerfDuration_t duration_ns{};
        uint32_t calls{};
        ncl_perf::PerfSampleState state = ncl_perf::PerfSampleState::Pending;
    };

    struct PerfSnapshot
    {
        ncl_perf::PerfTimestamp_t captured_at{};
        ncl_perf::PerfDuration_t coverage_ns{};
        uint32_t record_count{};
        uint32_t collector_generation{};
        uint32_t context_generation{};
        uint32_t map_generation{};
        uint32_t cpu_count{};
        uint32_t gpu_age{};
        uint32_t gpu_count{};
        PerfOutputId_t gpu_output_id{};
        ncl_perf::PerfDuration_t shader_compile_ns{};
        ncl_perf::PerfDuration_t shader_link_ns{};
        int32_t mode{};
        bool capacity_limited{};
        bool gpu_requested{};
        bool gpu_supported{};
        bool presenter_available{};
        bool trace_active{};
        PerfFrameRecord latest;
        PerfWorkerReport worker;
        std::array<PerfSummary, uint32_t(PerfSeries::Count)> summaries{};
        std::array<std::array<PerfGraphBin, kPerfGraphCapacity>, uint32_t(PerfSeries::Count)> graphs{};
        std::array<ncl_perf::PerfCpuSample, ncl_perf::kPerfScopeCapacity> cpu_tree{};
        std::array<PerfGpuSample, ncl_perf::kPerfScopeCapacity> gpu_passes{};
        std::array<uint64_t, uint32_t(PerfCounterId::Count)> counters{};
        std::array<PerfScalarValue, uint32_t(PerfGaugeId::Count)> gauges{};
        std::array<PerfScalarSummary, uint32_t(PerfGaugeId::Count)> gauge_summaries{};
        char coverage_reason[128]{};
        char gl_vendor[128]{};
        char gl_renderer[128]{};
        char gl_version[64]{};
        int32_t timer_bits{};
        float fps_cap{};
        int32_t vsync{};
        bool r_speeds{};
        bool studio_shader{};
        int32_t engine_build{};
        float panel_scale = 1.0f;
        PerfFrameId_t cpu_frame_id{};
        ncl_perf::PerfSampleState shader_compile_state = ncl_perf::PerfSampleState::Unavailable;
        ncl_perf::PerfSampleState shader_link_state = ncl_perf::PerfSampleState::Unavailable;
        uint32_t cpu_frame_count{};
        ncl_perf::PerfDuration_t cpu_window_ns{};
        ncl_perf::PerfDuration_t cpu_collection_ns{};
        ncl_perf::PerfDuration_t gpu_poll_ns{};
    };
} // namespace performance
