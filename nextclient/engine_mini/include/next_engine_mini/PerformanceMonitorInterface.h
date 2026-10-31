#pragma once

#include <source_location>

#include <next_engine_mini/PerformanceData.h>

#define PERFORMANCE_MONITOR_INTERFACE_VERSION "PerformanceMonitor001"

namespace performance
{
    namespace PerfStage
    {
        constexpr ncl_perf::PerfScopeId_t Host = 1;
        constexpr ncl_perf::PerfScopeId_t Commands = 10;
        constexpr ncl_perf::PerfScopeId_t Client = 11;
        constexpr ncl_perf::PerfScopeId_t Packets = 12;
        constexpr ncl_perf::PerfScopeId_t Prediction = 13;
        constexpr ncl_perf::PerfScopeId_t Entities = 14;
        constexpr ncl_perf::PerfScopeId_t TempEntities = 15;
        constexpr ncl_perf::PerfScopeId_t Screen = 20;
        constexpr ncl_perf::PerfScopeId_t View = 21;
        constexpr ncl_perf::PerfScopeId_t World = 22;
        constexpr ncl_perf::PerfScopeId_t ViewModel = 23;
        constexpr ncl_perf::PerfScopeId_t Hud = 24;
        constexpr ncl_perf::PerfScopeId_t Studio = 30;
        constexpr ncl_perf::PerfScopeId_t StudioGeometry = 31;
        constexpr ncl_perf::PerfScopeId_t StudioMaterial = 32;
        constexpr ncl_perf::PerfScopeId_t StudioShader = 33;
        constexpr ncl_perf::PerfScopeId_t StudioStock = 34;
        constexpr ncl_perf::PerfScopeId_t ShaderCompile = 35;
        constexpr ncl_perf::PerfScopeId_t ShaderLink = 36;
        constexpr ncl_perf::PerfScopeId_t Radar = 40;
        constexpr ncl_perf::PerfScopeId_t RadarPrepare = 41;
        constexpr ncl_perf::PerfScopeId_t RadarDraw = 42;
        constexpr ncl_perf::PerfScopeId_t RadarLayout = 43;
        constexpr ncl_perf::PerfScopeId_t RadarCapture = 44;
        constexpr ncl_perf::PerfScopeId_t RadarLayer = 45;
        constexpr ncl_perf::PerfScopeId_t RadarPolicy = 46;
        constexpr ncl_perf::PerfScopeId_t RadarVisibility = 47;
        constexpr ncl_perf::PerfScopeId_t RadarBackdrop = 48;
        constexpr ncl_perf::PerfScopeId_t RadarObjectives = 49;
        constexpr ncl_perf::PerfScopeId_t RadarPlayers = 50;
        constexpr ncl_perf::PerfScopeId_t RadarText = 51;
        constexpr ncl_perf::PerfScopeId_t RadarBatch = 52;
        constexpr ncl_perf::PerfScopeId_t RadarBackdropCapture = 53;
        constexpr ncl_perf::PerfScopeId_t RadarMapDraw = 54;
        constexpr ncl_perf::PerfScopeId_t Sound = 60;
        constexpr ncl_perf::PerfScopeId_t SoundExtra = 61;
        constexpr ncl_perf::PerfScopeId_t Swap = 62;
        constexpr ncl_perf::PerfScopeId_t Monitor = 63;
        constexpr ncl_perf::PerfScopeId_t StudioGlSave = 70;
        constexpr ncl_perf::PerfScopeId_t StudioGlRestore = 71;
        constexpr ncl_perf::PerfScopeId_t StudioProgramBind = 72;
        constexpr ncl_perf::PerfScopeId_t StudioBufferBind = 73;
        constexpr ncl_perf::PerfScopeId_t StudioBoneUpload = 74;
        constexpr ncl_perf::PerfScopeId_t StudioModelStateCapture = 75;
        constexpr ncl_perf::PerfScopeId_t StudioBatchStateSave = 76;
        constexpr ncl_perf::PerfScopeId_t StudioRestoreBindings = 77;
        constexpr ncl_perf::PerfScopeId_t StudioRestoreMatrices = 78;
        constexpr ncl_perf::PerfScopeId_t StudioRestoreTextures = 79;
        constexpr ncl_perf::PerfScopeId_t StudioDataPrepare = 80;
        constexpr ncl_perf::PerfScopeId_t StudioMeshSort = 81;
        constexpr ncl_perf::PerfScopeId_t StudioStateApply = 82;
        constexpr ncl_perf::PerfScopeId_t StudioUniformSetup = 83;
        constexpr ncl_perf::PerfScopeId_t StudioTextureSetup = 84;
        constexpr ncl_perf::PerfScopeId_t StudioInstancePlan = 85;
        constexpr ncl_perf::PerfScopeId_t StudioInstanceUpload = 86;
        constexpr ncl_perf::PerfScopeId_t StudioInstanceSubmit = 87;
    } // namespace PerfStage

    // All methods belong to the main engine/frame owner thread, including const reads and ReportWorker.
    // CPU scopes close LIFO on that thread; GPU scopes also require its matching live render context.
    class PerformanceMonitorInterface
    {
    public:
        virtual ~PerformanceMonitorInterface() = default;
        virtual ncl_perf::PerfScopeToken_t BeginCpu(
            ncl_perf::PerfScopeId_t scope_id,
            std::source_location location = std::source_location::current()
        ) = 0;
        virtual void EndCpu(ncl_perf::PerfScopeToken_t token) = 0;
        virtual ncl_perf::PerfScopeToken_t BeginGpu(
            ncl_perf::PerfScopeId_t scope_id,
            std::source_location location = std::source_location::current()
        ) = 0;
        virtual void EndGpu(ncl_perf::PerfScopeToken_t token) = 0;
        virtual void AddCounter(PerfCounterId counter, uint64_t value) = 0;
        virtual bool GetSnapshot(PerfSnapshot& output) const = 0;
        virtual uint32_t map_generation() const = 0;
        virtual void ReportWorker(const PerfWorkerReport& report) = 0;
        virtual bool SetGauge(PerfGaugeId gauge, double value) = 0;
    };
} // namespace performance
