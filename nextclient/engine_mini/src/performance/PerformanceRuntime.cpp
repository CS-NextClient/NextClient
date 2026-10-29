#include "performance/PerformanceRuntime.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <memory>
#include <vector>

#include <glad/glad.h>
#include <SDL_video.h>
#include <SDL_version.h>
#include <ncl_perf/profiler.h>

#include "performance/performance_integration.h"
#include "common/cmd.h"
#include "common/cvar.h"
#include "console/console.h"
#include "performance/performance_dump.h"
#include "graphics/render/render_context.h"

namespace performance
{
    PerformanceRuntime::PerformanceRuntime(
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
    ) :
        engine_(engine),
        client_(client),
        sdl_(sdl),
        lifecycle_(lifecycle),
        monitor_(monitor),
        mapper_(mapper),
        clock_(clock),
        device_(device),
        renderer_lifecycle_(renderer_lifecycle),
        text_(text),
        engine_build_(engine_build),
        subscription_(monitor)
    {}

    void PerformanceRuntime::BindControls(
        const goldsrc::cvar_t* fps_max,
        const goldsrc::cvar_t* vsync,
        const goldsrc::cvar_t* speeds,
        const goldsrc::cvar_t* studio_shader,
        goldsrc::cvar_t* no_refresh,
        goldsrc::cvar_t* net_graph
    )
    {
        fps_max_ = fps_max;
        vsync_ = vsync;
        speeds_ = speeds;
        studio_shader_ = studio_shader;
        no_refresh_ = no_refresh;
        net_graph_ = net_graph;
    }

    void PerformanceRuntime::OnBatchChanged(goldsrc::cvar_t* cvar)
    {
        float value = std::isfinite(cvar->value) ? std::clamp(cvar->value, 0.0f, 2.0f) : 0.0f;
        monitor_.SetBatchLevel(int(value));
    }

    void PerformanceRuntime::UpdateMapperLevel(const goldsrc::cvar_t* cvar)
    {
        float value = std::isfinite(cvar->value) ? std::clamp(cvar->value, 0.0f, 2.0f) : 0.0f;
        nitroapi::EngineData* engine = &engine_;
        mapper_.SetLevel(int(value), engine->client_static, engine->server);
    }

    void PerformanceRuntime::OnMapperChanged(goldsrc::cvar_t* cvar)
    {
        UpdateMapperLevel(cvar);
    }

    void PerformanceRuntime::UpdateMapper()
    {
        UpdateMapperLevel(&perf_map_);
        nitroapi::EngineData* engine = &engine_;
        const goldsrc::client_state_t* client =
            engine->client_static && engine->client_static->state == goldsrc::ca_active ? engine->client_state : nullptr;

        if (engine->key_dest && *engine->key_dest != goldsrc::key_game)
        {
            mapper_.ResetInput();
        }

        mapper_.UpdateWorld(client, monitor_.map_generation());
    }

    ncl_perf::PerfTimestamp_t PerformanceRuntime::Timestamp()
    {
        return clock_.Now();
    }

    void PerformanceRuntime::UpdateNetworkState(ncl_perf::PerfTimestamp_t now, bool normal_message)
    {
        nitroapi::EngineData* engine = &engine_;
        network_.Update(
            now,
            engine->client_static,
            engine->client_state,
            engine->realtime ? *engine->realtime : std::numeric_limits<double>::quiet_NaN(),
            monitor_.map_generation(),
            normal_message
        );
    }

    void PerformanceRuntime::UpdateNetwork()
    {
        ncl_perf::PerfTimestamp_t now = Timestamp();
        UpdateNetworkState(now);
        network_.BuildSnapshot(now, 0, network_snapshot_, network_columns_);
    }

    void PerformanceRuntime::OnNetworkChanged(goldsrc::cvar_t* cvar)
    {
        network_.SetEnabled(std::isfinite(cvar->value) && cvar->value >= 1.0f);
    }

    void PerformanceRuntime::OnModeChanged(goldsrc::cvar_t* cvar)
    {
        float value = std::isfinite(cvar->value) ? std::clamp(cvar->value, 0.0f, 3.0f) : 0.0f;
        monitor_.SetMode(int(value));
    }

    void PerformanceRuntime::OnGpuChanged(goldsrc::cvar_t* cvar)
    {
        monitor_.SetGpuEnabled(std::isfinite(cvar->value) && cvar->value >= 1.0f);
    }

