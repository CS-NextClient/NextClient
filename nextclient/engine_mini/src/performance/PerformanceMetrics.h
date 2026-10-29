#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <cmath>

#include <next_engine_mini/PerformanceMonitorInterface.h>

namespace performance
{
    enum class CollectionLevel
    {
        Ordinary,
        StudioDetail
    };

    enum class MetricUnit
    {
        Nanoseconds,
        Count,
        Packets
    };

    enum class SeriesSource
    {
        CpuInclusive,
        PresentedInterval,
        GpuRollup,
        MonitorOutputCost
    };

    enum class SeriesAggregation
    {
        DurationDistribution,
        PositiveDurationDistribution
    };

    enum class AvailabilityPolicy
    {
        PresentedBaseline,
        RegisteredStage,
        InactiveWhenUnused,
        DelayedGpu,
        DelayedGpuInactiveWhenUnused,
        CompletedOutputCost
    };

    enum class SeriesPlacement
    {
        Hidden,
        Cpu,
        Gpu,
        Footer
    };

    enum class GroupRole
    {
        None,
        Interval,
        Total,
        Part
    };

    enum class CounterSource
    {
        AcceptedFrameDelta,
        CpuDroppedScopes,
        GpuDroppedScopes,
        GpuExpiredResults
    };

    enum class CounterReduction
    {
        SumAcceptedFrames,
        LastObservation
    };

    enum class GaugeAggregation
    {
        LastObservation
    };

    enum class GaugePlacement
    {
        Studio
    };

    inline constexpr uint32_t kGroupPartCapacity = 3;
    inline constexpr uint32_t kStudioCostCapacity = 5;

    struct StageDescriptor
    {
        ncl_perf::PerfScopeDesc scope;
        CollectionLevel cpu_level = CollectionLevel::Ordinary;
        PerfSeries gpu_series = PerfSeries::Count;
        bool studio_cost{};
        bool studio_detail{};
        bool studio_other_cost{};
        const char* studio_label{};
        uint32_t studio_order{};
    };

    struct SeriesPresentation
    {
        const char* label;
        SeriesPlacement placement;
        uint32_t order;
        uint32_t color;
        uint32_t indentation;
        bool overview;
        bool fallback;
        GroupRole role;
        uint32_t part_order{};
    };

    struct SeriesDescriptor
    {
        PerfSeries id;
        const char* name;
        PerfRecordKind domain;
        SeriesSource source;
        ncl_perf::PerfScopeId_t scope_id;
        MetricUnit unit;
        SeriesAggregation aggregation;
        AvailabilityPolicy availability;
        SeriesPresentation presentation;
    };

    struct CounterDescriptor
    {
        PerfCounterId id;
        const char* name;
        MetricUnit unit;
        CounterSource source;
        CounterReduction reduction;
    };

    struct GaugeDescriptor
    {
        PerfGaugeId id;
        const char* name;
        PerfRecordKind domain;
        MetricUnit unit;
        GaugeAggregation aggregation;
        GaugePlacement placement;
        const char* label;
    };

    inline constexpr std::array kGauges = {
        GaugeDescriptor{
            PerfGaugeId::StudioQueuePeakPackets,
            "Studio queue peak packets (last completed opaque traversal)",
            PerfRecordKind::Host,
            MetricUnit::Packets,
            GaugeAggregation::LastObservation,
            GaugePlacement::Studio,
            "Queue peak (last traversal)"
        }
    };

