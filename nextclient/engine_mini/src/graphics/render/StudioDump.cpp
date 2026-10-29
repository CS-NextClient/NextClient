#include "graphics/render/StudioDiagnostics.h"
#include "graphics/render/RendererState.h"

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <ostream>
#include <string>
#include <vector>

#ifdef _WIN32
#include <Windows.h>
#endif
#include <ncl_math/color.h>

#include "engine.h"
#include "common/cmd.h"
#include "common/sys_dll.h"
#include "common/common.h"
#include "console/console.h"
#include "graphics/render/RenderConfig.h"
#include "graphics/render/render_file_name.h"

namespace render
{
    namespace
    {
        // A busy frame of the test level reaches about 1200 submissions. What the cap drops is
        // written down: a truncated frame that says nothing about it reads as a frame that had
        // no more in it.
        constexpr int kMaxRecords = 4096;

        // Distinct rejection reasons the summary lists, which is far more than a frame has.
        constexpr int kMaxReasons = 32;

        float CvarValue(const goldsrc::cvar_t* cvar)
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

        void StudioElights_f()
        {
            const double seconds = Cmd_Argc() > 1 ? atof(Cmd_Argv(1)) : 10.0;
            GetRendererState().diagnostics.Watch(seconds, Sys_FloatTime());
            Con_Printf("r_studio_elights: watching cl_elights for %.0f seconds\n", seconds);
        }

        void StudioDump_f()
        {
            const char* name = Cmd_Argc() > 1 ? Cmd_Argv(1) : "studio_dump";

            if (!FileName_IsPlain("r_studio_dump", name))
            {
                return;
            }

            const bool await_light = Cmd_Argc() > 2 && V_stricmp(Cmd_Argv(2), "lit") == 0;
            GetRendererState().diagnostics.Arm(name, await_light);
            Con_Printf(
                "r_studio_dump: recording the %s into %s.txt\n", await_light ? "next frame with an entity light" : "next frame", name
            );
        }
    } // namespace