    void PerformanceRuntime::OnScaleChanged(goldsrc::cvar_t* cvar)
    {
        monitor_.SetPanelScale(cvar->value);
    }

    void PerformanceRuntime::AbandonMonitorContext()
    {
        mapper_render_view_captured_ = false;
#if USE_PROFILER
        trace_backend_.AbandonGl();
#endif
        monitor_.AbandonGpu();

        if (panel_)
        {
            panel_->Invalidate();
        }
    }

    void PerformanceRuntime::ReleaseMonitorContext()
    {
        mapper_render_view_captured_ = false;
        if (panel_)
        {
            panel_->Invalidate();
        }

        if (monitor_.is_context(render::context::Current()))
        {
#if USE_PROFILER
            trace_backend_.ReleaseGl();
#endif
            monitor_.OnContextDestroyed();
        }
        else
        {
            AbandonMonitorContext();
        }
    }

    void PerformanceRuntime::UpdateEngineInfo()
    {
        monitor_.SetEngineInfo(
            fps_max_ ? fps_max_->value : 0,
            vsync_ ? int(vsync_->value) : 0,
            speeds_ && speeds_->value != 0,
            studio_shader_ && studio_shader_->value != 0,
            engine_build_
        );
    }

    void PerformanceRuntime::PrintDumpText(const std::vector<char>& text)
    {
        for (uint32_t offset = 0; offset + 1 < text.size(); offset += 512)
        {
            Con_Printf("%.*s", int(std::min(size_t(512), text.size() - offset - 1)), text.data() + offset);
        }
    }

    void PerformanceRuntime::DumpMapper()
    {
        const PerfMapperSnapshot& snapshot = mapper_.get_snapshot();
        uint32_t required = dump::FormatMapper(snapshot, nullptr, 0);
        std::vector<char> text(required);
        dump::FormatMapper(snapshot, text.data(), required);
        PrintDumpText(text);
    }

    void PerformanceRuntime::PublishDumpCapture()
    {
        if (!monitor_.is_dump_ready())
        {
            return;
        }

        PerfSnapshot snapshot;

        if (!monitor_.TakeDumpCapture(snapshot))
        {
            return;
        }

        Con_Printf("CPU capture: one complete Host frame; history and GPU use their own windows.\n");
        UpdateNetwork();
        UpdateMapper();
        uint32_t required = dump::Format(snapshot, nullptr, 0, &network_snapshot_, &mapper_.get_snapshot());
        std::vector<char> text(required);
        dump::Format(snapshot, text.data(), required, &network_snapshot_, &mapper_.get_snapshot());
        PrintDumpText(text);
    }

    void PerformanceRuntime::DumpPerformance()
    {
        UpdateEngineInfo();
        PerfSnapshot status;
        monitor_.GetSnapshot(status);

        if (!subscription_.is_bound() || !swap_subscription_ || !status.presenter_available)
        {
            Con_Printf("perf_dump: Host frame callbacks and main-window presentation are required.\n");
            return;
        }

        if (monitor_.RequestDumpCapture())
        {
            Con_Printf("perf_dump: capturing the next complete Host frame.\n");
        }
        else
        {
            Con_Printf("perf_dump: a capture is already pending or ready for output.\n");
        }
    }

    void PerformanceRuntime::ResetPerformance()
    {
        monitor_.Reset();
        network_.Reset();
        mapper_.Reset();
    }

    bool PerformanceRuntime::DrawableSize(void* window, int& width, int& height)
    {
        nitroapi::SDL2Data* sdl = sdl_;

        if (drawable_size_available_)
        {
            sdl->SDL_GL_GetDrawableSize(window, &width, &height);
        }
        else if (legacy_window_size_ && window_size_available_)
        {
            sdl->SDL_GetWindowSize(window, &width, &height);
        }

        return width > 0 && height > 0;
    }

    bool PerformanceRuntime::ShouldHideNetGraph()
    {
        if (!network_.is_enabled())
        {
            return false;
        }

        nitroapi::SDL2Data* sdl = sdl_;
        PerformanceMonitor& monitor = monitor_;

        if (!current_window_available_ || !swap_available_ || !monitor.is_context(render::context::Current()))
        {
            return false;
        }

        void* window = sdl->SDL_GL_GetCurrentWindow();
        int width = 0;
        int height = 0;

        return monitor.is_main_window(window) && DrawableSize(window, width, height) &&
               PerformancePanelLayout::CanShowNetwork(perf_scale_.value, width, height);
    }