    inline constexpr std::array kStages = {
        StageDescriptor{{PerfStage::Host, "Host", 0, ncl_perf::PerfScopeCapability::Cpu}},
        StageDescriptor{{PerfStage::Commands, "Commands (boundary unresolved)", PerfStage::Host, ncl_perf::PerfScopeCapability(0)}},
        StageDescriptor{{PerfStage::Client, "Client", PerfStage::Host, ncl_perf::PerfScopeCapability::Cpu}},
        StageDescriptor{{PerfStage::Packets, "Packets", PerfStage::Client, ncl_perf::PerfScopeCapability::Cpu}},
        StageDescriptor{{PerfStage::Prediction, "Prediction (boundary unresolved)", PerfStage::Client, ncl_perf::PerfScopeCapability(0)}},
        StageDescriptor{{PerfStage::Entities, "Entities", PerfStage::Client, ncl_perf::PerfScopeCapability::Cpu}},
        StageDescriptor{{PerfStage::TempEntities, "Temporary entities", PerfStage::Client, ncl_perf::PerfScopeCapability::Cpu}},
        StageDescriptor{
            {PerfStage::Screen, "Screen", PerfStage::Host, ncl_perf::PerfScopeCapability::CpuGpu},
            CollectionLevel::Ordinary,
            PerfSeries::RenderGpu,
            false,
            false,
            false
        },
        StageDescriptor{{PerfStage::View, "View", PerfStage::Screen, ncl_perf::PerfScopeCapability::Cpu}},
        StageDescriptor{{PerfStage::World, "World", PerfStage::View, ncl_perf::PerfScopeCapability::Cpu}},
        StageDescriptor{{PerfStage::ViewModel, "Viewmodel", PerfStage::View, ncl_perf::PerfScopeCapability::Cpu}},
        StageDescriptor{{PerfStage::Hud, "HUD", PerfStage::Screen, ncl_perf::PerfScopeCapability::Cpu}},
        StageDescriptor{{PerfStage::Studio, "Studio", PerfStage::View, ncl_perf::PerfScopeCapability::Cpu}},
        StageDescriptor{{PerfStage::StudioGeometry, "Geometry preparation", PerfStage::Studio, ncl_perf::PerfScopeCapability::Cpu}},
        StageDescriptor{{PerfStage::StudioMaterial, "Material preparation", PerfStage::Studio, ncl_perf::PerfScopeCapability::Cpu}},
        StageDescriptor{
            {PerfStage::StudioShader, "Shader submission", PerfStage::Studio, ncl_perf::PerfScopeCapability::CpuGpu},
            CollectionLevel::Ordinary,
            PerfSeries::StudioGpu,
            false,
            false,
            false
        },
        StageDescriptor{{PerfStage::StudioStock, "Stock submission", PerfStage::Studio, ncl_perf::PerfScopeCapability::Cpu}},
        StageDescriptor{
            {PerfStage::StudioGlSave, "GL state save", PerfStage::Studio, ncl_perf::PerfScopeCapability::Cpu},
            CollectionLevel::StudioDetail,
            PerfSeries::Count,
            true,
            false,
            false,
            "GL save",
            3
        },
        StageDescriptor{
            {PerfStage::StudioGlRestore, "GL state restore", PerfStage::Studio, ncl_perf::PerfScopeCapability::Cpu},
            CollectionLevel::StudioDetail,
            PerfSeries::Count,
            true,
            false,
            false,
            "GL restore",
            2
        },
        StageDescriptor{
            {PerfStage::StudioProgramBind, "Program bind", PerfStage::Studio, ncl_perf::PerfScopeCapability::Cpu},
            CollectionLevel::StudioDetail,
            PerfSeries::Count,
            true,
            false,
            false,
            "Program bind",
            4
        },
        StageDescriptor{
            {PerfStage::StudioBufferBind, "Buffer binds", PerfStage::Studio, ncl_perf::PerfScopeCapability::Cpu},
            CollectionLevel::StudioDetail,
            PerfSeries::Count,
            true,
            false,
            false,
            "Buffer binds",
            1
        },
        StageDescriptor{
            {PerfStage::StudioBoneUpload, "Bone upload", PerfStage::Studio, ncl_perf::PerfScopeCapability::Cpu},
            CollectionLevel::StudioDetail,
            PerfSeries::Count,
            true,
            false,
            false,
            "Bone upload",
            0
        },
        StageDescriptor{
            {PerfStage::StudioModelStateCapture, "Model state capture", PerfStage::StudioGlSave, ncl_perf::PerfScopeCapability::Cpu},
            CollectionLevel::StudioDetail,
            PerfSeries::Count,
            false,
            true,
            false
        },
        StageDescriptor{
            {PerfStage::StudioBatchStateSave, "Batch state save", PerfStage::StudioGlSave, ncl_perf::PerfScopeCapability::Cpu},
            CollectionLevel::StudioDetail,
            PerfSeries::Count,
            false,
            true,
            false
        },
        StageDescriptor{
            {PerfStage::StudioRestoreBindings, "Restore bindings", PerfStage::StudioGlRestore, ncl_perf::PerfScopeCapability::Cpu},
            CollectionLevel::StudioDetail,
            PerfSeries::Count,
            false,
            true,
            false
        },
        StageDescriptor{
            {PerfStage::StudioRestoreMatrices,
             "Restore matrices/attributes",
             PerfStage::StudioGlRestore,
             ncl_perf::PerfScopeCapability::Cpu},
            CollectionLevel::StudioDetail,
            PerfSeries::Count,
            false,
            true,
            false
        },
        StageDescriptor{
            {PerfStage::StudioRestoreTextures, "Restore textures", PerfStage::StudioGlRestore, ncl_perf::PerfScopeCapability::Cpu},
            CollectionLevel::StudioDetail,
            PerfSeries::Count,
            false,
            true,
            false
        },
        StageDescriptor{
            {PerfStage::StudioDataPrepare, "Model data preparation", PerfStage::Studio, ncl_perf::PerfScopeCapability::Cpu},
            CollectionLevel::StudioDetail,
            PerfSeries::Count,
            false,
            true,
            true
        },
        StageDescriptor{
            {PerfStage::StudioMeshSort, "Mesh sorting", PerfStage::Studio, ncl_perf::PerfScopeCapability::Cpu},
            CollectionLevel::StudioDetail,
            PerfSeries::Count,
            false,
            true,
            true
        },
        StageDescriptor{
            {PerfStage::StudioStateApply, "GL state apply", PerfStage::Studio, ncl_perf::PerfScopeCapability::Cpu},
            CollectionLevel::StudioDetail,
            PerfSeries::Count,
            false,
            true,
            true
        },
        StageDescriptor{
            {PerfStage::StudioUniformSetup, "Uniform setup", PerfStage::Studio, ncl_perf::PerfScopeCapability::Cpu},
            CollectionLevel::StudioDetail,
            PerfSeries::Count,
            false,
            true,
            true
        },
        StageDescriptor{
            {PerfStage::StudioTextureSetup, "Texture setup", PerfStage::Studio, ncl_perf::PerfScopeCapability::Cpu},
            CollectionLevel::StudioDetail,
            PerfSeries::Count,
            false,
            true,
            true
        },
        StageDescriptor{{PerfStage::ShaderCompile, "Shader compile", 0, ncl_perf::PerfScopeCapability::Cpu}},
        StageDescriptor{{PerfStage::ShaderLink, "Shader link", 0, ncl_perf::PerfScopeCapability::Cpu}},
        StageDescriptor{{PerfStage::Radar, "Radar total", PerfStage::Hud, ncl_perf::PerfScopeCapability::Cpu}},
        StageDescriptor{{PerfStage::RadarPrepare, "Radar preparation", PerfStage::Radar, ncl_perf::PerfScopeCapability::Cpu}},
        StageDescriptor{{PerfStage::RadarDraw, "Radar draw", PerfStage::Radar, ncl_perf::PerfScopeCapability::Cpu}},
        StageDescriptor{{PerfStage::RadarLayout, "Map layout", PerfStage::Radar, ncl_perf::PerfScopeCapability::Cpu}},
        StageDescriptor{{PerfStage::RadarCapture, "Map capture", PerfStage::RadarLayout, ncl_perf::PerfScopeCapability::Cpu}},
        StageDescriptor{
            {PerfStage::RadarLayer, "Layer image", PerfStage::RadarLayout, ncl_perf::PerfScopeCapability::CpuGpu},
            CollectionLevel::Ordinary,
            PerfSeries::RadarGpu,
            false,
            false,
            false
        },
        StageDescriptor{{PerfStage::RadarPolicy, "Radar policy", PerfStage::RadarDraw, ncl_perf::PerfScopeCapability::Cpu}},
        StageDescriptor{{PerfStage::RadarVisibility, "Visibility traces", PerfStage::RadarPolicy, ncl_perf::PerfScopeCapability::Cpu}},
        StageDescriptor{
            {PerfStage::RadarBackdrop, "Backdrop blur", PerfStage::RadarDraw, ncl_perf::PerfScopeCapability::CpuGpu},
            CollectionLevel::Ordinary,
            PerfSeries::RadarGpu,
            false,
            false,
            false
        },
        StageDescriptor{{PerfStage::RadarObjectives, "Objectives", PerfStage::RadarDraw, ncl_perf::PerfScopeCapability::Cpu}},
        StageDescriptor{{PerfStage::RadarPlayers, "Players", PerfStage::RadarDraw, ncl_perf::PerfScopeCapability::Cpu}},
        StageDescriptor{{PerfStage::RadarText, "Text", PerfStage::RadarDraw, ncl_perf::PerfScopeCapability::Cpu}},
        StageDescriptor{
            {PerfStage::RadarBatch, "Batch submission", PerfStage::RadarDraw, ncl_perf::PerfScopeCapability::CpuGpu},
            CollectionLevel::Ordinary,
            PerfSeries::RadarGpu,
            false,
            false,
            false
        },
        StageDescriptor{
            {PerfStage::RadarBackdropCapture, "Backdrop capture", PerfStage::RadarBackdrop, ncl_perf::PerfScopeCapability::Gpu},
            CollectionLevel::Ordinary,
            PerfSeries::RadarGpu,
            false,
            false,
            false
        },
        StageDescriptor{
            {PerfStage::RadarMapDraw, "Map draw", PerfStage::RadarDraw, ncl_perf::PerfScopeCapability::Gpu},
            CollectionLevel::Ordinary,
            PerfSeries::RadarGpu,
            false,
            false,
            false
        },
        StageDescriptor{{PerfStage::Sound, "Sound", PerfStage::Host, ncl_perf::PerfScopeCapability::Cpu}},
        StageDescriptor{{PerfStage::SoundExtra, "Sound extra", PerfStage::Host, ncl_perf::PerfScopeCapability::Cpu}},
        StageDescriptor{{PerfStage::Swap, "Swap", PerfStage::Screen, ncl_perf::PerfScopeCapability::Cpu}},
        StageDescriptor{{PerfStage::Monitor, "Monitor", PerfStage::Screen, ncl_perf::PerfScopeCapability::CpuGpu}}
    };

