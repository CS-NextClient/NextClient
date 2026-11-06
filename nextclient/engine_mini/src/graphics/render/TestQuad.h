#pragma once

#include "graphics/render/GpuBuffer.h"
#include "graphics/render/ShaderManager.h"

namespace render
{
    class TestQuad final : public IDeviceResource
    {
    private:
        friend class TestQuadTestAccess;
        RenderDevice& device_;
        ShaderManager& shaders_;
        GpuBuffer vertices_;
        unsigned int vao_{};

    public:
        TestQuad(RenderDevice& device, ShaderManager& shaders);
        ~TestQuad() override;
        TestQuad(const TestQuad&) = delete;
        TestQuad& operator=(const TestQuad&) = delete;
        void Draw(float time);
        void Clear();
        void OnDeviceLost() override;

    private:
        bool Ensure();
    };
} // namespace render
