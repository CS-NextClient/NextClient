#include "graphics/render/render_capture.h"

#include <filesystem>
#include <string>

#include <ncl_gl/capture.h>

#include "engine.h"
#include "common/cmd.h"
#include "common/common.h"
#include "common/cvar.h"
#include "console/console.h"
#include "graphics/render/RenderConfig.h"
#include "graphics/render/RenderCaptureSession.h"
#include "graphics/render/RendererState.h"
#include "graphics/render/render_device.h"
#include "graphics/render/render_file_name.h"

namespace render
{
    namespace
    {
        class EngineRenderCaptureBackend final : public RenderCaptureBackend
        {
        public:
            void SetCvar(goldsrc::cvar_t* cvar, const char* value) override
            {
                Cvar_DirectSet(cvar, value);
            }

            bool Capture(const char* name, const char* suffix, std::filesystem::path& path_out) override
            {
                ncl_gl::FrameImage image = ncl_gl::CaptureFramebuffer();
                if (!image.is_valid())
                {
                    Con_Printf("r_studio_ab: empty viewport\n");
                    return false;
                }

                std::filesystem::path path = std::filesystem::path(gEngfuncs.pfnGetGameDirectory()) / (std::string(name) + suffix + ".tga");
                if (!ncl_gl::WriteTga(path, image))
                {
                    Con_Printf("r_studio_ab: cannot write %s\n", path.string().c_str());
                    return false;
                }
                path_out = std::move(path);
                return true;
            }

            void ReportCompleted(const char* name, const std::filesystem::path& path) override
            {
                Con_Printf("r_studio_ab: wrote %s_off.tga and %s_on.tga to %s\n", name, name, path.parent_path().string().c_str());
            }
        };

        void StudioAb_f()
        {
            if (!Device_CheckAvailable())
            {
                Con_Printf("r_studio_ab: shader pipeline unavailable, nothing to compare\n");
                return;
            }

            if (g_RenderCfg.studio_shader == nullptr)
            {
                Con_Printf("r_studio_ab: r_studio_shader is not registered\n");
                return;
            }

            const char* level = gEngfuncs.pfnGetLevelName();

            if (level == nullptr || *level == '\0')
            {
                Con_Printf("r_studio_ab: no map loaded, there is no frame to capture\n");
                return;
            }

            if (GetRendererState().capture.is_pending())
            {
                Con_Printf("r_studio_ab: a capture is already pending\n");
                return;
            }

            const char* name = Cmd_Argc() > 1 ? Cmd_Argv(1) : "studio_ab";

            if (!FileName_IsPlain("r_studio_ab", name))
            {
                return;
            }

            GetRendererState().capture.Begin(name, *g_RenderCfg.studio_shader, g_RenderCfg.studio_materials);

            Con_Printf("r_studio_ab: capturing two frames with materials suppressed.\n");
            Con_Printf("             Pause first, or the scene moves between them.\n");
        }
    } // namespace

    RenderCaptureBackend& GetRenderCaptureBackend()
    {
        static EngineRenderCaptureBackend backend;
        return backend;
    }

    void Capture_RegisterCommands()
    {
        Cmd_AddCommand("r_studio_ab", StudioAb_f);
    }

    void Capture_OnSceneEnd()
    {
        GetRendererState().capture.OnSceneEnd();
    }

    void Capture_Cancel()
    {
        GetRendererState().capture.Cancel();
    }
} // namespace render
