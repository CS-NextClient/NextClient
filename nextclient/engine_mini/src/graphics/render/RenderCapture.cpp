#include "engine.h"

#include <filesystem>

#include <glad/glad.h>
#include <ncl_gl/capture.h>
#include "graphics/render/RenderDevice.h"

#include "common/cmd.h"
#include "common/common.h"
#include "common/cvar.h"
#include "console/console.h"
#include "graphics/render/RenderCapture.h"
#include "graphics/render/RenderConfig.h"
#include "graphics/render/RenderFileName.h"
#include "graphics/render/ServerMaterials.h"

namespace
{
    enum class CaptureState
    {
        Idle,
        PendingOff,
        PendingOn
    };

    CaptureState g_State = CaptureState::Idle;
    char g_Name[64] = "studio_ab";
    float g_SavedStudioShader;
    float g_SavedStudioMaterials;

    bool CaptureScene(const char* suffix, std::filesystem::path& path_out)
    {
        ncl_gl::FrameImage image = ncl_gl::CaptureFramebuffer();

        if (!image.ok())
        {
            Con_Printf("r_studio_ab: empty viewport\n");
            return false;
        }

        std::filesystem::path path =
            std::filesystem::path(gEngfuncs.pfnGetGameDirectory()) / (std::string(g_Name) + suffix + ".tga");

        if (!ncl_gl::WriteTga(path, image))
        {
            Con_Printf("r_studio_ab: cannot write %s\n", path.string().c_str());
            return false;
        }

        path_out = path;

        return true;
    }

    void SetStudioShader(const char* value)
    {
        if (render::g_RenderCfg.studio_shader != nullptr)
        {
            Cvar_DirectSet(render::g_RenderCfg.studio_shader, const_cast<char*>(value));
        }
    }

    void SetStudioMaterials(const char* value)
    {
        if (render::g_RenderCfg.studio_materials != nullptr)
        {
            Cvar_DirectSet(render::g_RenderCfg.studio_materials, const_cast<char*>(value));
        }
    }

    void StudioAb_f()
    {
        if (!render::Device_Available())
        {
            Con_Printf("r_studio_ab: shader pipeline unavailable, nothing to compare\n");
            return;
        }

        if (render::g_RenderCfg.studio_shader == nullptr)
        {
            Con_Printf("r_studio_ab: r_studio_shader is not registered\n");
            return;
        }

        // The pair is completed from the end of a world frame, which no map means never.
        // Forcing the cvars now would leave r_studio_shader archived as zero if the player
        // quits from the menu, and the shader path off on the next launch.
        const char* level = gEngfuncs.pfnGetLevelName();

        if (level == nullptr || *level == '\0')
        {
            Con_Printf("r_studio_ab: no map loaded, there is no frame to capture\n");
            return;
        }

        // A second call inside an unfinished window would save the zeros this one forced as
        // if they were the player's, and restore them for good.
        if (g_State != CaptureState::Idle)
        {
            Con_Printf("r_studio_ab: a capture is already in flight\n");
            return;
        }

        const char* name = Cmd_Argc() > 1 ? Cmd_Argv(1) : "studio_ab";

        if (!render::FileName_IsPlain("r_studio_ab", name))
        {
            return;
        }

        V_strcpy_safe(g_Name, name);

        g_SavedStudioShader = render::g_RenderCfg.studio_shader->value;

        // Suppressed for both frames. A material is by definition something the engine's
        // own path cannot draw, so an entity carrying one differs for a reason that says
        // nothing about whether the shader path reproduces the engine, which is the only
        // question this command asks. Guarding one source would not do: a model document
        // sits next to the model itself and is installed content like any other.
        g_SavedStudioMaterials = render::g_RenderCfg.studio_materials != nullptr
            ? render::g_RenderCfg.studio_materials->value
            : 0.0f;
        SetStudioMaterials("0");

        g_State = CaptureState::PendingOff;
        SetStudioShader("0");

        Con_Printf("r_studio_ab: capturing two frames with materials suppressed.\n");
        Con_Printf("             Pause first, or the scene moves between them.\n");
    }
}

void render::Capture_RegisterCommands()
{
    Cmd_AddCommand("r_studio_ab", StudioAb_f);
}

void render::Capture_OnSceneEnd()
{
    if (g_State == CaptureState::Idle)
    {
        return;
    }

    std::filesystem::path path;

    if (g_State == CaptureState::PendingOff)
    {
        g_State = CaptureState::Idle;

        if (CaptureScene("_off", path))
        {
            g_State = CaptureState::PendingOn;
            SetStudioShader("1");
        }
        else
        {
            SetStudioShader(g_SavedStudioShader != 0.0f ? "1" : "0");
            SetStudioMaterials(g_SavedStudioMaterials != 0.0f ? "1" : "0");
        }

        return;
    }

    g_State = CaptureState::Idle;
    bool ok = CaptureScene("_on", path);
    SetStudioShader(g_SavedStudioShader != 0.0f ? "1" : "0");
    SetStudioMaterials(g_SavedStudioMaterials != 0.0f ? "1" : "0");

    if (!ok)
    {
        return;
    }

    std::filesystem::path directory = path.parent_path();

    Con_Printf("r_studio_ab: wrote %s_off.tga and %s_on.tga to %s\n", g_Name, g_Name, directory.string().c_str());
    // --clusters because the fraction on its own is not a verdict here: a real defect
    // came to 0.178% against a 0.200% limit once, and only its shape gave it away.
    Con_Printf(
        "  python tools/radar_tests/imgdiff.py %s_off.tga %s_on.tga --clusters --out %s_diff.png\n",
        g_Name,
        g_Name,
        g_Name
    );
}
