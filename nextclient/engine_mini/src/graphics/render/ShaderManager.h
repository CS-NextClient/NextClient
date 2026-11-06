#pragma once

#include <memory>
#include <string>
#include <vector>

#include "graphics/render/RenderDevice.h"
#include "graphics/render/shader_manager.h"

namespace render
{
    class ShaderBackend
    {
    public:
        virtual ~ShaderBackend() = default;
        virtual bool LoadSource(const char* name, std::string& out) = 0;
        virtual std::vector<std::string> ListPrograms() = 0;
        virtual bool Build(const ProgramDesc& desc, ncl_gl::Program& program) = 0;
        virtual void DeleteProgram(unsigned int program) = 0;
        virtual double Time() = 0;
        virtual void ReportWarmup(int ready, int total, int compiled, double started) = 0;
    };

    class ShaderManager final : public IDeviceResource
    {
    private:
        struct Entry;
        RenderDevice& device_;
        ShaderBackend& backend_;
        std::vector<std::unique_ptr<Entry>> programs_;
        uint32_t generation_ = 1;
        int built_from_source_{};

    public:
        ShaderManager(RenderDevice& device, ShaderBackend& backend);
        ~ShaderManager() override;
        ShaderManager(const ShaderManager&) = delete;
        ShaderManager& operator=(const ShaderManager&) = delete;
        const ncl_gl::Program* Get(const ProgramDesc& desc);
        void WarmUp(const ProgramDesc* descs, int count);
        std::vector<std::string> ListPrograms();
        bool LoadSource(const char* name, std::string& out);
        void Clear();
        void OnDeviceLost() override;

        uint32_t get_generation() const
        {
            return generation_;
        }

        const uint32_t& get_generation_source() const
        {
            return generation_;
        }

        int get_built_from_source() const
        {
            return built_from_source_;
        }
    };

    ShaderBackend& GetShaderBackend();
} // namespace render