    void PerformanceRuntime::Bind()
    {
        drawable_size_available_ = sdl_ && HasFunction(sdl_->SDL_GL_GetDrawableSize);
        window_size_available_ = sdl_ && HasFunction(sdl_->SDL_GetWindowSize);
        current_window_available_ = sdl_ && HasFunction(sdl_->SDL_GL_GetCurrentWindow);
        swap_available_ = sdl_ && HasFunction(sdl_->SDL_GL_SwapWindow);
#if USE_PROFILER
        monitor_.SetTraceBackend(&trace_backend_);
#endif
        nitroapi::EngineData* engine = &engine_;
        nitroapi::ClientData* client = &client_;
        BindCpuStage(engine->ClientDLL_Frame, PerfStage::Client);
        BindCpuStage(engine->CL_ReadPackets, PerfStage::Packets);

        if (HasFunction(engine->CL_ClearState))
        {
            stage_subscriptions_.emplace_back(engine->CL_ClearState |= [this](goldsrc::qboolean quiet, const auto& next) {
                PerformanceRuntime& runtime = *this;
                runtime.ResetMapperSession();
                next->Invoke(quiet);
            });
        }

        if (HasFunction(engine->CL_ParseServerMessage))
        {
            stage_subscriptions_.emplace_back(engine->CL_ParseServerMessage |= [this](goldsrc::qboolean normal_message, const auto& next) {
                PerformanceRuntime& runtime = *this;
                NCL_PROFILE_NAMED_SCOPE("Server message parsing");
                next->Invoke(normal_message);

                if (runtime.network_.is_enabled())
                {
                    runtime.UpdateNetworkState(runtime.Timestamp(), normal_message != 0);
                }
            });
        }

        if (HasFunction(client->HUD_Key_Event))
        {
            stage_subscriptions_.emplace_back(
                client->HUD_Key_Event |= [this](int down, int key, const char* binding, const auto& next) -> int {
                    PerformanceRuntime& runtime = *this;
                    if (runtime.mapper_.is_enabled())
                    {
                        runtime.UpdateMapper();
                    }

                    nitroapi::EngineData* engine = &runtime.engine_;
                    bool gameplay = engine->client_static && engine->client_static->state == goldsrc::ca_active && engine->key_dest &&
                                    *engine->key_dest == goldsrc::key_game;

                    if (runtime.mapper_.HandleKey(key, down != 0, gameplay))
                    {
                        runtime.DumpMapper();
                    }

                    return next->Invoke(down, key, binding);
                }
            );
        }

        for (nitroapi::NitroFunctionVoid<>* function : {&client->IN_ClearStates, &client->IN_DeactivateMouse})
        {
            if (HasFunction(*function))
            {
                stage_subscriptions_.emplace_back(*function |= [this](const auto& next) {
                    PerformanceRuntime& runtime = *this;
                    runtime.mapper_.ResetInput();
                    next->Invoke();
                });
            }
        }

        if (HasFunction(engine->R_SetupGL))
        {
            stage_subscriptions_.emplace_back(engine->R_SetupGL |= [this](const auto& next) {
                PerformanceRuntime& runtime = *this;
                const uint64_t session_epoch = runtime.mapper_session_epoch_;
                const uint32_t device_generation = runtime.device_.get_generation();
                void* context_identity = runtime.device_.get_context_identity();
                runtime.mapper_render_view_captured_ = false;
                next->Invoke();

                if (runtime.mapper_.get_snapshot().level < 2 || runtime.mapper_session_epoch_ != session_epoch ||
                    runtime.device_.get_generation() != device_generation || !runtime.device_.IsContext(context_identity) ||
                    !runtime.device_.IsContextCurrent())
                {
                    return;
                }

                glGetFloatv(GL_MODELVIEW_MATRIX, runtime.mapper_render_view_.modelview.data());
                glGetFloatv(GL_PROJECTION_MATRIX, runtime.mapper_render_view_.projection.data());
                glGetIntegerv(GL_VIEWPORT, runtime.mapper_render_view_.viewport.data());
                glGetDoublev(GL_DEPTH_RANGE, runtime.mapper_render_view_.depth_range.data());
                glGetIntegerv(GL_DEPTH_FUNC, &runtime.mapper_render_view_.depth_function);
                runtime.mapper_render_view_.draw_framebuffer = 0;

                if (glBindFramebuffer)
                {
                    glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &runtime.mapper_render_view_.draw_framebuffer);
                }

                runtime.mapper_render_view_captured_ = true;
            });
        }