    inline constexpr std::array kSeries = {
        SeriesDescriptor{
            PerfSeries::FrameInterval,
            "Frame interval",
            PerfRecordKind::Output,
            SeriesSource::PresentedInterval,
            0,
            MetricUnit::Nanoseconds,
            SeriesAggregation::PositiveDurationDistribution,
            AvailabilityPolicy::PresentedBaseline,
            {"Swap Interval", SeriesPlacement::Cpu, 0, 0x7795ccff, 0, true, true, GroupRole::Interval, 0}
        },
        SeriesDescriptor{
            PerfSeries::HostCpu,
            "Host CPU",
            PerfRecordKind::Host,
            SeriesSource::CpuInclusive,
            PerfStage::Host,
            MetricUnit::Nanoseconds,
            SeriesAggregation::DurationDistribution,
            AvailabilityPolicy::RegisteredStage,
            {"Host Frame", SeriesPlacement::Cpu, 1, 0xe6ab63ff, 1, true, true, GroupRole::Total, 0}
        },
        SeriesDescriptor{
            PerfSeries::SwapCpu,
            "Swap CPU",
            PerfRecordKind::Host,
            SeriesSource::CpuInclusive,
            PerfStage::Swap,
            MetricUnit::Nanoseconds,
            SeriesAggregation::DurationDistribution,
            AvailabilityPolicy::RegisteredStage,
            {"Swap", SeriesPlacement::Cpu, 4, 0xc785ffff, 2, true, false, GroupRole::Part, 2}
        },
        SeriesDescriptor{
            PerfSeries::RenderGpu,
            "Render GPU",
            PerfRecordKind::Output,
            SeriesSource::GpuRollup,
            0,
            MetricUnit::Nanoseconds,
            SeriesAggregation::DurationDistribution,
            AvailabilityPolicy::DelayedGpu,
            {"Render", SeriesPlacement::Gpu, 0, 0xff7070ff, 0, true, true, GroupRole::Total, 0}
        },
        SeriesDescriptor{
            PerfSeries::StudioCpu,
            "Studio CPU",
            PerfRecordKind::Host,
            SeriesSource::CpuInclusive,
            PerfStage::Studio,
            MetricUnit::Nanoseconds,
            SeriesAggregation::DurationDistribution,
            AvailabilityPolicy::InactiveWhenUnused,
            {"Studio", SeriesPlacement::Cpu, 2, 0x90cf70ff, 2, true, false, GroupRole::Part, 0}
        },
        SeriesDescriptor{
            PerfSeries::StudioGpu,
            "Studio GPU",
            PerfRecordKind::Output,
            SeriesSource::GpuRollup,
            0,
            MetricUnit::Nanoseconds,
            SeriesAggregation::DurationDistribution,
            AvailabilityPolicy::DelayedGpuInactiveWhenUnused,
            {"Studio", SeriesPlacement::Gpu, 1, 0x53dc9aff, 1, true, false, GroupRole::Part, 0}
        },
        SeriesDescriptor{
            PerfSeries::RadarCpu,
            "Radar CPU",
            PerfRecordKind::Host,
            SeriesSource::CpuInclusive,
            PerfStage::Radar,
            MetricUnit::Nanoseconds,
            SeriesAggregation::DurationDistribution,
            AvailabilityPolicy::InactiveWhenUnused,
            {"Radar", SeriesPlacement::Cpu, 3, 0x71bdffff, 2, true, false, GroupRole::Part, 1}
        },
        SeriesDescriptor{
            PerfSeries::RadarGpu,
            "Radar GPU",
            PerfRecordKind::Output,
            SeriesSource::GpuRollup,
            0,
            MetricUnit::Nanoseconds,
            SeriesAggregation::DurationDistribution,
            AvailabilityPolicy::DelayedGpuInactiveWhenUnused,
            {"Radar", SeriesPlacement::Gpu, 2, 0x52e5ffff, 1, true, false, GroupRole::Part, 1}
        },
        SeriesDescriptor{
            PerfSeries::MonitorCpu,
            "Monitor CPU",
            PerfRecordKind::Output,
            SeriesSource::MonitorOutputCost,
            PerfStage::Monitor,
            MetricUnit::Nanoseconds,
            SeriesAggregation::DurationDistribution,
            AvailabilityPolicy::CompletedOutputCost,
            {"Monitor", SeriesPlacement::Footer, 0, 0xb8c4d8ff, 0, false, false, GroupRole::None, 0}
        }
    };

