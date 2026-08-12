#include "engine.h"

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <ostream>
#include <string>
#include <vector>

#include <Windows.h>

#include <ncl_math/color.h>

#include "common/cmd.h"
#include "common/sys_dll.h"
#include "common/common.h"
#include "console/console.h"
#include "graphics/render/RenderConfig.h"
#include "graphics/render/RenderFileName.h"
#include "graphics/render/StudioDump.h"

namespace
{
    // A busy frame of the test level reaches about 1200 submissions. What the cap drops is
    // written down: a truncated frame that says nothing about it reads as a frame that had
    // no more in it.
    constexpr int kMaxRecords = 4096;

    // Distinct rejection reasons the summary lists, which is far more than a frame has.
    constexpr int kMaxReasons = 32;

    struct Record
    {
        render::StudioSubmission fields;
        char model[64];
        char header[64];

        // As long as the buffer the submodel mismatch formats its offsets into, since
        // that is the one rejection whose text is the diagnostic rather than a label.
        char reason[256];
        bool drawn;
    };

    bool g_Armed;

    // Waiting for a frame with an entity light in it. A muzzle flash is in about a third
    // of the frames of sustained fire, so "the next frame" mostly records the case that
    // was not being asked about.
    bool g_AwaitLight;
    char g_Name[64] = "studio_dump";

    // Held only while a dump is armed: the records are a megabyte and a half, and they are
    // wanted for one frame of a session that runs for hours without ever asking for them.
    std::vector<Record> g_Records;
    int g_Count;
    int g_Dropped;

    float CvarValue(const cvar_t* cvar)
    {
        return cvar != nullptr ? cvar->value : 0.0f;
    }

    void FormatShadeModes(uint32_t modes, char* out, size_t size)
    {
        size_t used = 0;
        out[0] = '\0';

        // Every mode ShadeModeFor returns is a single digit, so the list needs no
        // separator handling beyond the comma.
        for (int mode = 0; mode < 10; mode++)
        {
            if ((modes & (1u << mode)) == 0 || used + 3 >= size)
            {
                continue;
            }

            if (used != 0)
            {
                out[used++] = ',';
            }

            out[used++] = static_cast<char>('0' + mode);
            out[used] = '\0';
        }

        if (used == 0 && size >= 2)
        {
            out[0] = '-';
            out[1] = '\0';
        }
    }

    void FormatRecord(int index, const Record& record, char* out, size_t size)
    {
        const render::StudioSubmission& fields = record.fields;

        char shade[32];
        FormatShadeModes(fields.shade_modes, shade, sizeof(shade));

        int written = V_snprintf(
            out,
            size,
            "%4d ent=%d%s model=%s hdr=%s sub=%u forced=0x%04x fx=%d rm=%d>%d blend=%.2f",
            index,
            fields.entity_index,
            fields.view_entity ? "*" : "",
            record.model,
            record.header,
            fields.submodel_offset,
            static_cast<unsigned>(fields.forced_face_flags),
            fields.renderfx,
            fields.rendermode,
            fields.effective_rendermode,
            fields.blend
        );

        if (written < 0 || written >= static_cast<int>(size))
        {
            return;
        }

        if (fields.glow_shell)
        {
            written += V_snprintf(
                out + written,
                size - written,
                " shell=%d,%d,%d glow=%.2f",
                static_cast<int>(fields.shell_color.x * 255.0f + 0.5f),
                static_cast<int>(fields.shell_color.y * 255.0f + 0.5f),
                static_cast<int>(fields.shell_color.z * 255.0f + 0.5f),
                fields.glow_scale
            );

            if (written < 0 || written >= static_cast<int>(size))
            {
                return;
            }
        }

        written += V_snprintf(
            out + written,
            size - written,
            " meshes=%d shade=%s mat=%d srv=%d elight=%d",
            fields.meshes,
            shade,
            fields.materials,
            fields.server_bound,
            fields.entity_lights
        );

        if (written < 0 || written >= static_cast<int>(size))
        {
            return;
        }

        if (record.drawn)
        {
            V_snprintf(out + written, size - written, " -> shader");
            return;
        }

        V_snprintf(out + written, size - written, " -> engine: %s", record.reason);
    }

    // Distinct rejection reasons with their counts, which is the part worth reading first:
    // a frame where everything went to the engine for one reason and a frame where a
    // single body part did are the same line count and completely different findings.
    int CollectReasons(const char** reasons, int* counts, int capacity)
    {
        int distinct = 0;

        for (int i = 0; i < g_Count; i++)
        {
            if (g_Records[i].drawn)
            {
                continue;
            }

            int found = -1;
            for (int j = 0; j < distinct; j++)
            {
                if (std::strcmp(reasons[j], g_Records[i].reason) == 0)
                {
                    found = j;
                    break;
                }
            }

            if (found >= 0)
            {
                counts[found]++;
            }
            else if (distinct < capacity)
            {
                reasons[distinct] = g_Records[i].reason;
                counts[distinct] = 1;
                distinct++;
            }
        }

        return distinct;
    }

