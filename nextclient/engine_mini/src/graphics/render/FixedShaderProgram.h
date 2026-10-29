#pragma once

#include <cstdint>

namespace ncl_gl
{
    struct Program;
}

namespace render
{
    class ShaderManager;
    struct ProgramDesc;

    class FixedShaderProgram
    {
    private:
        const ncl_gl::Program* program_{};
        uint32_t generation_{};

    public:
        const ncl_gl::Program* Get(ShaderManager& manager, const ProgramDesc& desc);
    };
} // namespace render