    inline constexpr std::array kCounters = {
        CounterDescriptor{
            PerfCounterId::CpuDroppedScopes,
            "CPU dropped scopes",
            MetricUnit::Count,
            CounterSource::CpuDroppedScopes,
            CounterReduction::SumAcceptedFrames
        },
        CounterDescriptor{
            PerfCounterId::GpuDroppedScopes,
            "GPU dropped scopes",
            MetricUnit::Count,
            CounterSource::GpuDroppedScopes,
            CounterReduction::LastObservation
        },
        CounterDescriptor{
            PerfCounterId::GpuExpiredResults,
            "GPU expired results",
            MetricUnit::Count,
            CounterSource::GpuExpiredResults,
            CounterReduction::LastObservation
        },
        CounterDescriptor{
            PerfCounterId::StudioEngineSubmissions,
            "Studio engine submissions",
            MetricUnit::Count,
            CounterSource::AcceptedFrameDelta,
            CounterReduction::SumAcceptedFrames
        },
        CounterDescriptor{
            PerfCounterId::StudioShaderSubmissions,
            "Studio shader submissions",
            MetricUnit::Count,
            CounterSource::AcceptedFrameDelta,
            CounterReduction::SumAcceptedFrames
        },
        CounterDescriptor{
            PerfCounterId::StudioDrawCalls,
            "Studio draw calls",
            MetricUnit::Count,
            CounterSource::AcceptedFrameDelta,
            CounterReduction::SumAcceptedFrames
        },
        CounterDescriptor{
            PerfCounterId::StudioConstituentDraws,
            "Studio constituent draws",
            MetricUnit::Count,
            CounterSource::AcceptedFrameDelta,
            CounterReduction::SumAcceptedFrames
        },
        CounterDescriptor{
            PerfCounterId::StudioTriangles,
            "Studio triangles",
            MetricUnit::Count,
            CounterSource::AcceptedFrameDelta,
            CounterReduction::SumAcceptedFrames
        },
        CounterDescriptor{
            PerfCounterId::ShaderCompiles,
            "Shader compiles",
            MetricUnit::Count,
            CounterSource::AcceptedFrameDelta,
            CounterReduction::SumAcceptedFrames
        },
        CounterDescriptor{
            PerfCounterId::ShaderLinks,
            "Shader links",
            MetricUnit::Count,
            CounterSource::AcceptedFrameDelta,
            CounterReduction::SumAcceptedFrames
        },
        CounterDescriptor{
            PerfCounterId::RadarBatches,
            "Radar batches",
            MetricUnit::Count,
            CounterSource::AcceptedFrameDelta,
            CounterReduction::SumAcceptedFrames
        },
        CounterDescriptor{
            PerfCounterId::RadarQuads,
            "Radar quads",
            MetricUnit::Count,
            CounterSource::AcceptedFrameDelta,
            CounterReduction::SumAcceptedFrames
        },
        CounterDescriptor{
            PerfCounterId::RadarVisibilityTraces,
            "Radar visibility traces",
            MetricUnit::Count,
            CounterSource::AcceptedFrameDelta,
            CounterReduction::SumAcceptedFrames
        },
        CounterDescriptor{
            PerfCounterId::RadarEntitySweeps,
            "Radar entity sweeps",
            MetricUnit::Count,
            CounterSource::AcceptedFrameDelta,
            CounterReduction::SumAcceptedFrames
        },
        CounterDescriptor{
            PerfCounterId::RadarLayerCacheHits,
            "Radar layer cache hits",
            MetricUnit::Count,
            CounterSource::AcceptedFrameDelta,
            CounterReduction::SumAcceptedFrames
        },
        CounterDescriptor{
            PerfCounterId::RadarLayerImages,
            "Radar layer images",
            MetricUnit::Count,
            CounterSource::AcceptedFrameDelta,
            CounterReduction::SumAcceptedFrames
        },
        CounterDescriptor{
            PerfCounterId::ShaderBuildFailures,
            "Shader build failures",
            MetricUnit::Count,
            CounterSource::AcceptedFrameDelta,
            CounterReduction::SumAcceptedFrames
        },
        CounterDescriptor{
            PerfCounterId::StudioPackets,
            "Studio packets",
            MetricUnit::Count,
            CounterSource::AcceptedFrameDelta,
            CounterReduction::SumAcceptedFrames
        },
        CounterDescriptor{
            PerfCounterId::StudioSharedRuns,
            "Studio shared GL runs",
            MetricUnit::Count,
            CounterSource::AcceptedFrameDelta,
            CounterReduction::SumAcceptedFrames
        },
        CounterDescriptor{
            PerfCounterId::StudioSharedPackets,
            "Studio shared GL packets",
            MetricUnit::Count,
            CounterSource::AcceptedFrameDelta,
            CounterReduction::SumAcceptedFrames
        },
        CounterDescriptor{
            PerfCounterId::StudioSinglePackets,
            "Studio single GL packets",
            MetricUnit::Count,
            CounterSource::AcceptedFrameDelta,
            CounterReduction::SumAcceptedFrames
        },
        CounterDescriptor{
            PerfCounterId::StudioProgramBinds,
            "Studio program binds",
            MetricUnit::Count,
            CounterSource::AcceptedFrameDelta,
            CounterReduction::SumAcceptedFrames
        },
        CounterDescriptor{
            PerfCounterId::StudioMultiDrawBatches,
            "Studio MultiDraw batches",
            MetricUnit::Count,
            CounterSource::AcceptedFrameDelta,
            CounterReduction::SumAcceptedFrames
        },
        CounterDescriptor{
            PerfCounterId::StudioBatchedMeshes,
            "Studio MultiDraw meshes",
            MetricUnit::Count,
            CounterSource::AcceptedFrameDelta,
            CounterReduction::SumAcceptedFrames
        },
        CounterDescriptor{
            PerfCounterId::StudioSingleMeshes,
            "Studio single meshes",
            MetricUnit::Count,
            CounterSource::AcceptedFrameDelta,
            CounterReduction::SumAcceptedFrames
        },
        CounterDescriptor{
            PerfCounterId::StudioFlushBrush,
            "Studio flush brush",
            MetricUnit::Count,
            CounterSource::AcceptedFrameDelta,
            CounterReduction::SumAcceptedFrames
        },
        CounterDescriptor{
            PerfCounterId::StudioFlushFallback,
            "Studio flush stock fallback",
            MetricUnit::Count,
            CounterSource::AcceptedFrameDelta,
            CounterReduction::SumAcceptedFrames
        },
        CounterDescriptor{
            PerfCounterId::StudioFlushClient,
            "Studio flush client drawing",
            MetricUnit::Count,
            CounterSource::AcceptedFrameDelta,
            CounterReduction::SumAcceptedFrames
        },
        CounterDescriptor{
            PerfCounterId::StudioFlushStageEnd,
            "Studio flush stage end",
            MetricUnit::Count,
            CounterSource::AcceptedFrameDelta,
            CounterReduction::SumAcceptedFrames
        },
        CounterDescriptor{
            PerfCounterId::StudioFlushCapacity,
            "Studio flush capacity",
            MetricUnit::Count,
            CounterSource::AcceptedFrameDelta,
            CounterReduction::SumAcceptedFrames
        },
        CounterDescriptor{
            PerfCounterId::StudioFlushResource,
            "Studio flush resource",
            MetricUnit::Count,
            CounterSource::AcceptedFrameDelta,
            CounterReduction::SumAcceptedFrames
        },
        CounterDescriptor{
            PerfCounterId::StudioFlushImmediate,
            "Studio flush immediate",
            MetricUnit::Count,
            CounterSource::AcceptedFrameDelta,
            CounterReduction::SumAcceptedFrames
        },
        CounterDescriptor{
            PerfCounterId::StudioFlushMutableSkin,
            "Studio flush remap",
            MetricUnit::Count,
            CounterSource::AcceptedFrameDelta,
            CounterReduction::SumAcceptedFrames
        },
        CounterDescriptor{
            PerfCounterId::StudioImmediateOutsideOpaque,
            "Studio immediate outside opaque",
            MetricUnit::Count,
            CounterSource::AcceptedFrameDelta,
            CounterReduction::SumAcceptedFrames
        },
        CounterDescriptor{
            PerfCounterId::StudioImmediateQueueDisabled,
            "Studio immediate queue disabled",
            MetricUnit::Count,
            CounterSource::AcceptedFrameDelta,
            CounterReduction::SumAcceptedFrames
        },
        CounterDescriptor{
            PerfCounterId::StudioImmediateCallback,
            "Studio immediate callback",
            MetricUnit::Count,
            CounterSource::AcceptedFrameDelta,
            CounterReduction::SumAcceptedFrames
        },
        CounterDescriptor{
            PerfCounterId::StudioImmediateMutableSkin,
            "Studio immediate remap",
            MetricUnit::Count,
            CounterSource::AcceptedFrameDelta,
            CounterReduction::SumAcceptedFrames
        }
    };