    int DrawnCount()
    {
        int drawn = 0;

        for (int i = 0; i < g_Count; i++)
        {
            if (g_Records[i].drawn)
            {
                drawn++;
            }
        }

        return drawn;
    }

    // The state of the array the entity lights are picked from, which is the only thing
    // that tells "the address is wrong" apart from "nothing was lit in this frame".
    void WriteEntityLights(std::ostream& file, char* line, size_t size)
    {
        if (p_cl_elights == nullptr)
        {
            file << "# cl_elights: not reachable\n";
            return;
        }

        const uint8_t* module = reinterpret_cast<const uint8_t*>(GetModuleHandleA("hw.dll"));
        ptrdiff_t offset = module != nullptr
            ? reinterpret_cast<const uint8_t*>(p_cl_elights) - module
            : 0;

        V_snprintf(line, size, "# cl_elights: %p, hw.dll+0x%x", static_cast<const void*>(p_cl_elights), static_cast<unsigned>(offset));
        file << line << "\n";

        int alive = 0;

        for (int i = 0; i < kMaxElights; i++)
        {
            const dlight_t& light = (*p_cl_elights)[i];

            if (!Elight_Alive(light))
            {
                continue;
            }

            alive++;

            V_snprintf(
                line,
                size,
                "#   %2d origin=%.0f %.0f %.0f radius=%.0f colour=%d,%d,%d key=0x%x dies in %.2f",
                i,
                light.origin[0],
                light.origin[1],
                light.origin[2],
                light.radius,
                light.color.r,
                light.color.g,
                light.color.b,
                static_cast<unsigned>(light.key),
                light.die - static_cast<float>(cl->time)
            );
            file << line << "\n";
        }

        V_snprintf(line, size, "#   %d of %d alive", alive, kMaxElights);
        file << line << "\n";
    }

    void WriteFile()
    {
        std::filesystem::path path =
            std::filesystem::path(gEngfuncs.pfnGetGameDirectory()) / (std::string(g_Name) + ".txt");

        std::ofstream file(path, std::ios::trunc);

        if (!file)
        {
            Con_Printf("r_studio_dump: cannot write %s\n", path.string().c_str());
            return;
        }

        const char* reasons[kMaxReasons];
        int counts[kMaxReasons];
        int distinct = CollectReasons(reasons, counts, kMaxReasons);

        int drawn = DrawnCount();

        char line[768];

        V_snprintf(
            line,
            sizeof(line),
            "# %d submissions, %d by the shader path, %d to the engine",
            g_Count,
            drawn,
            g_Count - drawn
        );
        file << line << "\n";

        V_snprintf(
            line,
            sizeof(line),
            "# r_studio_shader %g, r_studio_materials %g",
            CvarValue(render::g_RenderCfg.studio_shader),
            CvarValue(render::g_RenderCfg.studio_materials)
        );
        file << line << "\n";

        WriteEntityLights(file, line, sizeof(line));

        for (int i = 0; i < distinct; i++)
        {
            V_snprintf(line, sizeof(line), "#   %d to the engine: %s", counts[i], reasons[i]);
            file << line << "\n";
        }

        if (g_Dropped != 0)
        {
            V_snprintf(line, sizeof(line), "#   %d further submissions were dropped at the record cap", g_Dropped);
            file << line << "\n";
        }

        file << "# one line per StudioDrawPoints call, in submission order\n";

        for (int i = 0; i < g_Count; i++)
        {
            FormatRecord(i, g_Records[i], line, sizeof(line));
            file << line << "\n";
        }

        Con_Printf("r_studio_dump: %d submissions, %d by the shader path\n", g_Count, drawn);

        for (int i = 0; i < distinct; i++)
        {
            Con_Printf("  %d to the engine: %s\n", counts[i], reasons[i]);
        }

        if (g_Dropped != 0)
        {
            Con_Printf("  %d dropped at the record cap\n", g_Dropped);
        }

        Con_Printf("  wrote %s\n", path.string().c_str());
    }

    // A muzzle flash lives about 0.05 s against a fire cycle of 0.1, so a single frame
    // catches it about half the time and a frame that misses says the same thing as an
    // array that never fills. Watching over seconds separates the two.
    struct ElightWatch
    {
        double until;
        int frames;
        int frames_with_light;
        int most_at_once;
        bool seen;
        ncl_math::Vector3 origin;
        float radius;
        ncl_math::Color color;
        unsigned int key;
    };

    ElightWatch g_Watch;

