#include "graphics/preview/PreviewSceneTarget.h"

#include <cassert>

#include <glad/glad.h>

#include "graphics/render/FramebufferDepth.h"

namespace render::preview
{
    namespace
    {
        GLenum ColorFormat(GLuint framebuffer, GLenum buffer)
        {
            GLenum attachment = buffer;
            if (framebuffer == 0)
            {
                attachment = buffer == GL_FRONT || buffer == GL_FRONT_LEFT ? GL_FRONT_LEFT : GL_BACK_LEFT;
            }

            GLint type = GL_NONE;
            glGetFramebufferAttachmentParameteriv(GL_DRAW_FRAMEBUFFER, attachment, GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE, &type);
            if (type == GL_NONE)
            {
                return GL_NONE;
            }

            if (type == GL_RENDERBUFFER)
            {
                GLint object = 0;
                GLint format = 0;
                glGetFramebufferAttachmentParameteriv(GL_DRAW_FRAMEBUFFER, attachment, GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME, &object);
                glBindRenderbuffer(GL_RENDERBUFFER, object);
                glGetRenderbufferParameteriv(GL_RENDERBUFFER, GL_RENDERBUFFER_INTERNAL_FORMAT, &format);
                return format;
            }

            GLint red = 0;
            GLint green = 0;
            GLint blue = 0;
            GLint alpha = 0;
            GLint component = 0;
            GLint encoding = 0;
            glGetFramebufferAttachmentParameteriv(GL_DRAW_FRAMEBUFFER, attachment, GL_FRAMEBUFFER_ATTACHMENT_RED_SIZE, &red);
            glGetFramebufferAttachmentParameteriv(GL_DRAW_FRAMEBUFFER, attachment, GL_FRAMEBUFFER_ATTACHMENT_GREEN_SIZE, &green);
            glGetFramebufferAttachmentParameteriv(GL_DRAW_FRAMEBUFFER, attachment, GL_FRAMEBUFFER_ATTACHMENT_BLUE_SIZE, &blue);
            glGetFramebufferAttachmentParameteriv(GL_DRAW_FRAMEBUFFER, attachment, GL_FRAMEBUFFER_ATTACHMENT_ALPHA_SIZE, &alpha);
            glGetFramebufferAttachmentParameteriv(GL_DRAW_FRAMEBUFFER, attachment, GL_FRAMEBUFFER_ATTACHMENT_COMPONENT_TYPE, &component);
            glGetFramebufferAttachmentParameteriv(GL_DRAW_FRAMEBUFFER, attachment, GL_FRAMEBUFFER_ATTACHMENT_COLOR_ENCODING, &encoding);

            if (encoding == GL_SRGB)
            {
                return alpha ? GL_SRGB8_ALPHA8 : GL_SRGB8;
            }
            if (component == GL_FLOAT)
            {
                if (red == 11 && green == 11 && blue == 10)
                {
                    return GL_R11F_G11F_B10F;
                }
                if (alpha)
                {
                    return red == 16 ? GL_RGBA16F : GL_RGBA32F;
                }
                if (blue)
                {
                    return red == 16 ? GL_RGB16F : GL_RGB32F;
                }
                return green ? (red == 16 ? GL_RG16F : GL_RG32F) : (red == 16 ? GL_R16F : GL_R32F);
            }
            // Fixed-function preview rendering requires a normalized or floating-point color attachment.
            if (component != GL_UNSIGNED_NORMALIZED)
            {
                return GL_NONE;
            }
            if (red == 10 && green == 10 && blue == 10 && alpha == 2)
            {
                return GL_RGB10_A2;
            }
            if (red == 5 && green == 6 && blue == 5 && alpha == 0)
            {
                return GL_RGB565;
            }
            if (red == 5 && green == 5 && blue == 5 && alpha == 1)
            {
                return GL_RGB5_A1;
            }
            if (red == 4 && green == 4 && blue == 4 && alpha == 4)
            {
                return GL_RGBA4;
            }
            if (red != 8 && red != 16)
            {
                return GL_NONE;
            }
            if (alpha)
            {
                return red == 16 ? GL_RGBA16 : GL_RGBA8;
            }
            if (blue)
            {
                return red == 16 ? GL_RGB16 : GL_RGB8;
            }
            return green ? (red == 16 ? GL_RG16 : GL_RG8) : (red == 16 ? GL_R16 : GL_R8);
        }

