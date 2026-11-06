#pragma once

#include <memory>
#include <vector>

#include <nitroapi/NitroApiInterface.h>
#include <next_engine_mini/perf_cvars.h>

#include "common/cvar.h"
#include "performance/PerformanceMonitor.h"
#include "performance/PerformanceMapper.h"
#include "performance/PerformanceFrameSubscription.h"
#include "performance/PerformanceNetwork.h"
#include "performance/GpuQueryBackend.h"
#include "performance/PerformancePanel.h"
#include "performance/TracyGpuAdapter.h"
#include "performance/performance_scopes.h"
#include "graphics/render/RenderDevice.h"
#include "graphics/render/RendererLifecycle.h"

namespace performance
{
    class PerformanceRuntime
    {
    private:
        friend class PerformanceRuntimeTestAccess;
        nitroapi::EngineData& engine_;
        nitroapi::ClientData& client_;
        nitroapi::SDL2Data* sdl_{};
        bool drawable_size_available_{};
        bool window_size_available_{};
        bool current_window_available_{};
        bool swap_available_{};
        nitroapi::FrameLifecycleInterface* lifecycle_{};
        PerformanceMonitor& monitor_;
        PerformanceMapper& mapper_;
        ncl_perf::PerfClockInterface& clock_;
        render::RenderDevice& device_;
        render::RendererLifecycle& renderer_lifecycle_;
        TextRenderInterface* text_{};
        vgui2::ILocalize* localization_{};
        ConsoleText_t console_text_{};
        int engine_build_{};
        FrameSubscription subscription_;
        const goldsrc::cvar_t* fps_max_{};
        const goldsrc::cvar_t* vsync_{};
        const goldsrc::cvar_t* speeds_{};
        const goldsrc::cvar_t* studio_shader_{};
        GpuQueryBackend gpu_backend_;
        PerfMapRenderView mapper_render_view_;
        bool mapper_render_view_captured_{};
        uint64_t mapper_session_epoch_ = 1;
#if USE_PROFILER
        TracyPerformanceBackend trace_backend_;
#endif
        std::unique_ptr<PerformancePanel> panel_;
        PerformanceNetworkMonitor network_;
        PerfNetworkSnapshot network_snapshot_;
        uint32_t network_columns_ = kPerfGraphCapacity;
        PerfFrameGraphs frame_graphs_;
        goldsrc::cvar_t* net_graph_{};
        goldsrc::cvar_t* no_refresh_{};
        bool legacy_window_size_{};
        std::shared_ptr<nitroapi::Unsubscriber> swap_subscription_;
        std::vector<std::shared_ptr<nitroapi::Unsubscriber>> stage_subscriptions_;
        goldsrc::cvar_t perf_mode_{kPerfCvarDefs[0].name, const_cast<char*>(kPerfCvarDefs[0].default_value), kPerfCvarDefs[0].flags};
        goldsrc::cvar_t perf_gpu_{kPerfCvarDefs[1].name, const_cast<char*>(kPerfCvarDefs[1].default_value), kPerfCvarDefs[1].flags};
        goldsrc::cvar_t perf_scale_{kPerfCvarDefs[2].name, const_cast<char*>(kPerfCvarDefs[2].default_value), kPerfCvarDefs[2].flags};

        goldsrc::cvar_t perf_net_{kPerfCvarDefs[3].name, const_cast<char*>(kPerfCvarDefs[3].default_value), kPerfCvarDefs[3].flags};

        goldsrc::cvar_t perf_map_{kPerfCvarDefs[4].name, const_cast<char*>(kPerfCvarDefs[4].default_value), kPerfCvarDefs[4].flags};

        goldsrc::cvar_t perf_batch_{kPerfCvarDefs[5].name, const_cast<char*>(kPerfCvarDefs[5].default_value), kPerfCvarDefs[5].flags};
        goldsrc::cvar_t perf_background_alpha_{
            kPerfCvarDefs[6].name,
            const_cast<char*>(kPerfCvarDefs[6].default_value),
            kPerfCvarDefs[6].flags
        };

        cvarhook_t batch_hook_{BatchChanged};
        cvarhook_t mapper_hook_{MapperChanged};
        cvarhook_t network_hook_{NetworkChanged};
        cvarhook_t mode_hook_{ModeChanged};
        cvarhook_t gpu_hook_{GpuChanged};
        cvarhook_t scale_hook_{ScaleChanged};

