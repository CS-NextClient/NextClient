#include "RadarShot.h"
#include "RadarDraw.h"
#include "RadarUtil.h"
#include "../../main.h"

#include <glad/glad.h>
#include <ncl_gl/capture.h>
#include <strtools.h>
#include <cstring>
#include <filesystem>

namespace
{
    bool g_Pending;
    char g_File[260] = "radar_shot.tga";

    void RadarShotCmd()
    {
        const char* arg = gEngfuncs.Cmd_Argc() > 1 ? gEngfuncs.Cmd_Argv(1) : nullptr;
        RadarShot::Request(arg && arg[0] ? arg : "radar_shot.tga");
    }

} // namespace

namespace RadarShot
{
    void Init()
    {
        gEngfuncs.pfnAddCommand(const_cast<char*>("radar_shot"), RadarShotCmd);
    }

    void Request(const char* file)
    {
        V_strcpy_safe(g_File, file);
        g_Pending = true;
        gEngfuncs.Con_Printf("radar_shot: capturing the next frame to %s\n", g_File);
    }

    void CaptureIfPending()
    {
        if (!g_Pending)
        {
            return;
        }
        g_Pending = false;

        if (!RadarDraw_PrimsAvailable())
        {
            gEngfuncs.Con_Printf("radar_shot: GL pipeline unavailable\n");
            return;
        }

        ncl_gl::FrameImage image = ncl_gl::CaptureFramebuffer();
        if (!image.ok())
        {
            gEngfuncs.Con_Printf("radar_shot: empty viewport\n");
            return;
        }

        std::filesystem::path path = RadarUtil_AnchorPath(gEngfuncs.pfnGetGameDirectory(), g_File);
        if (!ncl_gl::WriteTga(path, image))
        {
            gEngfuncs.Con_Printf("radar_shot: cannot write %s\n", path.string().c_str());
            return;
        }
        gEngfuncs.Con_Printf("radar_shot: wrote %dx%d to %s\n", image.width, image.height, path.string().c_str());
    }
} // namespace RadarShot
