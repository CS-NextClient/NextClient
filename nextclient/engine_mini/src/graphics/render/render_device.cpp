#include "graphics/render/render_device.h"

#include <glad/glad.h>
#include <next_engine_mini/ShaderRendererInterface.h>

#include "graphics/render/FramebufferDepth.h"
#include "graphics/render/render_resources.h"

namespace render
{
    namespace
    {
        class ShaderRenderer : public ShaderRendererInterface
        {
        public:
            bool is_available() const override
            {
                return GetResources().device.is_available();
            }
        };
    } // namespace

    EXPOSE_SINGLE_INTERFACE(ShaderRenderer, ShaderRendererInterface, SHADER_RENDERER_INTERFACE_VERSION);

    void Device_Register(IDeviceResource* resource)
    {
        GetResources().device.Register(resource);
    }

    void Device_Unregister(IDeviceResource* resource)
    {
        GetResources().device.Unregister(resource);
    }

    void Device_OnContextCreated()
    {
        GetResources().device.OnContextCreated();
    }

    void Device_OnContextDestroyed()
    {
        GetResources().device.OnContextDestroyed();
    }

    bool Device_CheckAvailable()
    {
        return GetResources().device.CheckAvailable();
    }

    bool Device_IsContextAlive()
    {
        return GetResources().device.is_context_alive();
    }

    bool Device_IsContext(void* context)
    {
        return GetResources().device.IsContext(context);
    }

    bool Device_IsContextCurrent()
    {
        return GetResources().device.IsContextCurrent();
    }

    uint32_t Device_Generation()
    {
        return GetResources().device.get_generation();
    }

    bool Framebuffer_QueryDepthFormat(unsigned int target, unsigned int framebuffer, FramebufferDepthFormat_t& result)
    {
        result = {};
        const GLenum attachment = framebuffer == 0 ? GL_DEPTH : GL_DEPTH_ATTACHMENT;
        GLint type = GL_NONE;
        glGetFramebufferAttachmentParameteriv(target, attachment, GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE, &type);
        if (type == GL_NONE)
        {
            return false;
        }
        GLint component = GL_UNSIGNED_NORMALIZED;
        glGetFramebufferAttachmentParameteriv(target, attachment, GL_FRAMEBUFFER_ATTACHMENT_DEPTH_SIZE, &result.depth_bits);
        glGetFramebufferAttachmentParameteriv(target, attachment, GL_FRAMEBUFFER_ATTACHMENT_COMPONENT_TYPE, &component);
        const GLenum stencil_attachment = framebuffer == 0 ? GL_STENCIL : GL_STENCIL_ATTACHMENT;
        glGetFramebufferAttachmentParameteriv(target, stencil_attachment, GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE, &type);
        if (type != GL_NONE)
        {
            glGetFramebufferAttachmentParameteriv(target, stencil_attachment, GL_FRAMEBUFFER_ATTACHMENT_STENCIL_SIZE, &result.stencil_bits);
        }
        if (result.depth_bits == 0)
        {
            return false;
        }
        result.floating = component == GL_FLOAT;
        result.internal_format = result.stencil_bits > 0   ? (result.floating ? GL_DEPTH32F_STENCIL8 : GL_DEPTH24_STENCIL8)
                                 : result.floating         ? GL_DEPTH_COMPONENT32F
                                 : result.depth_bits <= 16 ? GL_DEPTH_COMPONENT16
                                 : result.depth_bits <= 24 ? GL_DEPTH_COMPONENT24
                                                           : GL_DEPTH_COMPONENT32;
        return true;
    }

    const char* Device_GlString(unsigned int name, const char* fallback)
    {
        const GLubyte* value = glGetString(name);

        return value != nullptr ? reinterpret_cast<const char*>(value) : fallback;
    }
} // namespace render