        void Storage(GLuint image, GLenum format, int samples, int wide, int tall)
        {
            glBindRenderbuffer(GL_RENDERBUFFER, image);
            if (samples > 0)
            {
                glRenderbufferStorageMultisample(GL_RENDERBUFFER, samples, format, wide, tall);
            }
            else
            {
                glRenderbufferStorage(GL_RENDERBUFFER, format, wide, tall);
            }
        }
    } // namespace

    PreviewSceneTarget::PreviewSceneTarget(RenderDevice& device) :
        device_(device)
    {
        device_.Register(this);
    }

    PreviewSceneTarget::~PreviewSceneTarget()
    {
        OnDeviceLost();
        device_.Unregister(this);
    }

    bool PreviewSceneTarget::Begin(const PreviewRect& view, bool preserve_color)
    {
        assert(!active_);
        if (view.wide <= 0 || view.tall <= 0)
        {
            return false;
        }

        glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &caller_read_);
        glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &caller_draw_);
        glGetIntegerv(GL_RENDERBUFFER_BINDING, &caller_renderbuffer_);
        glGetIntegerv(GL_VIEWPORT, caller_viewport_);
        glGetIntegerv(GL_SCISSOR_BOX, caller_scissor_);
        caller_scissor_enabled_ = glIsEnabled(GL_SCISSOR_TEST) != 0;
        caller_srgb_ = glIsEnabled(GL_FRAMEBUFFER_SRGB) != 0;
        active_ = true;

        GLint draw_buffer = GL_NONE;
        GLint samples = 0;
        glGetIntegerv(GL_DRAW_BUFFER0, &draw_buffer);
        glGetIntegerv(GL_SAMPLES, &samples);
        render::FramebufferDepthFormat_t depth;
        const GLenum color = draw_buffer == GL_NONE ? GL_NONE : ColorFormat(caller_draw_, draw_buffer);

        if (color == GL_NONE || !render::Framebuffer_QueryDepthFormat(GL_DRAW_FRAMEBUFFER, caller_draw_, depth))
        {
            Restore();
            return false;
        }

        if (framebuffer_ == 0)
        {
            glGenFramebuffers(1, &framebuffer_);
            glGenRenderbuffers(1, &color_);
            glGenRenderbuffers(1, &depth_);
        }

        glBindFramebuffer(GL_FRAMEBUFFER, framebuffer_);
        if (wide_ != view.wide || tall_ != view.tall || color_format_ != color || depth_format_ != depth.internal_format ||
            samples_ != samples)
        {
            Storage(color_, color, samples, view.wide, view.tall);
            Storage(depth_, depth.internal_format, samples, view.wide, view.tall);
            glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, color_);
            glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depth_);
            glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_RENDERBUFFER, depth.stencil_bits ? depth_ : 0);
            glDrawBuffer(GL_COLOR_ATTACHMENT0);
            glReadBuffer(GL_COLOR_ATTACHMENT0);
            wide_ = view.wide;
            tall_ = view.tall;
            color_format_ = color;
            depth_format_ = depth.internal_format;
            samples_ = samples;
        }

        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        {
            Restore();
            return false;
        }

        if (preserve_color || depth.stencil_bits > 0)
        {
            glBindFramebuffer(GL_READ_FRAMEBUFFER, caller_draw_);
            GLint read_buffer = GL_NONE;
            glGetIntegerv(GL_READ_BUFFER, &read_buffer);
            if (preserve_color)
            {
                glReadBuffer(draw_buffer);
            }
            glDisable(GL_SCISSOR_TEST);
            glDisable(GL_FRAMEBUFFER_SRGB);
            glBlitFramebuffer(
                view.x,
                view.y,
                view.x + view.wide,
                view.y + view.tall,
                0,
                0,
                view.wide,
                view.tall,
                (preserve_color ? GL_COLOR_BUFFER_BIT : 0) | (depth.stencil_bits ? GL_STENCIL_BUFFER_BIT : 0),
                GL_NEAREST
            );
            glReadBuffer(read_buffer);
            glBindFramebuffer(GL_READ_FRAMEBUFFER, framebuffer_);
        }
        // Effects sample the entire view even when presentation is clipped.
        GLboolean depth_mask = GL_FALSE;
        GLdouble clear_depth = 1;
        glGetBooleanv(GL_DEPTH_WRITEMASK, &depth_mask);
        glGetDoublev(GL_DEPTH_CLEAR_VALUE, &clear_depth);
        glDisable(GL_SCISSOR_TEST);
        glDepthMask(GL_TRUE);
        glClearDepth(1);
        glClear(GL_DEPTH_BUFFER_BIT);
        glDepthMask(depth_mask);
        glClearDepth(clear_depth);
        if (caller_srgb_)
        {
            glEnable(GL_FRAMEBUFFER_SRGB);
        }
        glBindRenderbuffer(GL_RENDERBUFFER, caller_renderbuffer_);
        glViewport(0, 0, view.wide, view.tall);
        glScissor(0, 0, view.wide, view.tall);
        glEnable(GL_SCISSOR_TEST);
        return true;
    }

    void PreviewSceneTarget::Present(const PreviewRect& view, const PreviewRect& clip)
    {
        assert(active_);
        glBindFramebuffer(GL_READ_FRAMEBUFFER, framebuffer_);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, caller_draw_);
        glEnable(GL_SCISSOR_TEST);
        glScissor(clip.x, clip.y, clip.wide, clip.tall);
        glDisable(GL_FRAMEBUFFER_SRGB);
        glBlitFramebuffer(
            0,
            0,
            wide_,
            tall_,
            view.x,
            view.y,
            view.x + view.wide,
            view.y + view.tall,
            GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT,
            GL_NEAREST
        );
        Restore();
    }

    void PreviewSceneTarget::Cancel()
    {
        if (active_ && device_.IsContextCurrent())
        {
            Restore();
        }

        active_ = false;
    }

    void PreviewSceneTarget::Restore()
    {
        glBindFramebuffer(GL_READ_FRAMEBUFFER, caller_read_);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, caller_draw_);
        glBindRenderbuffer(GL_RENDERBUFFER, caller_renderbuffer_);
        glViewport(caller_viewport_[0], caller_viewport_[1], caller_viewport_[2], caller_viewport_[3]);
        glScissor(caller_scissor_[0], caller_scissor_[1], caller_scissor_[2], caller_scissor_[3]);
        (caller_scissor_enabled_ ? glEnable : glDisable)(GL_SCISSOR_TEST);
        (caller_srgb_ ? glEnable : glDisable)(GL_FRAMEBUFFER_SRGB);
        active_ = false;
    }

    void PreviewSceneTarget::Clear()
    {
        if (device_.IsContextCurrent())
        {
            if (active_)
            {
                Restore();
            }
            glDeleteFramebuffers(1, &framebuffer_);
            glDeleteRenderbuffers(1, &color_);
            glDeleteRenderbuffers(1, &depth_);
        }
        OnDeviceLost();
    }

    void PreviewSceneTarget::OnDeviceLost()
    {
        framebuffer_ = color_ = depth_ = 0;
        color_format_ = depth_format_ = 0;
        wide_ = tall_ = samples_ = 0;
        active_ = false;
    }

} // namespace render::preview