    void SampleEntityLights()
    {
        if (g_Watch.until == 0.0)
        {
            return;
        }

        if (Sys_FloatTime() >= g_Watch.until)
        {
            g_Watch.until = 0.0;

            Con_Printf(
                "r_studio_elights: %d frames, %d of them with a light, at most %d at once\n",
                g_Watch.frames,
                g_Watch.frames_with_light,
                g_Watch.most_at_once
            );

            if (g_Watch.seen)
            {
                Con_Printf(
                    "  last: origin %.0f %.0f %.0f  radius %.0f  colour %d,%d,%d  key 0x%x\n",
                    g_Watch.origin.x,
                    g_Watch.origin.y,
                    g_Watch.origin.z,
                    g_Watch.radius,
                    static_cast<int>(g_Watch.color.r),
                    static_cast<int>(g_Watch.color.g),
                    static_cast<int>(g_Watch.color.b),
                    g_Watch.key
                );
            }
            else
            {
                Con_Printf("  not one light in the array for the whole watch\n");
            }

            return;
        }

        g_Watch.frames++;

        if (p_cl_elights == nullptr)
        {
            return;
        }

        int alive = 0;

        for (int i = 0; i < kMaxElights; i++)
        {
            const dlight_t& light = (*p_cl_elights)[i];

            if (!Elight_Alive(light))
            {
                continue;
            }

            alive++;

            g_Watch.seen = true;
            g_Watch.radius = light.radius;
            g_Watch.key = static_cast<unsigned int>(light.key);
            g_Watch.color = ncl_math::Color(light.color.r, light.color.g, light.color.b);
            g_Watch.origin = ncl_math::Vector3::Ref(light.origin);
        }

        if (alive != 0)
        {
            g_Watch.frames_with_light++;
        }

        g_Watch.most_at_once = std::max(g_Watch.most_at_once, alive);
    }

    void StudioElights_f()
    {
        double seconds = Cmd_Argc() > 1 ? atof(Cmd_Argv(1)) : 10.0;

        g_Watch = ElightWatch{};
        g_Watch.until = Sys_FloatTime() + seconds;

        Con_Printf("r_studio_elights: watching cl_elights for %.0f seconds\n", seconds);
    }

    int AliveEntityLights()
    {
        if (p_cl_elights == nullptr)
        {
            return 0;
        }

        int alive = 0;

        for (int i = 0; i < kMaxElights; i++)
        {
            if (Elight_Alive((*p_cl_elights)[i]))
            {
                alive++;
            }
        }

        return alive;
    }

    void StudioDump_f()
    {
        const char* name = Cmd_Argc() > 1 ? Cmd_Argv(1) : "studio_dump";

        if (!render::FileName_IsPlain("r_studio_dump", name))
        {
            return;
        }

        V_strcpy_safe(g_Name, name);

        g_Records.resize(kMaxRecords);
        g_Count = 0;
        g_Dropped = 0;
        g_Armed = true;
        g_AwaitLight = Cmd_Argc() > 2 && V_stricmp(Cmd_Argv(2), "lit") == 0;

        Con_Printf(
            "r_studio_dump: recording the %s into %s.txt\n",
            g_AwaitLight ? "next frame with an entity light" : "next frame",
            g_Name
        );
    }
}

bool render::Dump_Armed()
{
    return g_Armed;
}

void render::Dump_Note(const StudioSubmission& submission, const StudioOutcome& outcome)
{
    if (!g_Armed)
    {
        return;
    }

    if (g_Count == kMaxRecords)
    {
        g_Dropped++;
        return;
    }

    Record& record = g_Records[g_Count++];

    record.fields = submission;
    record.drawn = outcome.drawn;

    // Copied rather than referenced: the model name lives in the engine's model cache and
    // the rejection detail in a single buffer the next submission overwrites.
    V_strcpy_safe(record.model, outcome.model != nullptr ? outcome.model : "");
    V_strcpy_safe(record.header, outcome.header != nullptr ? outcome.header : "");
    V_strcpy_safe(record.reason, outcome.reason != nullptr ? outcome.reason : "");
}

void render::Dump_OnSceneEnd()
{
    SampleEntityLights();

    if (!g_Armed)
    {
        return;
    }

    if (g_AwaitLight && AliveEntityLights() == 0)
    {
        // This frame is not the one being asked about; drop what it recorded and stay
        // armed for the next.
        g_Count = 0;
        g_Dropped = 0;
        return;
    }

    g_Armed = false;

    WriteFile();

    g_Records.clear();
    g_Records.shrink_to_fit();
}

void render::Dump_RegisterCommands()
{
    Cmd_AddCommand("r_studio_dump", StudioDump_f);
    Cmd_AddCommand("r_studio_elights", StudioElights_f);
}
