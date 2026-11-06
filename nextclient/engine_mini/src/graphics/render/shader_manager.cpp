#include "graphics/render/shader_manager.h"


#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include <cmrc/cmrc.hpp>
#include <glad/glad.h>
#include <nitro_utils/string_utils.h>
#include <tier1/strtools.h>
#include <ncl_gl/program.h>
#include <next_engine_mini/performance_program_callbacks.h>

#include "engine.h"
#include "graphics/render/ShaderManager.h"
#include "graphics/render/render_resources.h"
#include "common/sys_dll.h"
#include "console/console.h"
#include "graphics/render/shader_includes.h"
#include "performance/PerformanceMonitor.h"

CMRC_DECLARE(engine_mini_rc);

namespace render
{
    namespace
    {
        constexpr const char* kEmbeddedRoot = "assets/shaders/";
        constexpr std::string_view kVertexSuffix = ".vert";
        constexpr std::string_view kFragmentSuffix = ".frag";
        bool LoadSourceInternal(const char* name, std::string& out)
        {
            cmrc::embedded_filesystem fs = cmrc::engine_mini_rc::get_filesystem();
            std::string embedded_path = std::string(kEmbeddedRoot) + name;

            if (!fs.is_file(embedded_path))
            {
                Con_DPrintf(ConLogType::Error, "render: shader source %s not found\n", name);

                return false;
            }

            cmrc::file file = fs.open(embedded_path);
            out.assign(file.begin(), file.end());

            return true;
        }

        std::string ApplyDefines(const std::string& source, const char* defines)
        {
            if (defines == nullptr || *defines == '\0')
            {
                return source;
            }

            // GLSL requires #version to be the first directive, so the defines go on the
            // line right after it rather than at the top.
            size_t version_pos = source.find("#version");

            if (version_pos == std::string::npos)
            {
                return std::string(defines) + "\n" + source;
            }

            size_t line_end = source.find('\n', version_pos);

            if (line_end == std::string::npos)
            {
                return source + "\n" + defines + "\n";
            }

            return source.substr(0, line_end + 1) + defines + "\n" + source.substr(line_end + 1);
        }

        std::string MakeKey(const ProgramDesc& desc)
        {
            std::string key = desc.vs_name;
            key += '|';
            key += desc.fs_name;
            key += '|';

            if (desc.defines != nullptr)
            {
                key += desc.defines;
            }

            for (int i = 0; i < desc.uniform_count; i++)
            {
                key += '|';
                key += desc.uniform_names[i];
            }

            for (int i = 0; i < desc.attrib_count; i++)
            {
                key += '|';
                key += desc.attrib_names[i];
            }

            return key;
        }

        // Short enough to read down a column of them. The key carries the whole uniform table
        // and is kept for the failure line, where the exact identity is what is being asked.
        std::string ProgramLabel(const ProgramDesc& desc)
        {
            std::string label = desc.vs_name;
            label += " + ";
            label += desc.fs_name;

            if (desc.defines != nullptr && *desc.defines != '\0')
            {
                label += " [defines]";
            }

            return label;
        }

        double Milliseconds(double since)
        {
            return (Sys_FloatTime() - since) * 1000.0;
        }


        class EmbeddedShaderBackend final : public ShaderBackend
        {
        private:
            performance::PerformanceMonitor& monitor_;

        public:
            explicit EmbeddedShaderBackend(performance::PerformanceMonitor& monitor) :
                monitor_(monitor)
            {}
            bool LoadSource(const char* name, std::string& out) override
            {
                return LoadSourceInternal(name, out);
            }

