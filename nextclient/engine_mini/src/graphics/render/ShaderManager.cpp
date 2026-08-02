#include "engine.h"

#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include <cmrc/cmrc.hpp>
#include <glad/glad.h>
#include <nitro_utils/string_utils.h>
#include <tier1/strtools.h>
#include "graphics/render/RenderDevice.h"
#include <ncl_gl/program.h>

#include "common/filesystem.h"
#include "common/sys_dll.h"
#include "console/console.h"
#include "graphics/render/RenderConfig.h"
#include "graphics/render/ShaderCache.h"
#include "graphics/render/ShaderManager.h"

CMRC_DECLARE(engine_mini_rc);

namespace
{
    constexpr const char* kEmbeddedRoot = "assets/shaders/";
    constexpr const char* kDiskRoot = "shaders/";
    constexpr std::string_view kVertexSuffix = ".vert";
    constexpr std::string_view kFragmentSuffix = ".frag";

    // The hashed fields are NUL-free text, so a NUL after each one keeps differently
    // split concatenations apart, which plain chaining would alias.
    constexpr std::string_view kHashFieldEnd("\0", 1);

    struct Entry
    {
        std::string key;
        ncl_gl::Program program;
        bool failed;
    };

    // unique_ptr, not Entry by value: Shaders_Get hands out a pointer to a program and
    // callers keep it across frames, so adding a second program must not move the first.
    // With one program in the tree this was invisible; the material path added another and
    // every model drawn by the older one vanished.
    std::vector<std::unique_ptr<Entry>> g_Programs;
    uint32_t g_Generation = 1;

    // Programs live in the context they were linked in. On a mode change the names are
    // dropped without a glDeleteProgram: the old context is gone and the names may
    // already belong to programs of the new one.
    class ShaderResources : public render::IDeviceResource
    {
    public:
        void OnDeviceLost() override
        {
            g_Programs.clear();
            g_Generation++;
        }
    };

    ShaderResources g_Resources;
    bool g_Registered;

    void EnsureRegistered()
    {
        if (!g_Registered)
        {
            g_Registered = true;
            render::Device_Register(&g_Resources);
        }
    }

    bool LoadFromDisk(const char* name, std::string& out)
    {
        std::string path = std::string(kDiskRoot) + name;

        FileHandle_t file = FS_Open(path.c_str(), "rb");
        if (file == nullptr)
        {
            return false;
        }

        unsigned int size = FS_Size(file);
        out.resize(size);

        int read = size != 0 ? FS_Read(out.data(), static_cast<int>(size), file) : 0;
        FS_Close(file);

        if (read != static_cast<int>(size))
        {
            return false;
        }

        return true;
    }

    bool ShaderReload()
    {
        return render::g_RenderCfg.shader_reload != nullptr && render::g_RenderCfg.shader_reload->value != 0.0f;
    }