    constexpr bool HasCapability(const StageDescriptor& stage, ncl_perf::PerfScopeCapability capability)
    {
        return (uint32_t(stage.scope.capabilities) & uint32_t(capability)) != 0;
    }

    template <size_t Size>
    constexpr const StageDescriptor* FindStage(const std::array<StageDescriptor, Size>& stages, ncl_perf::PerfScopeId_t scope_id)
    {
        for (const StageDescriptor& stage : stages)
        {
            if (stage.scope.scope_id == scope_id)
            {
                return &stage;
            }
        }

        return nullptr;
    }

    template <size_t Size>
    constexpr const SeriesDescriptor* FindSeries(const std::array<SeriesDescriptor, Size>& series, PerfSeries id)
    {
        for (const SeriesDescriptor& descriptor : series)
        {
            if (descriptor.id == id)
            {
                return &descriptor;
            }
        }

        return nullptr;
    }

    inline constexpr std::array<const StageDescriptor*, ncl_perf::kPerfScopeCapacity> kStageLookup = [] {
        std::array<const StageDescriptor*, ncl_perf::kPerfScopeCapacity> lookup{};

        for (const StageDescriptor& stage : kStages)
        {
            if (stage.scope.scope_id < lookup.size())
            {
                lookup[stage.scope.scope_id] = &stage;
            }
        }

        return lookup;
    }();

