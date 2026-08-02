#include "graphics/render/GpuBuffer.h"

namespace
{
    GLenum BindingQuery(GLenum target)
    {
        switch (target)
        {
            case GL_ELEMENT_ARRAY_BUFFER: return GL_ELEMENT_ARRAY_BUFFER_BINDING;
            case GL_UNIFORM_BUFFER: return GL_UNIFORM_BUFFER_BINDING;
            default: return GL_ARRAY_BUFFER_BINDING;
        }
    }
}

render::GpuBuffer::GpuBuffer() :
    name_(0),
    target_(GL_ARRAY_BUFFER),
    size_(0)
{
    render::Device_Register(this);
}

render::GpuBuffer::~GpuBuffer()
{
    Destroy();
    render::Device_Unregister(this);
}

bool render::GpuBuffer::Create(GLenum target, const void* data, size_t size, GLenum usage)
{
    Destroy();

    if (!Device_Available() || size == 0)
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

bool render::GpuBuffer::Update(const void* data, size_t size)
{
    if (name_ == 0 || size > size_ || !render::Device_ContextAlive())
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

void render::GpuBuffer::Destroy()
{
    if (name_ != 0 && render::Device_ContextAlive())
    {
        glDeleteBuffers(1, &name_);
    }

    name_ = 0;
    size_ = 0;
}

void render::GpuBuffer::OnDeviceLost()
{
    name_ = 0;
    size_ = 0;
}

render::GpuBufferSlice render::GpuBuffer::slice(size_t offset, size_t size) const
{
    return GpuBufferSlice {name_, offset, size};
}
