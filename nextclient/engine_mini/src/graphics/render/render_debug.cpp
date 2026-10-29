#include "graphics/render/render_debug.h"

#include <cstring>

#include <glad/glad.h>

#include "engine.h"
#include "graphics/render/render_device.h"
#include "common/common.h"
#include "console/console.h"
#include "graphics/render/RenderConfig.h"

namespace render
{
    namespace
    {
        const char* SourceName(GLenum source)
        {
            switch (source)
            {
                case GL_DEBUG_SOURCE_API:
                    return "api";
                case GL_DEBUG_SOURCE_WINDOW_SYSTEM:
                    return "wsi";
                case GL_DEBUG_SOURCE_SHADER_COMPILER:
                    return "compiler";
                case GL_DEBUG_SOURCE_THIRD_PARTY:
                    return "thirdparty";
                case GL_DEBUG_SOURCE_APPLICATION:
                    return "app";
                default:
                    return "other";
            }
        }

        const char* TypeName(GLenum type)
        {
            switch (type)
            {
                case GL_DEBUG_TYPE_ERROR:
                    return "error";
                case GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR:
                    return "deprecated";
                case GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR:
                    return "undefined";
                case GL_DEBUG_TYPE_PORTABILITY:
                    return "portability";
                case GL_DEBUG_TYPE_PERFORMANCE:
                    return "performance";
                case GL_DEBUG_TYPE_MARKER:
                    return "marker";
                default:
                    return "other";
            }
        }

        const char* SeverityName(GLenum severity)
        {
            switch (severity)
            {
                case GL_DEBUG_SEVERITY_HIGH:
                    return "high";
                case GL_DEBUG_SEVERITY_MEDIUM:
                    return "medium";
                case GL_DEBUG_SEVERITY_LOW:
                    return "low";
                default:
                    return "notification";
            }
        }

        void APIENTRY
        DebugCallback(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar* message, const void* user_param)
        {
            ConLogType log_type =
                (type == GL_DEBUG_TYPE_ERROR || severity == GL_DEBUG_SEVERITY_HIGH) ? ConLogType::Error : ConLogType::Warning;

            Con_DPrintf(
                log_type,
                "GL [%s/%s/%s] %u: %s\n",
                SourceName(source),
                TypeName(type),
                SeverityName(severity),
                id,
                message != nullptr ? message : ""
            );
        }

        bool InstallCallback()
        {
            if (glDebugMessageCallback != nullptr)
            {
                glDebugMessageCallback(DebugCallback, nullptr);

                return true;
            }

            // Desktop drivers normally expose the unsuffixed KHR_debug entry points; the
            // suffixed ones exist mainly on ES, so they are only a fallback here.
            if (glDebugMessageCallbackKHR != nullptr)
            {
                glDebugMessageCallbackKHR(DebugCallback, nullptr);

                return true;
            }

            return false;
        }

        void Control(GLenum source, GLenum type, GLenum severity, GLboolean enabled)
        {
            if (glDebugMessageControl != nullptr)
            {
                glDebugMessageControl(source, type, severity, 0, nullptr, enabled);
            }
            else if (glDebugMessageControlKHR != nullptr)
            {
                glDebugMessageControlKHR(source, type, severity, 0, nullptr, enabled);
            }
        }
    } // namespace

    void Debug_Apply()
    {
        if (!Device_CheckAvailable())
        {
            return;
        }

        // The cvar does not exist yet the first time this runs. It is registered in Draw_Init,
        // and the context - with the shader warm-up this switch is there to instrument - is
        // created before that, so the command line is the only switch available that early. It
        // is the same escape hatch, and there for the same reason, as -noshaderwarmup.
        int level = g_RenderCfg.debug_output != nullptr ? static_cast<int>(g_RenderCfg.debug_output->value)
                                                        : (COM_CheckParm("-gldebug") != 0 ? 2 : 0);

        if (level <= 0)
        {
            if (glDebugMessageCallback != nullptr || glDebugMessageCallbackKHR != nullptr)
            {
                glDisable(GL_DEBUG_OUTPUT);
                Control(GL_DONT_CARE, GL_DONT_CARE, GL_DONT_CARE, GL_FALSE);
            }

            return;
        }

        if (!InstallCallback())
        {
            Con_DPrintf(ConLogType::Warning, "render: gl_debug_output is set but the driver exposes no debug output\n");
            return;
        }

        glEnable(GL_DEBUG_OUTPUT);

        // Synchronous delivery costs performance but reports on the offending call, which is
        // the only reason to turn a diagnostic switch on.
        glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);

        Control(GL_DONT_CARE, GL_DONT_CARE, GL_DONT_CARE, level >= 2 ? GL_TRUE : GL_FALSE);

        if (level < 2)
        {
            Control(GL_DONT_CARE, GL_DEBUG_TYPE_ERROR, GL_DONT_CARE, GL_TRUE);
            Control(GL_DONT_CARE, GL_DONT_CARE, GL_DEBUG_SEVERITY_HIGH, GL_TRUE);
        }
    }
} // namespace render
