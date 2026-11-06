#include "graphics/render/GpuBuffer.h"

#include "graphics/render/RenderDevice.h"

namespace render
{
    namespace
    {
        GLenum BindingQuery(GLenum target)
        {
            switch (target)
            {
                case GL_ELEMENT_ARRAY_BUFFER:
                    return GL_ELEMENT_ARRAY_BUFFER_BINDING;
                case GL_UNIFORM_BUFFER:
                    return GL_UNIFORM_BUFFER_BINDING;
                default:
                    return GL_ARRAY_BUFFER_BINDING;
            }
        }
    } // namespace

    GpuBuffer::GpuBuffer(RenderDevice& device) :
        device_(device),
        name_(0),
        target_(GL_ARRAY_BUFFER),
        size_(0)
    {
        device_.Register(this);
    }

    GpuBuffer::~GpuBuffer()
    {
        OnDeviceLost();
        device_.Unregister(this);
    }

    bool GpuBuffer::Create(GLenum target, const void* data, size_t size, GLenum usage)
    {
        Destroy();

        if (!device_.CheckAvailable() || size == 0)
        {
            return false;
        }

        target_ = target;
        size_ = size;

        glGenBuffers(1, &name_);

        if (name_ == 0)
        {
            size_ = 0;

            return false;
        }

        GLint previous = 0;
        glGetIntegerv(BindingQuery(target_), &previous);

        glBindBuffer(target_, name_);
        glBufferData(target_, static_cast<GLsizeiptr>(size), data, usage);
        glBindBuffer(target_, static_cast<GLuint>(previous));

        return true;
    }

    bool GpuBuffer::Update(const void* data, size_t size)
    {
        if (name_ == 0 || size > size_ || !device_.IsContextCurrent())
        {
            return false;
        }

        GLint previous = 0;
        glGetIntegerv(BindingQuery(target_), &previous);

        glBindBuffer(target_, name_);
        glBufferSubData(target_, 0, static_cast<GLsizeiptr>(size), data);
        glBindBuffer(target_, static_cast<GLuint>(previous));

        return true;
    }

    void GpuBuffer::Destroy()
    {
        if (name_ != 0 && device_.IsContextCurrent())
        {
            glDeleteBuffers(1, &name_);
        }

        name_ = 0;
        size_ = 0;
    }

    void GpuBuffer::OnDeviceLost()
    {
        name_ = 0;
        size_ = 0;
    }

    GpuBufferSlice GpuBuffer::GetSlice(size_t offset, size_t size) const
    {
        return GpuBufferSlice{name_, offset, size};
    }
} // namespace render