    void StudioDiagnostics::FormatRecord(int index, const Record& record, char* out, size_t size)
    {
        const StudioSubmission& fields = record.fields;

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

    int StudioDiagnostics::CollectReasons(const char** reasons, int* counts, int capacity)
    {
        int distinct = 0;

        for (int i = 0; i < count_; i++)
        {
            if (records_[i].drawn)
            {
                continue;
            }

            int found = -1;

            for (int j = 0; j < distinct; j++)
            {
                if (std::strcmp(reasons[j], records_[i].reason) == 0)
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
                reasons[distinct] = records_[i].reason;
                counts[distinct] = 1;
                distinct++;
            }
        }

        return distinct;
    }

    int StudioDiagnostics::DrawnCount()
    {
        int drawn = 0;

        for (int i = 0; i < count_; i++)
        {
            if (records_[i].drawn)
            {
                drawn++;
            }
        }

        return drawn;
    }

    void StudioDiagnostics::WriteEntityLights(std::ostream& file, char* line, size_t size)
    {
        if (engine_.lights == nullptr)
        {
            file << "# cl_elights: not reachable\n";
            return;
        }

#ifdef _WIN32
        const uint8_t* module = reinterpret_cast<const uint8_t*>(GetModuleHandleA("hw.dll"));
        ptrdiff_t offset = module != nullptr ? reinterpret_cast<const uint8_t*>(engine_.lights) - module : 0;

        V_snprintf(line, size, "# cl_elights: %p, hw.dll+0x%x", static_cast<const void*>(engine_.lights), static_cast<unsigned>(offset));
#else
        V_snprintf(line, size, "# cl_elights: %p", static_cast<const void*>(engine_.lights));
#endif
        file << line << "\n";

        int alive = 0;

        for (int i = 0; i < engine_.light_count; i++)
        {
            const goldsrc::dlight_t& light = engine_.lights[i];

            if (!(light.die > engine_.state->time && light.radius > 0.0f))
            {
                continue;
            }

            alive++;

            V_snprintf(
                line,
                size,
                "#   %2d origin=%.0f %.0f %.0f radius=%.0f color=%d,%d,%d key=0x%x dies in %.2f",
                i,
                light.origin[0],
                light.origin[1],
                light.origin[2],
                light.radius,
                light.color.r,
                light.color.g,
                light.color.b,
                static_cast<unsigned>(light.key),
                light.die - static_cast<float>(engine_.state->time)
            );
            file << line << "\n";
        }

        V_snprintf(line, size, "#   %d of %d alive", alive, engine_.light_count);
        file << line << "\n";
    }

    void StudioDiagnostics::WriteFile(const char* directory)
    {
        std::filesystem::path path = std::filesystem::path(directory) / (std::string(name_) + ".txt");

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

        V_snprintf(line, sizeof(line), "# %d submissions, %d by the shader path, %d to the engine", count_, drawn, count_ - drawn);
        file << line << "\n";

        V_snprintf(
            line,
            sizeof(line),
            "# r_studio_shader %g, r_studio_materials %g",
            CvarValue(engine_.config->studio_shader),
            CvarValue(engine_.config->studio_materials)
        );
        file << line << "\n";

        WriteEntityLights(file, line, sizeof(line));

        for (int i = 0; i < distinct; i++)
        {
            V_snprintf(line, sizeof(line), "#   %d to the engine: %s", counts[i], reasons[i]);
            file << line << "\n";
        }

        if (dropped_ != 0)
        {
            V_snprintf(line, sizeof(line), "#   %d further submissions were dropped at the record cap", dropped_);
            file << line << "\n";
        }

        file << "# one line per StudioDrawPoints call, in submission order\n";

        for (int i = 0; i < count_; i++)
        {
            FormatRecord(i, records_[i], line, sizeof(line));
            file << line << "\n";
        }

        Con_Printf("r_studio_dump: %d submissions, %d by the shader path\n", count_, drawn);

        for (int i = 0; i < distinct; i++)
        {
            Con_Printf("  %d to the engine: %s\n", counts[i], reasons[i]);
        }

        if (dropped_ != 0)
        {
            Con_Printf("  %d dropped at the record cap\n", dropped_);
        }

        Con_Printf("  wrote %s\n", path.string().c_str());
    }

    void StudioDiagnostics::SampleEntityLights(double time)
    {
        if (watch_.until == 0.0)
        {
            return;
        }

        if (time >= watch_.until)
        {
            watch_.until = 0.0;

            Con_Printf(
                "r_studio_elights: %d frames, %d of them with a light, at most %d at once\n",
                watch_.frames,
                watch_.frames_with_light,
                watch_.most_at_once
            );

            if (watch_.seen)
            {
                Con_Printf(
                    "  last: origin %.0f %.0f %.0f  radius %.0f  color %d,%d,%d  key 0x%x\n",
                    watch_.origin.x,
                    watch_.origin.y,
                    watch_.origin.z,
                    watch_.radius,
                    static_cast<int>(watch_.color.r),
                    static_cast<int>(watch_.color.g),
                    static_cast<int>(watch_.color.b),
                    watch_.key
                );
            }
            else
            {
                Con_Printf("  not one light in the array for the whole watch\n");
            }

            return;
        }

        watch_.frames++;

        if (engine_.lights == nullptr)
        {
            return;
        }

        int alive = 0;

        for (int i = 0; i < engine_.light_count; i++)
        {
            const goldsrc::dlight_t& light = engine_.lights[i];

            if (!(light.die > engine_.state->time && light.radius > 0.0f))
            {
                continue;
            }

            alive++;

            watch_.seen = true;
            watch_.radius = light.radius;
            watch_.key = static_cast<unsigned int>(light.key);
            watch_.color = ncl_math::Color(light.color.r, light.color.g, light.color.b);
            watch_.origin = ncl_math::Vector3::Ref(light.origin);
        }

        if (alive != 0)
        {
            watch_.frames_with_light++;
        }

        watch_.most_at_once = std::max(watch_.most_at_once, alive);
    }

    int StudioDiagnostics::AliveEntityLights()
    {
        if (engine_.lights == nullptr)
        {
            return 0;
        }

        int alive = 0;

        for (int i = 0; i < engine_.light_count; i++)
        {
            if ((engine_.lights[i].die > engine_.state->time && engine_.lights[i].radius > 0.0f))
            {
                alive++;
            }
        }

        return alive;
    }

    void StudioDiagnostics::Note(const StudioSubmission& submission, const StudioOutcome& outcome)
    {
        if (!armed_)
        {
            return;
        }

        if (count_ == kMaxRecords)
        {
            dropped_++;
            return;
        }

        Record& record = records_[count_++];

        record.fields = submission;
        record.drawn = outcome.drawn;

        // Copied rather than referenced: the model name lives in the engine's model cache and
        // the rejection detail in a single buffer the next submission overwrites.
        V_strcpy_safe(record.model, outcome.model != nullptr ? outcome.model : "");
        V_strcpy_safe(record.header, outcome.header != nullptr ? outcome.header : "");
        V_strcpy_safe(record.reason, outcome.reason != nullptr ? outcome.reason : "");
    }

    void StudioDiagnostics::OnSceneEnd(const char* directory, double time)
    {
        SampleEntityLights(time);

        if (!armed_)
        {
            return;
        }

        if (await_light_ && AliveEntityLights() == 0)
        {
            // This frame is not the one being asked about; drop what it recorded and stay
            // armed for the next.
            count_ = 0;
            dropped_ = 0;
            return;
        }

        armed_ = false;

        WriteFile(directory);

        records_.clear();
        records_.shrink_to_fit();
    }

    void StudioDiagnostics::Arm(const char* name, bool await_light)
    {
        V_strcpy_safe(name_, name);
        records_.resize(kMaxRecords);
        count_ = 0;
        dropped_ = 0;
        armed_ = true;
        await_light_ = await_light;
    }

    void StudioDiagnostics::Watch(double seconds, double time)
    {
        watch_ = ElightWatch{};
        watch_.until = time + seconds;
    }

    void StudioDiagnostics::Clear()
    {
        armed_ = false;
        await_light_ = false;
        count_ = 0;
        dropped_ = 0;
        watch_ = ElightWatch{};
        std::vector<Record>().swap(records_);
    }

    void Dump_OnSceneEnd()
    {
        RendererState& state = GetRendererState();
        if (state.diagnostics.needs_scene_end())
        {
            Studio_BindEngine();
            state.diagnostics.OnSceneEnd(gEngfuncs.pfnGetGameDirectory(), Sys_FloatTime());
        }
    }

    void Dump_RegisterCommands()
    {
        Cmd_AddCommand("r_studio_dump", StudioDump_f);
        Cmd_AddCommand("r_studio_elights", StudioElights_f);
    }
} // namespace render