    inline constexpr std::array<const SeriesDescriptor*, uint32_t(PerfSeries::Count)> kSeriesLookup = [] {
        std::array<const SeriesDescriptor*, uint32_t(PerfSeries::Count)> lookup{};

        for (const SeriesDescriptor& series : kSeries)
        {
            if (uint32_t(series.id) < lookup.size())
            {
                lookup[uint32_t(series.id)] = &series;
            }
        }

        return lookup;
    }();

    inline constexpr std::array<const CounterDescriptor*, uint32_t(PerfCounterId::Count)> kCounterLookup = [] {
        std::array<const CounterDescriptor*, uint32_t(PerfCounterId::Count)> lookup{};

        for (const CounterDescriptor& counter : kCounters)
        {
            if (uint32_t(counter.id) < lookup.size())
            {
                lookup[uint32_t(counter.id)] = &counter;
            }
        }

        return lookup;
    }();

    constexpr const StageDescriptor* Stage(ncl_perf::PerfScopeId_t scope_id)
    {
        return scope_id < kStageLookup.size() ? kStageLookup[scope_id] : nullptr;
    }

    constexpr const SeriesDescriptor* Series(PerfSeries id)
    {
        return uint32_t(id) < kSeriesLookup.size() ? kSeriesLookup[uint32_t(id)] : nullptr;
    }

    constexpr const CounterDescriptor* Counter(PerfCounterId id)
    {
        return uint32_t(id) < kCounterLookup.size() ? kCounterLookup[uint32_t(id)] : nullptr;
    }

    constexpr const GaugeDescriptor* Gauge(PerfGaugeId id)
    {
        for (const GaugeDescriptor& gauge : kGauges)
        {
            if (gauge.id == id)
            {
                return &gauge;
            }
        }

        return nullptr;
    }

    inline void ReduceScalar(PerfScalarSummary& summary, const PerfScalarValue& sample)
    {
        if (sample.state != ncl_perf::PerfSampleState::Measured)
        {
            return;
        }

        summary.maximum = summary.count ? std::max(summary.maximum, sample.value) : sample.value;
        ++summary.count;
        summary.last = sample.value;
        summary.mean = std::lerp(summary.mean, sample.value, 1.0 / double(summary.count));
        summary.state = ncl_perf::PerfSampleState::Measured;
    }

    constexpr bool IsPresentedIn(const SeriesDescriptor& series, SeriesPlacement placement)
    {
        if (series.presentation.placement != placement)
        {
            return false;
        }

        switch (placement)
        {
            case SeriesPlacement::Cpu:
            case SeriesPlacement::Gpu:
                return series.presentation.overview;
            case SeriesPlacement::Footer:
                return true;
            default:
                return false;
        }
    }

    template <size_t Size>
    constexpr uint32_t CountSeries(const std::array<SeriesDescriptor, Size>& catalog, SeriesPlacement placement)
    {
        uint32_t count = 0;

        for (const SeriesDescriptor& series : catalog)
        {
            count += IsPresentedIn(series, placement);
        }

        return count;
    }

    constexpr uint32_t CountSeries(SeriesPlacement placement)
    {
        return CountSeries(kSeries, placement);
    }

    constexpr uint32_t CountOverviewSeries()
    {
        return CountSeries(SeriesPlacement::Cpu) + CountSeries(SeriesPlacement::Gpu);
    }

    template <size_t Size>
    constexpr const SeriesDescriptor* OrderedSeries(
        const std::array<SeriesDescriptor, Size>& catalog,
        SeriesPlacement placement,
        uint32_t order
    )
    {
        for (const SeriesDescriptor& series : catalog)
        {
            if (IsPresentedIn(series, placement) && series.presentation.order == order)
            {
                return &series;
            }
        }

        return nullptr;
    }

    constexpr const SeriesDescriptor* OrderedSeries(SeriesPlacement placement, uint32_t order)
    {
        return OrderedSeries(kSeries, placement, order);
    }

    constexpr const SeriesDescriptor* GroupSeries(SeriesPlacement placement, GroupRole role, uint32_t part_order = 0)
    {
        for (const SeriesDescriptor& series : kSeries)
        {
            if (series.presentation.placement == placement && series.presentation.role == role &&
                (role != GroupRole::Part || series.presentation.part_order == part_order))
            {
                return &series;
            }
        }

        return nullptr;
    }

