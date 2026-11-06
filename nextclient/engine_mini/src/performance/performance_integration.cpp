#include "performance/performance_integration.h"

#include <nitroapi/NitroApiInterface.h>
#include <tier1/interface.h>

#include "engine.h"
#include "common/cvar.h"
#include "graphics/render/RendererState.h"
#include "graphics/render/RenderConfig.h"
#include "performance/PerformanceRuntime.h"
#ifdef _WIN32
#include "graphics/text/text_render.h"
#endif

namespace performance
{
    namespace
    {
        PerformanceRuntime* g_Runtime{};
        PerformanceRuntime* g_ControlSink{};
    } // namespace

    void BindControlSink(PerformanceRuntime& runtime)
    {
        g_ControlSink = &runtime;
    }

    void PerformanceRuntime::BatchChanged(goldsrc::cvar_t* cvar)
    {
        g_ControlSink->OnBatchChanged(cvar);
    }

    void PerformanceRuntime::MapperChanged(goldsrc::cvar_t* cvar)
    {
        g_ControlSink->OnMapperChanged(cvar);
    }

    void PerformanceRuntime::NetworkChanged(goldsrc::cvar_t* cvar)
    {
        g_ControlSink->OnNetworkChanged(cvar);
    }

    void PerformanceRuntime::ModeChanged(goldsrc::cvar_t* cvar)
    {
        g_ControlSink->OnModeChanged(cvar);
    }

    void PerformanceRuntime::GpuChanged(goldsrc::cvar_t* cvar)
    {
        g_ControlSink->OnGpuChanged(cvar);
    }

    void PerformanceRuntime::ScaleChanged(goldsrc::cvar_t* cvar)
    {
        g_ControlSink->OnScaleChanged(cvar);
    }

    void PerformanceRuntime::DumpCommand()
    {
        g_ControlSink->DumpPerformance();
    }

    void PerformanceRuntime::ResetCommand()
    {
        g_ControlSink->ResetPerformance();
    }

    void runtime::Bind(nitroapi::NitroApiInterface* nitro_api)
    {
        CreateInterfaceFn factory = Sys_GetFactory("nitro_api2.dll");
        nitroapi::FrameLifecycleInterface* lifecycle =
            factory ? reinterpret_cast<nitroapi::FrameLifecycleInterface*>(factory(NITRO_FRAME_LIFECYCLE_INTERFACE_VERSION, nullptr))
                    : nullptr;
        nitroapi::EngineData& engine = *nitro_api->GetEngineData();
        if (!engine.Host_Frame.GetHook() || !engine.Host_Frame.GetHook()->GetOriginalFunctionAddress() ||
            !engine.Host_FrameInternal.GetHook() || !engine.Host_FrameInternal.GetHook()->GetOriginalFunctionAddress() ||
            !engine.Host_FilterTime.GetHook() || !engine.Host_FilterTime.GetHook()->GetOriginalFunctionAddress())
        {
            lifecycle = nullptr;
        }
        render::RendererState& renderer = render::GetRendererState();
        static PerformanceRuntime runtime(
            engine,
            *nitro_api->GetClientData(),
            nitro_api->GetSDL2Data(),
            lifecycle,
            GetMonitor(),
            GetMapper(),
            get_clock(),
            renderer.resources.device,
            renderer.lifecycle,
#ifdef _WIN32
            TextRender_Get(),
#else
            nullptr,
#endif
            nitro_api->GetBuildVersion()->build_number
        );
        g_Runtime = &runtime;
        runtime.Bind();
    }

    void runtime::RegisterControls()
    {
        BindControlSink(*g_Runtime);
        g_Runtime->RegisterControls();
    }

    void runtime::PresentationInitialized()
    {
        g_Runtime->BindControls(
            fps_max,
            gl_vsync,
            r_speeds,
            render::g_RenderCfg.studio_shader,
            r_norefresh,
            Cvar_FindVar("net_graph")
        );
        runtime::BindPresentation();
    }

    void runtime::BindPresentation()
    {
        g_Runtime->BindPresentation(g_pLocalize, gEngfuncs.pfnDrawConsoleString);
    }

    void runtime::ContextCreated()
    {
        if (g_Runtime)
        {
            g_Runtime->ContextCreated();
        }
    }

    void runtime::ContextChanging()
    {
        if (g_Runtime)
        {
            g_Runtime->ContextChanging();
        }
    }

    void runtime::ReleaseContext()
    {
        if (g_Runtime)
        {
            g_Runtime->ReleaseContext();
        }
    }

    void runtime::AbandonContext()
    {
        if (g_Runtime)
        {
            g_Runtime->AbandonContext();
        }
    }

    void runtime::MapChanged()
    {
        if (g_Runtime)
        {
            g_Runtime->MapChanged();
        }
    }

    void runtime::Shutdown()
    {
        if (g_Runtime)
        {
            g_Runtime->Shutdown();
        }
    }

    bool presentation::UsesLegacyWindowSize(uint8_t major, uint8_t minor, uint8_t patch, bool drawable_size)
    {
        // SDL 2.0.1 introduced both high-DPI windows and the drawable-size query.
        return !drawable_size && ((major == 1 && minor == 3) || (major == 2 && minor == 0 && patch == 0));
    }
} // namespace performance
