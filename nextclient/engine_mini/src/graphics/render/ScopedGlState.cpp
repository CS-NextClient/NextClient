#include "graphics/render/ScopedGlState.h"

#include <glad/glad.h>

#include "graphics/render/RenderDevice.h"

namespace render
{
    ScopedGlState::ScopedGlState()
    {
        texture_cache_ = GL_CaptureTextureCache();
        modern_ = GLAD_GL_VERSION_3_3 != 0;
        glGetIntegerv(GL_MATRIX_MODE, &matrix_mode_);

        if (modern_)
        {
            glGetIntegerv(GL_CURRENT_PROGRAM, &program_);
            glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &vao_);
            glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &array_buffer_);
            glGetIntegerv(GL_ELEMENT_ARRAY_BUFFER_BINDING, &element_buffer_);
            glGetIntegerv(GL_UNIFORM_BUFFER_BINDING, &uniform_buffer_);
            glGetIntegeri_v(GL_UNIFORM_BUFFER_BINDING, render::kStudioBonesBinding, &bones_binding_);
            glGetInteger64i_v(GL_UNIFORM_BUFFER_START, render::kStudioBonesBinding, &bones_start_);
            glGetInteger64i_v(GL_UNIFORM_BUFFER_SIZE, render::kStudioBonesBinding, &bones_size_);
            glGetIntegerv(GL_RENDERBUFFER_BINDING, &renderbuffer_);
            glGetIntegerv(GL_PIXEL_PACK_BUFFER_BINDING, &pixel_pack_buffer_);
            glGetIntegerv(GL_PIXEL_UNPACK_BUFFER_BINDING, &pixel_unpack_buffer_);
            for (unsigned int unit = 0; unit < samplers_.size(); unit++)
            {
                glGetIntegeri_v(GL_SAMPLER_BINDING, unit, &samplers_[unit]);
            }
            glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &read_framebuffer_);
            glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &draw_framebuffer_);
        }

        glPushAttrib(GL_ALL_ATTRIB_BITS);
        glPushClientAttrib(GL_CLIENT_VERTEX_ARRAY_BIT | GL_CLIENT_PIXEL_STORE_BIT);
        glMatrixMode(GL_PROJECTION);
        glPushMatrix();
        glMatrixMode(GL_MODELVIEW);
        glPushMatrix();
    }

    ScopedGlState::ScopedGlState(RenderDevice& device, const uint64_t* cancellation_epoch) :
        ScopedGlState()
    {
        device_ = &device;
        generation_ = device.get_generation();
        cancellation_epoch_ = cancellation_epoch;
        expected_cancellation_epoch_ = cancellation_epoch != nullptr ? *cancellation_epoch : 0;
    }

    ScopedGlState::~ScopedGlState()
    {
        if (device_ != nullptr)
        {
            if (generation_ != device_->get_generation() || !device_->is_context_alive())
            {
                return;
            }
            if ((cancellation_epoch_ == nullptr || *cancellation_epoch_ != expected_cancellation_epoch_) && !device_->IsContextCurrent())
            {
                return;
            }
        }
        glMatrixMode(GL_PROJECTION);
        glPopMatrix();
        glMatrixMode(GL_MODELVIEW);
        glPopMatrix();
        if (modern_)
        {
            glBindFramebuffer(GL_READ_FRAMEBUFFER, static_cast<GLuint>(read_framebuffer_));
            glBindFramebuffer(GL_DRAW_FRAMEBUFFER, static_cast<GLuint>(draw_framebuffer_));
            // Client attributes restore into the bound VAO, including its generic attribute buffers.
            glBindVertexArray(static_cast<GLuint>(vao_));
        }

        glPopClientAttrib();
        glPopAttrib();
        glMatrixMode(matrix_mode_);

        if (modern_)
        {
            glUseProgram(static_cast<GLuint>(program_));
            glBindBuffer(GL_ARRAY_BUFFER, static_cast<GLuint>(array_buffer_));
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, static_cast<GLuint>(element_buffer_));
            if (bones_binding_ != 0 && bones_size_ > 0)
            {
                glBindBufferRange(
                    GL_UNIFORM_BUFFER,
                    render::kStudioBonesBinding,
                    static_cast<GLuint>(bones_binding_),
                    static_cast<GLintptr>(bones_start_),
                    static_cast<GLsizeiptr>(bones_size_)
                );
            }
            else
            {
                glBindBufferBase(GL_UNIFORM_BUFFER, render::kStudioBonesBinding, static_cast<GLuint>(bones_binding_));
            }

            glBindBuffer(GL_UNIFORM_BUFFER, static_cast<GLuint>(uniform_buffer_));
            glBindRenderbuffer(GL_RENDERBUFFER, static_cast<GLuint>(renderbuffer_));
            glBindBuffer(GL_PIXEL_PACK_BUFFER, static_cast<GLuint>(pixel_pack_buffer_));
            glBindBuffer(GL_PIXEL_UNPACK_BUFFER, static_cast<GLuint>(pixel_unpack_buffer_));
            for (unsigned int unit = 0; unit < samplers_.size(); unit++)
            {
                glBindSampler(unit, static_cast<GLuint>(samplers_[unit]));
            }
        }

        GL_RestoreTextureCache(texture_cache_);
    }
} // namespace render