    constexpr uint32_t CountParts(SeriesPlacement placement)
    {
        uint32_t count = 0;

        for (const SeriesDescriptor& series : kSeries)
        {
            count += series.presentation.placement == placement && series.presentation.role == GroupRole::Part;
        }

        return count;
    }

    constexpr uint32_t CountStudioCosts()
    {
        uint32_t count = 0;

        for (const StageDescriptor& stage : kStages)
        {
            count += stage.studio_cost;
        }

        return count;
    }

    constexpr const StageDescriptor* StudioCost(uint32_t order)
    {
        for (const StageDescriptor& stage : kStages)
        {
            if (stage.studio_cost && stage.studio_order == order)
            {
                return &stage;
            }
        }

        return nullptr;
    }

    constexpr bool IsSummarySample(const SeriesDescriptor& series, const PerfValue& value)
    {
        return value.state == ncl_perf::PerfSampleState::Measured &&
               (series.aggregation != SeriesAggregation::PositiveDurationDistribution || value.duration_ns != 0);
    }

    constexpr const char* UnitLabel(MetricUnit unit)
    {
        switch (unit)
        {
            case MetricUnit::Nanoseconds:
                return "ms";
            case MetricUnit::Packets:
                return "packets";
            default:
                return "count";
        }
    }

    constexpr bool HasName(const char* name)
    {
        return name && name[0];
    }

    constexpr bool HasScopeName(const char* name)
    {
        if (!HasName(name))
        {
            return false;
        }

        for (uint32_t length = 0; length < ncl_perf::kPerfNameCapacity; ++length)
        {
            if (!name[length])
            {
                return true;
            }
        }

        return false;
    }

    template <size_t Size>
    constexpr bool ValidateStages(const std::array<StageDescriptor, Size>& stages)
    {
        if (!Size || Size > ncl_perf::kPerfScopeCapacity)
        {
            return false;
        }

        uint32_t costs = 0;

        for (const StageDescriptor& stage : stages)
        {
            if (!stage.scope.scope_id || stage.scope.scope_id >= ncl_perf::kPerfScopeCapacity || !HasScopeName(stage.scope.name) ||
                uint32_t(stage.scope.capabilities) > uint32_t(ncl_perf::PerfScopeCapability::CpuGpu) ||
                (stage.cpu_level != CollectionLevel::Ordinary && stage.cpu_level != CollectionLevel::StudioDetail) ||
                (stage.cpu_level == CollectionLevel::StudioDetail && !HasCapability(stage, ncl_perf::PerfScopeCapability::Cpu)) ||
                (stage.studio_other_cost && !stage.studio_detail) ||
                ((stage.studio_cost || stage.studio_detail) && stage.cpu_level != CollectionLevel::StudioDetail) ||
                (stage.studio_cost && (!HasName(stage.studio_label) || stage.studio_order >= kStudioCostCapacity)))
            {
                return false;
            }

            costs += stage.studio_cost;

            for (const StageDescriptor& other : stages)
            {
                if (&stage != &other && (stage.scope.scope_id == other.scope.scope_id ||
                                         (stage.studio_cost && other.studio_cost && stage.studio_order == other.studio_order)))
                {
                    return false;
                }
            }

            const StageDescriptor* ancestor = &stage;
            uint32_t depth = 1;

            while (ancestor->scope.parent_id)
            {
                ancestor = FindStage(stages, ancestor->scope.parent_id);

                if (!ancestor || ++depth > ncl_perf::kPerfStackCapacity)
                {
                    return false;
                }
            }
        }

        return costs <= kStudioCostCapacity;
    }