    bool LoadSourceInternal(const char* name, std::string& out)
    {
        bool reload = ShaderReload();

        if (reload && LoadFromDisk(name, out))
        {
            return true;
        }

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

    // The uniform table is part of the identity, not just the sources: the same pair of
    // sources is asked for by stages that name different uniforms, and a shared entry
    // would hand one of them the locations resolved for the other.
    std::string MakeKey(const render::ProgramDesc& desc)
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

    // What the driver's blob was produced from. The uniform names are left out on purpose:
    // they do not reach the linker, and the locations are resolved against the loaded
    // program afterwards, so two stages naming different uniforms can share one blob.
    uint64_t SourceHash(const render::ProgramDesc& desc, const std::string& vs, const std::string& fs)
    {
        uint64_t hash = render::Cache_HashSeed();

        hash = render::Cache_Hash(vs, hash);
        hash = render::Cache_Hash(kHashFieldEnd, hash);
        hash = render::Cache_Hash(fs, hash);
        hash = render::Cache_Hash(kHashFieldEnd, hash);

        for (int i = 0; i < desc.attrib_count; i++)
        {
            hash = render::Cache_Hash(desc.attrib_names[i], hash);
            hash = render::Cache_Hash(kHashFieldEnd, hash);
        }

        return hash;
    }

    Entry* Find(const std::string& key)
    {
        for (const std::unique_ptr<Entry>& entry : g_Programs)
        {
            if (entry->key == key)
            {
                return entry.get();
            }
        }

        return nullptr;
    }

    // Counted per batch, so the start-up summary can say which of the two the run paid for.
    int g_BuiltFromCache;
    int g_BuiltFromSource;

    // Short enough to read down a column of them. The key carries the whole uniform table
    // and is kept for the failure line, where the exact identity is what is being asked.
    std::string ProgramLabel(const render::ProgramDesc& desc)
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

    double Kilobytes(size_t bytes)
    {
        return static_cast<double>(bytes) / 1024.0;
    }

    const ncl_gl::Program* Build(const render::ProgramDesc& desc)
    {
        if (!render::Device_Available())
        {
            return nullptr;
        }

        EnsureRegistered();

        std::string key = MakeKey(desc);

        if (Entry* cached = Find(key))
        {
            return cached->failed ? nullptr : &cached->program;
        }

        g_Programs.push_back(std::make_unique<Entry>());
        Entry& entry = *g_Programs.back();
        entry.key = key;
        entry.failed = true;

        std::string label = ProgramLabel(desc);

        std::string vs_source;
        std::string fs_source;

        if (!LoadSourceInternal(desc.vs_name, vs_source) || !LoadSourceInternal(desc.fs_name, fs_source))
        {
            Con_DPrintf(ConLogType::Warning, "render: %s has no source\n", label.c_str());
            return nullptr;
        }

        vs_source = ApplyDefines(vs_source, desc.defines);
        fs_source = ApplyDefines(fs_source, desc.defines);

        // Bypassed entirely while sources are being edited: r_shader_reload exists to see
        // an edited program build, and a cache in that path is one more thing to rule out.
        bool use_cache = !ShaderReload() && ncl_gl::ProgramBinarySupported();
        uint64_t hash = use_cache ? SourceHash(desc, vs_source, fs_source) : 0;

        if (use_cache)
        {
            GLenum format = 0;

            if (const std::vector<uint8_t>* blob = render::Cache_Lookup(hash, format))
            {
                double started = Sys_FloatTime();

                ncl_gl::LoadProgramBinary(
                    entry.program,
                    blob->data(),
                    blob->size(),
                    format,
                    desc.uniform_names,
                    desc.uniform_count
                );

                if (entry.program.ok)
                {
                    g_BuiltFromCache++;
                    entry.failed = false;

                    Con_DPrintf(
                        ConLogType::Info,
                        "render: %s from cache, %.1f KB, %.1f ms\n",
                        label.c_str(),
                        Kilobytes(blob->size()),
                        Milliseconds(started)
                    );

                    return &entry.program;
                }

                Con_DPrintf(ConLogType::Info, "render: %s cached binary refused, compiling\n", label.c_str());
            }
        }

        double started = Sys_FloatTime();

        std::string info_log;
        ncl_gl::LoadProgram(
            entry.program,
            vs_source.c_str(),
            fs_source.c_str(),
            desc.uniform_names,
            desc.uniform_count,
            desc.attrib_names,
            desc.attrib_count,
            &info_log
        );

        double elapsed = Milliseconds(started);

        // Drivers end the log with a newline, and some answer a clean build with a line of
        // their own rather than with nothing.
        nitro_utils::rtrim(info_log);

        if (!entry.program.ok)
        {
            // Printed unconditionally: a program that fails to build silently disables a
            // whole stage, and the fallback that follows looks like nothing happened.
            Con_Printf("render: program %s failed: %s\n", key.c_str(), info_log.c_str());
            return nullptr;
        }

        g_BuiltFromSource++;
        entry.failed = false;

        char cached[64] = "";

        if (use_cache)
        {
            GLenum format = 0;
            std::vector<uint8_t> blob;

            if (ncl_gl::GetProgramBinary(entry.program.prog, format, blob))
            {
                V_sprintf_safe(cached, ", binary %.1f KB cached", Kilobytes(blob.size()));
                render::Cache_Store(hash, format, std::move(blob));
            }
        }

        Con_DPrintf(
            ConLogType::Info,
            "render: %s compiled, %u + %u bytes of source, %.1f ms%s\n",
            label.c_str(),
            static_cast<unsigned>(vs_source.size()),
            static_cast<unsigned>(fs_source.size()),
            elapsed,
            cached
        );

        if (!info_log.empty())
        {
            Con_DPrintf(ConLogType::Info, "render:   %s driver log: %s\n", label.c_str(), info_log.c_str());
        }

        return &entry.program;
    }
}

const ncl_gl::Program* render::Shaders_Get(const ProgramDesc& desc)
{
    const ncl_gl::Program* program = Build(desc);

    // A program built outside the start-up batch is one the shipped set did not cover,
    // so the entry it just added is worth keeping without waiting for the next start.
    Cache_Save();

    return program;
}

void render::Shaders_WarmUp(const ProgramDesc* descs, int count)
{
    if (!Device_Available())
    {
        return;
    }

    g_BuiltFromCache = 0;
    g_BuiltFromSource = 0;

    double started = Sys_FloatTime();
    int built = 0;

    for (int i = 0; i < count; i++)
    {
        if (Build(descs[i]) != nullptr)
        {
            built++;
        }
    }

    Cache_Save();

    Con_DPrintf(
        ConLogType::Info,
        "render: %d of %d programs ready, %d from cache, %d compiled, %.0f ms\n",
        built,
        count,
        g_BuiltFromCache,
        g_BuiltFromSource,
        Milliseconds(started)
    );
}

std::vector<std::string> render::Shaders_ListPrograms()
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

void render::Shaders_Reload()
{
    if (render::Device_ContextAlive())
    {
        for (const std::unique_ptr<Entry>& entry : g_Programs)
        {
            if (entry->program.prog != 0)
            {
                glDeleteProgram(entry->program.prog);
            }
        }
    }

    g_Programs.clear();
    g_Generation++;

    // The stored blobs were linked from the sources being replaced, and answering from
    // them would make the reload look like it did nothing.
    render::Cache_Clear();
}

uint32_t render::Shaders_Generation()
{
    return g_Generation;
}

bool render::Shaders_LoadSource(const char* name, std::string& out)
{
    return LoadSourceInternal(name, out);
}