        monitor_.SetStageAvailable(PerfStage::View, HasFunction(engine->R_RenderView));

        if (HasFunction(engine->R_RenderView))
        {
            stage_subscriptions_.emplace_back(engine->R_RenderView |= [this](const auto& next) {
                PerformanceRuntime& runtime = *this;
                LocalCpuScope cpu(&runtime.monitor_, PerfStage::View);
                PerformanceMapper& mapper = runtime.mapper_;

                if (!mapper.is_enabled())
                {
                    next->Invoke();
                    return;
                }

                runtime.UpdateMapper();
                const nitroapi::EngineData& engine = runtime.engine_;
                const goldsrc::client_static_t* client_static = engine.client_static;
                const goldsrc::client_state_t* client = engine.client_state;
                const goldsrc::model_t* world = client ? client->worldmodel : nullptr;
                const goldsrc::cactive_t connection_state = client_static ? client_static->state : goldsrc::ca_disconnected;
                const double connect_time = client_static ? client_static->netchan.connect_time : 0;
                const uint32_t map_generation = runtime.monitor_.map_generation();
                const uint64_t session_epoch = runtime.mapper_session_epoch_;
                const uint32_t device_generation = runtime.device_.get_generation();
                void* context_identity = runtime.device_.get_context_identity();
                mapper.BeginView(*engine.c_brush_polys, *engine.c_alias_polys);
                runtime.mapper_render_view_captured_ = false;
                next->Invoke();

                if (!mapper.is_enabled() || runtime.mapper_session_epoch_ != session_epoch)
                {
                    return;
                }

                const goldsrc::client_static_t* current_static = engine.client_static;
                const goldsrc::client_state_t* current_client = engine.client_state;
                if (runtime.monitor_.map_generation() != map_generation || current_static != client_static || current_client != client ||
                    (current_static &&
                     (current_static->state != connection_state || current_static->netchan.connect_time != connect_time)) ||
                    (current_client ? current_client->worldmodel : nullptr) != world)
                {
                    runtime.ResetMapperSession();
                    return;
                }

                if (runtime.device_.get_generation() != device_generation || runtime.device_.get_context_identity() != context_identity ||
                    (runtime.no_refresh_ && runtime.no_refresh_->value != 0))
                {
                    return;
                }

                bool captured = mapper.CaptureView(
                    *engine.c_brush_polys,
                    *engine.c_alias_polys,
                    {engine.cl_visedicts, size_t(*engine.cl_numvisedicts)},
                    *engine.r_refdef
                );

                if (captured && runtime.mapper_render_view_captured_ && runtime.device_.IsContextCurrent())
                {
                    panel::DrawMapOutline(mapper.get_snapshot(), runtime.mapper_render_view_);
                }
            });
        }

        BindCpuStage(engine->R_DrawWorld, PerfStage::World);
        BindCpuStage(engine->R_DrawViewModel, PerfStage::ViewModel);
        BindCpuStage(engine->S_Update, PerfStage::Sound);
        BindCpuStage(engine->S_ExtraUpdate, PerfStage::SoundExtra);
        BindCpuStage(client->HUD_CreateEntities, PerfStage::Entities);
        BindCpuStage(client->HUD_TempEntUpdate, PerfStage::TempEntities);
        BindCpuStage(client->HUD_Redraw, PerfStage::Hud);
        monitor_.SetStageAvailable(PerfStage::Screen, HasFunction(engine->SCR_UpdateScreen));

        if (HasFunction(engine->SCR_UpdateScreen))
        {
            stage_subscriptions_.emplace_back(engine->SCR_UpdateScreen |= [this](const auto& next) {
                PerformanceRuntime& runtime = *this;
                runtime.ContextChanging();
                LocalCpuScope cpu(&runtime.monitor_, PerfStage::Screen);
                LocalGpuScope gpu(&runtime.monitor_, PerfStage::Screen);
                bool hide_net_graph = runtime.ShouldHideNetGraph();

                PerformanceNetGraphMask net_graph(runtime.net_graph_, hide_net_graph);

                if (runtime.mapper_.is_enabled())
                {
                    runtime.mapper_.BeginFrame();
                }

                next->Invoke();
            });
        }

