#pragma once

#include <tier1/interface.h>

namespace render
{
    class ShaderRendererInterface : public IBaseInterface
    {
    public:
        virtual bool is_available() const = 0;
    };
} // namespace render

#define SHADER_RENDERER_INTERFACE_VERSION "ShaderRenderer001"
