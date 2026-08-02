#include "RadarDump.h"
#include "RadarUtil.h"
#include "../../main.h"

#include <strtools.h>

#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace
{
    bool g_Pending;
    bool g_Capturing;
    char g_File[260] = "radar_dump.json";

    float g_Time, g_BoxX, g_BoxY, g_BoxSize;

    struct Marker
    {
        std::string kind;
        int id;
        float wx, wy, sx, sy;
        int r, g, b, a;
        std::string flags;
    };
    std::vector<Marker> g_Markers;

    void RadarDumpCmd()
    {
        const char* arg = gEngfuncs.Cmd_Argc() > 1 ? gEngfuncs.Cmd_Argv(1) : nullptr;
        RadarDump::Request(arg && arg[0] ? arg : "radar_dump.json");
    }
} // namespace

namespace RadarDump
{
    void Init()
    {
        gEngfuncs.pfnAddCommand(const_cast<char*>("radar_dump"), RadarDumpCmd);
    }

    void Request(const char* file)
    {
        V_strcpy_safe(g_File, file);
        g_Pending = true;
        gEngfuncs.Con_Printf("radar_dump: capturing the next radar frame to %s\n", g_File);
    }

    bool Capturing()
    {
        return g_Capturing;
    }

    void BeginFrame(float time, float box_x, float box_y, float box_size)
    {
        if (!g_Pending)
        {
            return;
        }
        g_Pending = false;
        g_Capturing = true;
        g_Markers.clear();
        g_Time = time;
        g_BoxX = box_x;
        g_BoxY = box_y;
        g_BoxSize = box_size;
    }

    void AddMarker(const char* kind, int id, float wx, float wy, float sx, float sy, int r, int g, int b, int a, const char* flags)
    {
        if (!g_Capturing)
        {
            return;
        }
        g_Markers.push_back({kind, id, wx, wy, sx, sy, r, g, b, a, flags ? flags : ""});
    }

    void EndFrame()
    {
        if (!g_Capturing)
        {
            return;
        }
        g_Capturing = false;

        std::string path_str = RadarUtil_AnchorPath(gEngfuncs.pfnGetGameDirectory(), g_File).string();

        std::ofstream f(path_str, std::ios::binary);
        if (!f)
        {
            gEngfuncs.Con_Printf("radar_dump: cannot write %s\n", path_str.c_str());
            return;
        }

        char buf[512];
        f << "{\n";
        std::snprintf(
            buf,
            sizeof(buf),
            "  \"time\": %.3f,\n  \"box\": {\"x\": %.2f, \"y\": %.2f, \"size\": %.2f},\n",
            g_Time,
            g_BoxX,
            g_BoxY,
            g_BoxSize
        );
        f << buf << "  \"markers\": [\n";
        for (size_t i = 0; i < g_Markers.size(); i++)
        {
            const Marker& m = g_Markers[i];
            std::snprintf(
                buf,
                sizeof(buf),
                "    {\"kind\": \"%s\", \"id\": %d, \"wx\": %.1f, \"wy\": %.1f, \"sx\": %.1f, \"sy\": %.1f, \"r\": %d, \"g\": %d, "
                "\"b\": %d, \"a\": %d, \"flags\": \"%s\"}%s\n",
                m.kind.c_str(),
                m.id,
                m.wx,
                m.wy,
                m.sx,
                m.sy,
                m.r,
                m.g,
                m.b,
                m.a,
                m.flags.c_str(),
                i + 1 < g_Markers.size() ? "," : ""
            );
            f << buf;
        }
        f << "  ]\n}\n";
        gEngfuncs.Con_Printf("radar_dump: wrote %d markers to %s\n", (int)g_Markers.size(), path_str.c_str());
    }
} // namespace RadarDump