        if (HasFunction(engine->Netchan_Transmit))
        {
            stage_subscriptions_.emplace_back(
                engine->Netchan_Transmit |= [this](goldsrc::netchan_t* channel, int length, uint8_t* data, const auto& next) {
                    PerformanceRuntime& runtime = *this;
                    goldsrc::client_static_t* client_static = runtime.engine_.client_static;
                    bool observe = runtime.network_.is_enabled() && client_static && channel == &client_static->netchan;

                    if (observe)
                    {
                        runtime.UpdateNetworkState(runtime.Timestamp());
                    }

                    next->Invoke(channel, length, data);

                    if (observe && runtime.network_.is_enabled())
                    {
                        const goldsrc::client_static_t* current_client = runtime.engine_.client_static;

                        if (current_client && channel == &current_client->netchan)
                        {
                            runtime.network_.ObserveTraffic(current_client->netchan);
                        }
                    }
                }
            );
        }

        if (HasFunction(engine->Netchan_Process))
        {
            stage_subscriptions_.emplace_back(
                engine->Netchan_Process |= [this](goldsrc::netchan_t* channel, const auto& next) -> goldsrc::qboolean {
                    PerformanceRuntime& runtime = *this;
                    goldsrc::client_static_t* client_static = runtime.engine_.client_static;
                    bool observe = runtime.network_.is_enabled() && client_static && channel == &client_static->netchan;

                    if (observe)
                    {
                        runtime.UpdateNetworkState(runtime.Timestamp());
                    }

                    goldsrc::qboolean result = next->Invoke(channel);

                    if (observe && runtime.network_.is_enabled())
                    {
                        const goldsrc::client_static_t* current_client = runtime.engine_.client_static;

                        if (current_client && channel == &current_client->netchan)
                        {
                            runtime.network_.ObserveTraffic(current_client->netchan);
                        }
                    }

                    return result;
                }
            );
        }

        subscription_.Bind(nullptr);

        nitroapi::FrameLifecycleInterface* lifecycle = lifecycle_;

        if (lifecycle)
        {
            if (HasFunction(engine->Host_Frame) && HasFunction(engine->Host_FrameInternal) && HasFunction(engine->Host_FilterTime))
            {
                stage_subscriptions_.emplace_back(engine->Host_Frame += [](float, int, int*, int) {});
                stage_subscriptions_.emplace_back(engine->Host_FrameInternal += [](float) {});
                stage_subscriptions_.emplace_back(engine->Host_FilterTime += [](float, int) {});
            }
            subscription_.Bind(lifecycle);
        }

        nitroapi::SDL2Data* sdl = sdl_;

        if (sdl && HasFunction(sdl->SDL_GetVersion))
        {
            SDL_version version{};
            sdl->SDL_GetVersion(&version);
            legacy_window_size_ =
                presentation::UsesLegacyWindowSize(version.major, version.minor, version.patch, HasFunction(sdl->SDL_GL_GetDrawableSize));
        }

        if (sdl && HasFunction(sdl->SDL_CreateWindowFunc))
        {
            stage_subscriptions_.emplace_back(
                sdl->SDL_CreateWindowFunc += [this](const char*, int, int, int, int, uint32_t flags, void* window) {
                    PerformanceRuntime& runtime = *this;
                    runtime.monitor_.OnWindowCreated(window, (flags & SDL_WINDOW_OPENGL) != 0);
                }
            );
        }

        if (sdl && HasFunction(sdl->SDL_DestroyWindowFunc))
        {
            stage_subscriptions_.emplace_back(sdl->SDL_DestroyWindowFunc |= [this](void* window, const auto& next) {
                PerformanceRuntime& runtime = *this;
                if (runtime.monitor_.is_main_window(window))
                {
                    if (runtime.device_.is_context_alive())
                    {
                        runtime.renderer_lifecycle_.ReleaseLiveContext();
                    }
                    else
                    {
                        runtime.ReleaseContext();
                    }
                }

                runtime.monitor_.OnWindowDestroyed(window);
                next->Invoke(window);
            });
        }

