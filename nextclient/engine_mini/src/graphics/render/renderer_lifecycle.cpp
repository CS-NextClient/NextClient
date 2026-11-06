#include "graphics/render/RendererLifecycle.h"

#include "graphics/render/RendererState.h"
#include "graphics/render/render_capture.h"
#include "performance/performance_integration.h"
#ifdef _WIN32
#include "graphics/text/text_render.h"
#include "graphics/vgui2.h"
#endif

namespace render
{
    namespace
    {
        class EngineRendererLifecycleBackend final : public RendererLifecycleBackend
        {
        public:
            void MapChanged() override
            {
                performance::runtime::MapChanged();
            }
            void ReleaseContext() override
            {
                performance::runtime::ReleaseContext();
            }
            void AbandonContext() override
            {
                performance::runtime::AbandonContext();
            }
            void Shutdown() override
            {
                performance::runtime::Shutdown();
            }
            void UninstallTextSurface() override
            {
#ifdef _WIN32
                VGUI2_UninstallTextSurface();
#endif
            }
            void ReleaseText() override
            {
#ifdef _WIN32
                TextRender_ReleaseGl();
#endif
            }
            void ForgetText() override
            {
#ifdef _WIN32
                TextRender_ForgetGl();
#endif
            }
        };
    } // namespace

    RendererLifecycleBackend& GetRendererLifecycleBackend()
    {
        static EngineRendererLifecycleBackend backend;
        return backend;
    }

    RendererLifecycle& GetLifecycle()
    {
        return GetRendererState().lifecycle;
    }
} // namespace render
