#include "graphics/render/StudioPhaseCapture.h"

#include <array>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <limits>
#include <string>
#include <system_error>

#include <glad/glad.h>

#ifdef _WIN32
#include <Windows.h>
#endif

#include "common/cmd.h"
#include "common/common.h"
#include "console/console.h"
#include "engine.h"
#include "graphics/gl_local.h"
#include "graphics/render/RendererState.h"
#include "graphics/render/render_file_name.h"

namespace render
{
    namespace
    {
        uint64_t Now()
        {
            return static_cast<uint64_t>(
                std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch()).count()
            );
        }

        char g_CaptureName[64] = "studio_phases";

        StudioPhaseCapture& get_recorder()
        {
            return GetRendererState().diagnostics.get_phase_capture();
        }

        void Start()
        {
            const char* name = Cmd_Argc() > 1 ? Cmd_Argv(1) : "studio_phases";

            if (!FileName_IsPlain("studio diagnostics", name))
            {
                return;
            }

            const int first = Cmd_Argc() > 2 ? std::atoi(Cmd_Argv(2)) : 0;
            const int last = Cmd_Argc() > 3 ? std::atoi(Cmd_Argv(3)) : std::numeric_limits<int>::max();

            if (!get_recorder().Start({}, first, last))
            {
                Con_Printf("Studio phases: start failed, stop and export the current recording first.\n");
                return;
            }

            std::strncpy(g_CaptureName, name, sizeof(g_CaptureName) - 1);
            Con_Printf("Studio phases: armed phase recording %s, demo frames %d-%d.\n", name, first, last);
        }

        void Stop()
        {
            get_recorder().Stop();
            Con_Printf("Studio phases: stopped; export is ready.\n");
        }

        void Status()
        {
            Con_Printf(
                "Studio phases: active=%d views=%u overflow=%d\n",
                get_recorder().is_active(),
                static_cast<unsigned int>(get_recorder().get_view_count()),
                get_recorder().has_overflow()
            );
        }

        void Export()
        {
            StudioPhaseCapture& recorder = get_recorder();

            if (recorder.is_active())
            {
                Con_Printf("Studio phases: stop before exporting outside the measured loop.\n");
                return;
            }

            if (!recorder.has_recording())
            {
                Con_Printf("Studio phases: no stopped recording for this export command.\n");
                return;
            }

            const char* name = Cmd_Argc() > 1 ? Cmd_Argv(1) : g_CaptureName;

            if (!FileName_IsPlain("studio diagnostics", name))
            {
                return;
            }

            const std::filesystem::path directory = std::filesystem::path(gEngfuncs.pfnGetGameDirectory()) / "studio_diagnostics";
            std::error_code error;

            std::filesystem::create_directories(directory, error);
            if (error)
            {
                Con_Printf("Studio phases: cannot create export directory: %s\n", error.message().c_str());
                return;
            }

            const std::filesystem::path phase_path = directory / (std::string(name) + ".phases.csv");
            const std::filesystem::path metadata_path = directory / (std::string(name) + ".phases.metadata.json");
            const std::array<std::filesystem::path, 2> paths = {
                phase_path,
                metadata_path
            };
            std::ofstream phases(phase_path.string() + ".tmp");
            std::ofstream metadata(metadata_path.string() + ".tmp");

            const bool written = recorder.Export(phases, metadata);
            phases.close();
            metadata.close();

            if (!written || !phases.good() || !metadata.good())
            {
                Con_Printf("Studio phases: export failed; recording retained.\n");
                return;
            }

            std::filesystem::remove(metadata_path, error);
            if (error)
            {
                Con_Printf("Studio phases: cannot invalidate the previous export marker; recording retained.\n");
                return;
            }

            for (size_t i = 0; i < paths.size(); ++i)
            {
                const std::filesystem::path temporary = paths[i].string() + ".tmp";

#ifdef _WIN32
                const bool replaced =
                    MoveFileExW(
                        temporary.c_str(),
                        paths[i].c_str(),
                        MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH
                    ) != 0;
#else
                std::filesystem::rename(temporary, paths[i], error);
                const bool replaced = !error;
#endif

                if (!replaced)
                {
                    Con_Printf("Studio phases: cannot publish export file; recording retained.\n");
                    return;
                }
            }

            Con_Printf("Studio phases: exported %s to %s.\n", name, directory.string().c_str());
            recorder.Reset();
        }
    } // namespace

    void StudioPhase_BeginView()
    {
        StudioPhaseCapture& recorder = get_recorder();

        if (!recorder.is_active() || !cls || !cls->demoplayback)
        {
            return;
        }

        StudioPhaseFrame frame;
        frame.host_frame = eng()->host_framecount ? *eng()->host_framecount : 0;
        frame.render_frame = eng()->r_framecount ? *eng()->r_framecount : 0;
        frame.demo_frame = cls->demoframecount;
        frame.client_time = cl ? cl->time : 0;
        frame.old_client_time = cl ? cl->oldtime : 0;
        frame.demo = cls->demofilename;
        recorder.BeginView(frame, Now());
    }

    void StudioPhase_EndView()
    {
        if (get_recorder().is_active())
        {
            get_recorder().EndView(Now());
        }
    }

    void StudioPhase_BeginPass(int pass)
    {
        if (get_recorder().is_active())
        {
            get_recorder().BeginPass(pass, Now());
        }
    }

    void StudioPhase_EndPass()
    {
        if (get_recorder().is_active())
        {
            get_recorder().EndPass(Now());
        }
    }

    uint64_t StudioPhase_BeginWorld()
    {
        return get_recorder().is_active() ? Now() : 0;
    }

    void StudioPhase_EndWorld(uint64_t started)
    {
        if (started && get_recorder().is_active())
        {
            get_recorder().AddWorldTime(Now() - started);
        }
    }

    void StudioPhase_RegisterCommands()
    {
        Cmd_AddCommand("r_studio_phases_start", Start);
        Cmd_AddCommand("r_studio_phases_stop", Stop);
        Cmd_AddCommand("r_studio_phases_status", Status);
        Cmd_AddCommand("r_studio_phases_export", Export);
    }
} // namespace render