        if (sdl && HasFunction(sdl->SDL_GL_CreateContext))
        {
            stage_subscriptions_.emplace_back(sdl->SDL_GL_CreateContext += [this](void* window, void* context) {
                PerformanceRuntime& runtime = *this;
                if (context && runtime.monitor_.is_main_window(window))
                {
                    if (runtime.device_.is_context_alive() && !runtime.device_.IsContext(context))
                    {
                        runtime.renderer_lifecycle_.OnContextLost();
                    }
                    else
                    {
                        runtime.ContextChanging();
                    }
                }
            });
        }

        if (sdl && HasFunction(sdl->SDL_GL_DeleteContext))
        {
            stage_subscriptions_.emplace_back(sdl->SDL_GL_DeleteContext |= [this](void* context, const auto& next) {
                PerformanceRuntime& runtime = *this;
                if (runtime.device_.IsContext(context))
                {
                    runtime.renderer_lifecycle_.ReleaseLiveContext();
                }
                else if (runtime.monitor_.is_context(context))
                {
                    runtime.ReleaseContext();
                }

                next->Invoke(context);
            });
        }

        if (sdl && HasFunction(sdl->SDL_GL_SwapWindow))
        {
            swap_subscription_ = sdl->SDL_GL_SwapWindow |= [this](void* window, const auto& next) {
                PerformanceRuntime& runtime = *this;
                PerformanceMonitor& monitor = runtime.monitor_;

                if (monitor.is_main_window(window) && monitor.is_dump_ready())
                {
                    runtime.PublishDumpCapture();
                }

                PerfOutputId_t output_id = monitor.BeginOutput(window);

                if (output_id || ((runtime.network_.is_enabled() || runtime.mapper_.is_enabled() || monitor.get_batch_level()) &&
                                  monitor.is_main_window(window)))
                {
                    runtime.ContextChanging();
                    {
                        LocalCpuScope cpu(&monitor, PerfStage::Monitor);
                        LocalGpuScope gpu(&monitor, PerfStage::Monitor);

                        if (output_id && monitor.is_context(render::context::Current()))
                        {
                            monitor.PollGpu();
                        }

                        runtime.UpdateEngineInfo();
                        monitor.AdvanceSnapshot();
                        int width = 0;
                        int height = 0;
                        bool drawable = runtime.DrawableSize(window, width, height);
                        uint32_t frame_columns =
                            PerformancePanelLayout::GraphColumnCount(runtime.perf_scale_.value, width, height, monitor.get_mode());
                        PerfSnapshot snapshot;
                        monitor.GetPanelSnapshot(
                            snapshot, nullptr, monitor.get_mode() > 0 ? &runtime.frame_graphs_ : nullptr, frame_columns
                        );

                        if (runtime.network_.is_enabled())
                        {
                            uint32_t columns = PerformancePanelLayout::NetworkColumnCount(snapshot.panel_scale, width, height);

                            if (columns)
                            {
                                runtime.network_columns_ = columns;
                            }

                            runtime.UpdateNetwork();
                        }

                        runtime.UpdateMapper();

                        if (drawable && monitor.is_context(render::context::Current()))
                        {
                            if (!runtime.panel_ && (snapshot.mode || runtime.network_.is_enabled() || runtime.mapper_.is_enabled() ||
                                                    monitor.get_batch_level()))
                            {
                                runtime.panel_ = std::make_unique<PerformancePanel>(
#ifdef _WIN32
                                    *runtime.text_,
#endif
                                    runtime.clock_,
                                    runtime.engine_.currenttexture,
                                    runtime.localization_,
                                    runtime.console_text_
                                );
                            }

                            if (runtime.panel_)
                            {
                                runtime.panel_->SetBackgroundAlpha(runtime.perf_background_alpha_.value);
                                runtime.panel_->Draw(
                                    snapshot,
                                    width,
                                    height,
                                    snapshot.mode,
                                    runtime.network_.is_enabled() ? &runtime.network_snapshot_ : nullptr,
                                    runtime.mapper_.is_enabled() ? &runtime.mapper_.get_snapshot() : nullptr,
                                    monitor.get_batch_level(),
                                    monitor.get_batch_snapshot(),
                                    nullptr,
                                    snapshot.mode > 0 ? &runtime.frame_graphs_ : nullptr
                                );
                            }
                        }
                    }

                    if (output_id)
                    {
                        monitor.CloseRenderGpu();
                        monitor.MarkSwapStart();
                    }
                }

                next->Invoke(window);
                monitor.EndOutput(output_id);
            };
        }
    }

    void PerformanceRuntime::ContextCreated()
    {
        if (!device_.IsContextCurrent())
        {
            return;
        }

        nitroapi::SDL2Data* sdl = sdl_;
        PerformanceMonitor& monitor = monitor_;

        if (monitor.is_context(render::context::Current()))
        {
            return;
        }

        gpu_backend_.Probe();
        monitor.SetGraphicsInfo(
            reinterpret_cast<const char*>(glGetString(GL_VENDOR)),
            reinterpret_cast<const char*>(glGetString(GL_RENDERER)),
            reinterpret_cast<const char*>(glGetString(GL_VERSION))
        );
        monitor.SetGpuBackend(&gpu_backend_);
        monitor.OnContextCreated(render::context::Current());
#if USE_PROFILER
        if (gpu_backend_.IsTimestampSupported() && gpu_backend_.counter_bits() > 0)
        {
            trace_backend_.InitializeGpu();
        }
#endif
        if (current_window_available_)
        {
            void* window = sdl->SDL_GL_GetCurrentWindow();

            if (window)
            {
                monitor.SetMainWindow(window);
            }
        }
    }

    void PerformanceRuntime::BindPresentation(vgui2::ILocalize* localization, ConsoleText_t console_text)
    {
        localization_ = localization;
        console_text_ = console_text;

        if (panel_)
        {
            panel_->BindPresentation(localization, console_text);
        }
    }

    void PerformanceRuntime::RegisterControls()
    {
        Cvar_RegisterVariable(&perf_mode_);
        Cvar_RegisterVariable(&perf_gpu_);
        Cvar_RegisterVariable(&perf_scale_);
        Cvar_RegisterVariable(&perf_net_);
        Cvar_RegisterVariable(&perf_map_);
        Cvar_RegisterVariable(&perf_batch_);
        Cvar_RegisterVariable(&perf_background_alpha_);
        Cvar_HookVariable(perf_mode_.name, &mode_hook_);
        Cvar_HookVariable(perf_gpu_.name, &gpu_hook_);
        Cvar_HookVariable(perf_scale_.name, &scale_hook_);
        Cvar_HookVariable(perf_net_.name, &network_hook_);
        Cvar_HookVariable(perf_map_.name, &mapper_hook_);
        Cvar_HookVariable(perf_batch_.name, &batch_hook_);
        OnBatchChanged(&perf_batch_);
        Cmd_AddCommand("perf_dump", DumpCommand);
        Cmd_AddCommand("perf_reset", ResetCommand);
    }

    void PerformanceRuntime::Shutdown()
    {
        network_.SetEnabled(false);
        ++mapper_session_epoch_;
        mapper_.SetLevel(0);
        no_refresh_ = nullptr;
        net_graph_ = nullptr;
        mapper_render_view_ = {};
        mapper_render_view_captured_ = false;

        for (const auto& subscription : stage_subscriptions_)
        {
            subscription->Unsubscribe();
        }

        stage_subscriptions_.clear();

        if (swap_subscription_)
        {
            swap_subscription_->Unsubscribe();
            swap_subscription_.reset();
        }

        AbandonContext();
        subscription_.BeginShutdown();
        monitor_.Shutdown();
#if USE_PROFILER
        if (!monitor_.is_inside_host())
        {
            monitor_.SetTraceBackend(nullptr);
        }
#endif
        legacy_window_size_ = false;
    }

    void PerformanceRuntime::ContextChanging()
    {
        if (!monitor_.is_context(render::context::Current()))
        {
            AbandonMonitorContext();
        }
    }

    void PerformanceRuntime::ReleaseContext()
    {
        ReleaseMonitorContext();
    }

    void PerformanceRuntime::AbandonContext()
    {
        AbandonMonitorContext();
    }

    void PerformanceRuntime::ResetMapperSession()
    {
        ++mapper_session_epoch_;
        mapper_.Reset();
        mapper_render_view_captured_ = false;
    }

    void PerformanceRuntime::MapChanged()
    {
        ResetMapperSession();
        monitor_.OnMapChanged();
    }
} // namespace performance