    template <size_t StageCount, size_t SeriesCount, size_t CounterCount>
    constexpr bool ValidateCatalog(
        const std::array<StageDescriptor, StageCount>& stages,
        const std::array<SeriesDescriptor, SeriesCount>& series,
        const std::array<CounterDescriptor, CounterCount>& counters
    )
    {
        if (!ValidateStages(stages) || SeriesCount != uint32_t(PerfSeries::Count) || CounterCount != uint32_t(PerfCounterId::Count))
        {
            return false;
        }

        for (const SeriesDescriptor& descriptor : series)
        {
            if (uint32_t(descriptor.id) >= uint32_t(PerfSeries::Count) || !HasName(descriptor.name) ||
                descriptor.unit != MetricUnit::Nanoseconds ||
                (descriptor.domain != PerfRecordKind::Host && descriptor.domain != PerfRecordKind::Output))
            {
                return false;
            }

            const StageDescriptor* source = FindStage(stages, descriptor.scope_id);

            switch (descriptor.source)
            {
                case SeriesSource::CpuInclusive:
                    if (!source || !HasCapability(*source, ncl_perf::PerfScopeCapability::Cpu) ||
                        descriptor.domain != PerfRecordKind::Host ||
                        (descriptor.availability != AvailabilityPolicy::RegisteredStage &&
                         descriptor.availability != AvailabilityPolicy::InactiveWhenUnused))
                    {
                        return false;
                    }
                    break;
                case SeriesSource::MonitorOutputCost:
                    if (!source || !HasCapability(*source, ncl_perf::PerfScopeCapability::Cpu) ||
                        descriptor.domain != PerfRecordKind::Output || descriptor.availability != AvailabilityPolicy::CompletedOutputCost)
                    {
                        return false;
                    }
                    break;
                case SeriesSource::PresentedInterval:
                    if (descriptor.scope_id || descriptor.domain != PerfRecordKind::Output ||
                        descriptor.availability != AvailabilityPolicy::PresentedBaseline)
                    {
                        return false;
                    }
                    break;
                case SeriesSource::GpuRollup:
                    if (descriptor.scope_id || descriptor.domain != PerfRecordKind::Output ||
                        (descriptor.availability != AvailabilityPolicy::DelayedGpu &&
                         descriptor.availability != AvailabilityPolicy::DelayedGpuInactiveWhenUnused))
                    {
                        return false;
                    }
                    break;
                default:
                    return false;
            }

            SeriesAggregation expected = descriptor.source == SeriesSource::PresentedInterval
                                             ? SeriesAggregation::PositiveDurationDistribution
                                             : SeriesAggregation::DurationDistribution;

            if (descriptor.aggregation != expected || !HasName(descriptor.presentation.label))
            {
                return false;
            }

            const SeriesPresentation& presentation = descriptor.presentation;
            bool grouped = presentation.placement == SeriesPlacement::Cpu || presentation.placement == SeriesPlacement::Gpu;

            if (uint32_t(presentation.role) > uint32_t(GroupRole::Part) ||
                (!grouped && presentation.placement != SeriesPlacement::Hidden && presentation.placement != SeriesPlacement::Footer) ||
                (grouped && (presentation.role == GroupRole::None || !presentation.overview)) ||
                (!grouped && (presentation.role != GroupRole::None || presentation.overview || presentation.fallback)) ||
                (presentation.fallback && presentation.role != GroupRole::Interval && presentation.role != GroupRole::Total) ||
                (presentation.placement == SeriesPlacement::Gpu && descriptor.source != SeriesSource::GpuRollup) ||
                (presentation.placement == SeriesPlacement::Cpu && descriptor.domain != PerfRecordKind::Host &&
                 presentation.role != GroupRole::Interval) ||
                (presentation.role == GroupRole::Interval && descriptor.source != SeriesSource::PresentedInterval) ||
                (presentation.role == GroupRole::Part && presentation.part_order >= kGroupPartCapacity))
            {
                return false;
            }

            uint32_t presentation_count = 0;
            uint32_t part_count = 0;
            uint32_t total_count = 0;

            for (const SeriesDescriptor& other : series)
            {
                if (&descriptor != &other && descriptor.id == other.id)
                {
                    return false;
                }
                if (presentation.placement != other.presentation.placement)
                {
                    continue;
                }

                presentation_count += IsPresentedIn(other, presentation.placement);
                part_count += other.presentation.role == GroupRole::Part;
                total_count += other.presentation.role == GroupRole::Total;

                if (&descriptor != &other &&
                    (presentation.order == other.presentation.order ||
                     (presentation.role == other.presentation.role &&
                      (presentation.role == GroupRole::Interval || presentation.role == GroupRole::Total ||
                       (presentation.role == GroupRole::Part && presentation.part_order == other.presentation.part_order)))))
                {
                    return false;
                }
            }

            if (IsPresentedIn(descriptor, presentation.placement) && presentation.order >= presentation_count)
            {
                return false;
            }

            if (grouped && (part_count > kGroupPartCapacity || total_count != 1 ||
                            (presentation.role == GroupRole::Part && presentation.part_order >= part_count)))
            {
                return false;
            }

            if (descriptor.source == SeriesSource::GpuRollup)
            {
                bool has_stage = false;

                for (const StageDescriptor& stage : stages)
                {
                    has_stage = has_stage || stage.gpu_series == descriptor.id;
                }

                if (!has_stage)
                {
                    return false;
                }
            }
        }

        for (const StageDescriptor& stage : stages)
        {
            if (stage.gpu_series == PerfSeries::Count)
            {
                continue;
            }

            const SeriesDescriptor* rollup = FindSeries(series, stage.gpu_series);

            if (!HasCapability(stage, ncl_perf::PerfScopeCapability::Gpu) || !rollup || rollup->source != SeriesSource::GpuRollup)
            {
                return false;
            }
        }

        for (const CounterDescriptor& counter : counters)
        {
            bool cumulative = counter.source == CounterSource::GpuDroppedScopes || counter.source == CounterSource::GpuExpiredResults;

            if (uint32_t(counter.id) >= uint32_t(PerfCounterId::Count) || !HasName(counter.name) || counter.unit != MetricUnit::Count ||
                (counter.source != CounterSource::AcceptedFrameDelta && counter.source != CounterSource::CpuDroppedScopes && !cumulative) ||
                counter.reduction != (cumulative ? CounterReduction::LastObservation : CounterReduction::SumAcceptedFrames))
            {
                return false;
            }

            for (const CounterDescriptor& other : counters)
            {
                if (&counter != &other && counter.id == other.id)
                {
                    return false;
                }
            }
        }

        return true;
    }

    template <size_t Size>
    constexpr bool ValidateGauges(const std::array<GaugeDescriptor, Size>& gauges)
    {
        if (Size != uint32_t(PerfGaugeId::Count))
        {
            return false;
        }

        for (const GaugeDescriptor& gauge : gauges)
        {
            if (uint32_t(gauge.id) >= uint32_t(PerfGaugeId::Count) || !HasName(gauge.name) || !HasName(gauge.label) ||
                gauge.domain != PerfRecordKind::Host || gauge.unit != MetricUnit::Packets ||
                gauge.aggregation != GaugeAggregation::LastObservation || gauge.placement != GaugePlacement::Studio)
            {
                return false;
            }

            for (const GaugeDescriptor& other : gauges)
            {
                if (&gauge != &other && gauge.id == other.id)
                {
                    return false;
                }
            }
        }

        return true;
    }

    static_assert(ValidateGauges(kGauges));
    static_assert(ncl_perf::kPerfScopeCapacity == 128 && ncl_perf::kPerfStackCapacity == 64);
    static_assert(ValidateCatalog(kStages, kSeries, kCounters));
    static_assert(CountStudioCosts() == kStudioCostCapacity);
} // namespace performance