            bool Build(const ProgramDesc& desc, ncl_gl::Program& program) override
            {
                std::string key = MakeKey(desc);
                std::string label = ProgramLabel(desc);

                ShaderSourceLoader_t loader = [](std::string_view name, std::string& source) {
                    return LoadSourceInternal(std::string(name).c_str(), source);
                };
                ncl_utils::ResultT<std::string> vertex = ShaderIncludes_Expand(desc.vs_name, loader);
                ncl_utils::ResultT<std::string> fragment = ShaderIncludes_Expand(desc.fs_name, loader);

                if (vertex.has_error() || fragment.has_error())
                {
                    const std::string error = vertex.has_error() ? vertex.error_str() : fragment.error_str();
                    Con_DPrintf(ConLogType::Warning, "render: %s: %s\n", label.c_str(), error.c_str());

                    return false;
                }

                std::string vs_source = ApplyDefines(*vertex, desc.defines);
                std::string fs_source = ApplyDefines(*fragment, desc.defines);

                double started = Sys_FloatTime();

                std::string info_log;
                const ncl_gl::ProgramBuildCallbacks callbacks = performance::MakeProgramBuildCallbacks(&monitor_);
                ncl_gl::LoadProgram(
                    program,
                    vs_source.c_str(),
                    fs_source.c_str(),
                    desc.uniform_names,
                    desc.uniform_count,
                    desc.attrib_names,
                    desc.attrib_count,
                    &info_log,
                    &callbacks
                );

                double elapsed = Milliseconds(started);

                // Drivers end the log with a newline, and some answer a clean build with a line of
                // their own rather than with nothing.
                nitro_utils::rtrim(info_log);

                if (!program.ok)
                {
                    // Printed unconditionally: a program that fails to build silently disables a
                    // whole stage, and the fallback that follows looks like nothing happened.
                    Con_Printf("render: program %s failed: %s\n", key.c_str(), info_log.c_str());

                    return false;
                }


                Con_DPrintf(
                    ConLogType::Info,
                    "render: %s compiled, %u + %u bytes of source, %.1f ms\n",
                    label.c_str(),
                    static_cast<unsigned>(vs_source.size()),
                    static_cast<unsigned>(fs_source.size()),
                    elapsed
                );

                if (!info_log.empty())
                {
                    Con_DPrintf(ConLogType::Info, "render:   %s driver log: %s\n", label.c_str(), info_log.c_str());
                }

                return true;
            }

            std::vector<std::string> ListPrograms() override
            {
                cmrc::embedded_filesystem fs = cmrc::engine_mini_rc::get_filesystem();
                std::string root(kEmbeddedRoot);

                // Trailing separator: the resource paths carry one, iterate_directory does not take it.
                root.pop_back();

                std::vector<std::string> names;

                if (!fs.is_directory(root))
                {
                    return names;
                }

                for (const cmrc::directory_entry& file : fs.iterate_directory(root))
                {
                    const std::string& filename = file.filename();

                    if (!file.is_file() || !filename.ends_with(kVertexSuffix))
                    {
                        continue;
                    }

                    std::string name = filename.substr(0, filename.size() - kVertexSuffix.size());

                    if (fs.is_file(root + "/" + name + std::string(kFragmentSuffix)))
                    {
                        names.push_back(std::move(name));
                    }
                }

                return names;
            }

            void DeleteProgram(unsigned int program) override
            {
                glDeleteProgram(program);
            }
            double Time() override
            {
                return Sys_FloatTime();
            }
            void ReportWarmup(int ready, int total, int compiled, double started) override
            {
                Con_DPrintf(
                    ConLogType::Info,
                    "render: %d of %d programs ready, %d compiled, %.0f ms\n",
                    ready,
                    total,
                    compiled,
                    Milliseconds(started)
                );
            }
        };
    } // namespace

    ShaderBackend& GetShaderBackend()
    {
        static EmbeddedShaderBackend backend(performance::GetMonitor());
        return backend;
    }

    const ncl_gl::Program* Shaders_Get(const ProgramDesc& desc)
    {
        return GetResources().shaders.Get(desc);
    }

    void Shaders_WarmUp(const ProgramDesc* descs, int count)
    {
        GetResources().shaders.WarmUp(descs, count);
    }

    std::vector<std::string> Shaders_ListPrograms()
    {
        return GetResources().shaders.ListPrograms();
    }

    uint32_t Shaders_Generation()
    {
        return GetResources().shaders.get_generation();
    }

    void Shaders_Clear()
    {
        GetResources().shaders.Clear();
    }

    bool Shaders_LoadSource(const char* name, std::string& out)
    {
        return GetResources().shaders.LoadSource(name, out);
    }
} // namespace render