    public:
        PerformanceRuntime(
            nitroapi::EngineData& engine,
            nitroapi::ClientData& client,
            nitroapi::SDL2Data* sdl,
            nitroapi::FrameLifecycleInterface* lifecycle,
            PerformanceMonitor& monitor,
            PerformanceMapper& mapper,
            ncl_perf::PerfClockInterface& clock,
            render::RenderDevice& device,
            render::RendererLifecycle& renderer_lifecycle,
            TextRenderInterface* text,
            int engine_build
        );
        PerformanceRuntime(const PerformanceRuntime&) = delete;
        PerformanceRuntime& operator=(const PerformanceRuntime&) = delete;
        void Bind();
        void BindControls(
            const goldsrc::cvar_t* fps_max,
            const goldsrc::cvar_t* vsync,
            const goldsrc::cvar_t* speeds,
            const goldsrc::cvar_t* studio_shader,
            goldsrc::cvar_t* no_refresh,
            goldsrc::cvar_t* net_graph
        );
        void RegisterControls();
        void BindPresentation(vgui2::ILocalize* localization, ConsoleText_t console_text);
        void ContextCreated();
        void ContextChanging();
        void ReleaseContext();
        void AbandonContext();
        void MapChanged();
        void Shutdown();
        void UpdateMapper();
        void UpdateNetwork();
        void OnBatchChanged(goldsrc::cvar_t* cvar);
        void OnMapperChanged(goldsrc::cvar_t* cvar);
        void OnNetworkChanged(goldsrc::cvar_t* cvar);
        void OnModeChanged(goldsrc::cvar_t* cvar);
        void OnGpuChanged(goldsrc::cvar_t* cvar);
        void OnScaleChanged(goldsrc::cvar_t* cvar);
        void DumpPerformance();
        void ResetPerformance();
        const PerfNetworkSnapshot& get_network_snapshot() const
        {
            return network_snapshot_;
        }

    private:
        void UpdateMapperLevel(const goldsrc::cvar_t* cvar);
        void ResetMapperSession();
        ncl_perf::PerfTimestamp_t Timestamp();
        void UpdateNetworkState(ncl_perf::PerfTimestamp_t now, bool normal_message = false);
        void AbandonMonitorContext();
        void ReleaseMonitorContext();
        void UpdateEngineInfo();
        void PrintDumpText(const std::vector<char>& text);
        void DumpMapper();
        void PublishDumpCapture();
        bool DrawableSize(void* window, int& width, int& height);
        bool ShouldHideNetGraph();
        template <class TFunction>
        bool HasFunction(const TFunction& function)
        {
            return function.GetHook() && function.GetHook()->GetOriginalFunctionAddress();
        }

        template <class... TArgs>
        void BindCpuStage(
            nitroapi::NitroFunctionVoid<TArgs...>& function,
            ncl_perf::PerfScopeId_t scope_id,
            std::source_location location = std::source_location::current()
        )
        {
            monitor_.SetStageAvailable(scope_id, HasFunction(function));

            if (!HasFunction(function))
            {
                return;
            }

            stage_subscriptions_.emplace_back(
                function |= [this, scope_id, location](TArgs... args, nitroapi::NextHandlerInterface<void, TArgs...>* next) {
                    PerformanceRuntime& runtime = *this;
                    LocalCpuScope scope(&runtime.monitor_, scope_id, location);
                    next->Invoke(std::forward<TArgs>(args)...);
                }
            );
        }

        template <class TResult, class... TArgs>
        void BindCpuStage(
            nitroapi::NitroFunction<TResult, TArgs...>& function,
            ncl_perf::PerfScopeId_t scope_id,
            std::source_location location = std::source_location::current()
        )
        {
            monitor_.SetStageAvailable(scope_id, HasFunction(function));

            if (!HasFunction(function))
            {
                return;
            }

            stage_subscriptions_.emplace_back(
                function |= [this, scope_id, location](TArgs... args, nitroapi::NextHandlerInterface<TResult, TArgs...>* next) -> TResult {
                    PerformanceRuntime& runtime = *this;
                    LocalCpuScope scope(&runtime.monitor_, scope_id, location);

                    return next->Invoke(std::forward<TArgs>(args)...);
                }
            );
        }

        static void BatchChanged(goldsrc::cvar_t* cvar);
        static void MapperChanged(goldsrc::cvar_t* cvar);
        static void NetworkChanged(goldsrc::cvar_t* cvar);
        static void ModeChanged(goldsrc::cvar_t* cvar);
        static void GpuChanged(goldsrc::cvar_t* cvar);
        static void ScaleChanged(goldsrc::cvar_t* cvar);
        static void DumpCommand();
        static void ResetCommand();
    };

    void BindControlSink(PerformanceRuntime& runtime);
} // namespace performance
