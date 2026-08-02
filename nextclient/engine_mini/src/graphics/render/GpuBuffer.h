#pragma once

#include <cstddef>

#include <glad/glad.h>

#include "graphics/render/RenderDevice.h"

namespace render
{
    // A byte range inside a buffer. Passed by value to draw code so several meshes can
    // share one buffer without each owning it.
    struct GpuBufferSlice
    {
        GLuint buffer;
        size_t offset;
        size_t size;
    };

    class GpuBuffer : public IDeviceResource
    {
    public:
        GpuBuffer();
        ~GpuBuffer() override;

        GpuBuffer(const GpuBuffer&) = delete;
        GpuBuffer& operator=(const GpuBuffer&) = delete;

        bool Create(GLenum target, const void* data, size_t size, GLenum usage = GL_STATIC_DRAW);

        // Replaces the contents of an already created buffer. The size must not grow.
        bool Update(const void* data, size_t size);

        void Destroy();

        void OnDeviceLost() override;

        GLuint name() const { return name_; }
        size_t size() const { return size_; }
        bool ok() const { return name_ != 0; }

        GpuBufferSlice slice(size_t offset, size_t size) const;

    private:
        GLuint name_;
        GLenum target_;
        size_t size_;
    };
}
