#include "graphics/render/ShaderManager.h"

#include "graphics/render/FixedShaderProgram.h"

namespace render
{
    const ncl_gl::Program* FixedShaderProgram::Get(ShaderManager& manager, const ProgramDesc& desc)
    {
        const uint32_t generation = manager.get_generation();
        if (generation_ != generation)
        {
            program_ = manager.Get(desc);
            generation_ = generation;
        }

        return program_;
    }

    struct ShaderManager::Entry
    {
        std::string vertex;
        std::string fragment;
        std::string defines;
        std::vector<std::string> uniforms;
        std::vector<std::string> attributes;
        ncl_gl::Program program;
        bool failed = true;

        explicit Entry(const ProgramDesc& desc) :
            vertex(desc.vs_name),
            fragment(desc.fs_name),
            defines(desc.defines != nullptr ? desc.defines : "")
        {
            for (int index = 0; index < desc.uniform_count; ++index)
            {
                uniforms.emplace_back(desc.uniform_names[index]);
            }

            for (int index = 0; index < desc.attrib_count; ++index)
            {
                attributes.emplace_back(desc.attrib_names[index]);
            }
        }

        bool Matches(const ProgramDesc& desc) const
        {
            if (vertex != desc.vs_name || fragment != desc.fs_name || defines != (desc.defines != nullptr ? desc.defines : "") ||
                uniforms.size() != static_cast<size_t>(desc.uniform_count) || attributes.size() != static_cast<size_t>(desc.attrib_count))
            {
                return false;
            }

            for (int index = 0; index < desc.uniform_count; ++index)
            {
                if (uniforms[index] != desc.uniform_names[index])
                {
                    return false;
                }
            }

            for (int index = 0; index < desc.attrib_count; ++index)
            {
                if (attributes[index] != desc.attrib_names[index])
                {
                    return false;
                }
            }

            return true;
        }
    };

    ShaderManager::ShaderManager(RenderDevice& device, ShaderBackend& backend) :
        device_(device),
        backend_(backend)
    {
        device_.Register(this);
    }

    ShaderManager::~ShaderManager()
    {
        device_.Unregister(this);
    }

    const ncl_gl::Program* ShaderManager::Get(const ProgramDesc& desc)
    {
        if (!device_.CheckAvailable())
        {
            return nullptr;
        }

        for (const std::unique_ptr<Entry>& entry : programs_)
        {
            if (entry->Matches(desc))
            {
                return entry->failed ? nullptr : &entry->program;
            }
        }

        programs_.push_back(std::make_unique<Entry>(desc));
        Entry& entry = *programs_.back();
        entry.failed = !backend_.Build(desc, entry.program);

        if (entry.failed)
        {
            return nullptr;
        }

        ++built_from_source_;

        return &entry.program;
    }

    void ShaderManager::WarmUp(const ProgramDesc* descs, int count)
    {
        if (!device_.CheckAvailable())
        {
            return;
        }

        built_from_source_ = 0;
        double started = backend_.Time();
        int ready = 0;

        for (int index = 0; index < count; ++index)
        {
            if (Get(descs[index]) != nullptr)
            {
                ++ready;
            }
        }

        backend_.ReportWarmup(ready, count, built_from_source_, started);
    }

    std::vector<std::string> ShaderManager::ListPrograms()
    {
        return backend_.ListPrograms();
    }

    bool ShaderManager::LoadSource(const char* name, std::string& out)
    {
        return backend_.LoadSource(name, out);
    }

    void ShaderManager::Clear()
    {
        if (device_.IsContextCurrent())
        {
            for (const std::unique_ptr<Entry>& entry : programs_)
            {
                if (entry->program.prog != 0)
                {
                    backend_.DeleteProgram(entry->program.prog);
                }
            }
        }

        OnDeviceLost();
    }

    void ShaderManager::OnDeviceLost()
    {
        programs_.clear();
        ++generation_;
    }
} // namespace render
