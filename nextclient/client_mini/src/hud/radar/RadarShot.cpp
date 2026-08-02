#include "RadarShot.h"
#include "RadarDraw.h"
#include "RadarUtil.h"
#include "../../main.h"

#include <glad/glad.h>
#include <strtools.h>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <vector>

namespace
{
    bool g_Pending;
    char g_File[260] = "radar_shot.tga";

    void RadarShotCmd()
    {
        const char* arg = gEngfuncs.Cmd_Argc() > 1 ? gEngfuncs.Cmd_Argv(1) : nullptr;
        RadarShot::Request(arg && arg[0] ? arg : "radar_shot.tga");
    }

    bool WriteTga(const std::filesystem::path& path, int w, int h, const unsigned char* bgr)
    {
        std::ofstream f(path, std::ios::binary);
        if (!f)
        {
            return false;
        }
        unsigned char header[18] = {};
        header[2] = 2; // uncompressed true-colour
        header[12] = (unsigned char)(w & 0xFF);
        header[13] = (unsigned char)((w >> 8) & 0xFF);
        header[14] = (unsigned char)(h & 0xFF);
        header[15] = (unsigned char)((h >> 8) & 0xFF);
        header[16] = 24;
        // descriptor 0: bottom-left origin, matching glReadPixels row order
        f.write(reinterpret_cast<const char*>(header), sizeof(header));
        f.write(reinterpret_cast<const char*>(bgr), (std::streamsize)w * h * 3);
        return (bool)f;
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

        GLint vp[4] = {0, 0, 0, 0};
        glGetIntegerv(GL_VIEWPORT, vp);
        int w = vp[2];
        int h = vp[3];
        if (w < 4 || h < 4)
        {
            gEngfuncs.Con_Printf("radar_shot: empty viewport\n");
            return;
        }

        // Read from wherever the scene+HUD is actually drawn (default buffer or the FBO
        // used by fullscreen/-stretchaspect), the same source the blur backdrop captures.
        GLint prev_read_fb = 0, prev_draw_fb = 0;
        glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &prev_read_fb);
        glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &prev_draw_fb);
        glBindFramebuffer(GL_READ_FRAMEBUFFER, prev_draw_fb);
        GLint prev_read_buf = GL_BACK;
        glGetIntegerv(GL_READ_BUFFER, &prev_read_buf);
        glReadBuffer(prev_draw_fb == 0 ? GL_BACK : GL_COLOR_ATTACHMENT0);

        GLint prev_align = 4;
        glGetIntegerv(GL_PACK_ALIGNMENT, &prev_align);
        glPixelStorei(GL_PACK_ALIGNMENT, 1);

        std::vector<unsigned char> pixels((size_t)w * h * 3);
        glReadPixels(vp[0], vp[1], w, h, GL_BGR, GL_UNSIGNED_BYTE, pixels.data());

        glPixelStorei(GL_PACK_ALIGNMENT, prev_align);
        glReadBuffer(prev_read_buf);
        glBindFramebuffer(GL_READ_FRAMEBUFFER, prev_read_fb);

        std::filesystem::path path = RadarUtil_AnchorPath(gEngfuncs.pfnGetGameDirectory(), g_File);
        if (!WriteTga(path, w, h, pixels.data()))
        {
            gEngfuncs.Con_Printf("radar_shot: cannot write %s\n", path.string().c_str());
            return;
        }
        gEngfuncs.Con_Printf("radar_shot: wrote %dx%d to %s\n", w, h, path.string().c_str());
    }
} // namespace RadarShot
