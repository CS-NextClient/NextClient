#include "graphics/render/TestQuad.h"
#include "graphics/render/render_test.h"
#include "graphics/render/RendererState.h"

#include <glad/glad.h>

#include "engine.h"
#include "graphics/render/GpuBuffer.h"
#include "graphics/render/render_resources.h"
#include "graphics/render/render_device.h"
#include "common/sys_dll.h"
#include "graphics/render/RenderConfig.h"
#include "graphics/render/shader_manager.h"

namespace render
{
    namespace
    {
        enum
        {
            kUniformTime = 0,
            kRenderTestUniformCount
        };

        constexpr const char* kUniformNames[kRenderTestUniformCount] = {"uTime"};

        // No attribute name table: the shaders declare layout(location = 0), which GLSL 330
        // allows and which removes the bind-before-link discipline the GL 2.x path needs.
        constexpr ProgramDesc kProgram = {"test_quad.vert", "test_quad.frag", nullptr, kUniformNames, kRenderTestUniformCount, nullptr, 0};

        constexpr float kVertices[] = {
            -1.0f,
            -1.0f,
            1.0f,
            -1.0f,
            -1.0f,
            1.0f,
            1.0f,
            1.0f,
        };

    } // namespace

    bool TestQuad::Ensure()
    {
        if (vao_ != 0 && vertices_.is_valid())
        {
            return true;
        }

        if (!vertices_.is_valid() && !vertices_.Create(GL_ARRAY_BUFFER, kVertices, sizeof(kVertices)))
        {
            return false;
        }

        GLint previous_vao = 0;
        glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &previous_vao);

        GLint previous_buffer = 0;
        glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &previous_buffer);

        glGenVertexArrays(1, &vao_);

        if (vao_ != 0)
        {
            glBindVertexArray(vao_);
            glBindBuffer(GL_ARRAY_BUFFER, vertices_.get_name());
            glEnableVertexAttribArray(0);
            glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), nullptr);
        }

        glBindVertexArray(static_cast<GLuint>(previous_vao));
        glBindBuffer(GL_ARRAY_BUFFER, static_cast<GLuint>(previous_buffer));

        return vao_ != 0;
    }

    void TestQuad::Draw(float time)
    {
        const ncl_gl::Program* program = shaders_.Get(kProgram);

        if (program == nullptr || !Ensure())
        {
            return;
        }

        // The stage runs inside the fixed-function frame, so every piece of state this
        // touches is restored: glPushAttrib does not cover buffer or array bindings.
        GLint previous_vao = 0;
        glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &previous_vao);

        GLint previous_buffer = 0;
        glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &previous_buffer);

        GLint previous_program = 0;
        glGetIntegerv(GL_CURRENT_PROGRAM, &previous_program);

        glPushAttrib(GL_ALL_ATTRIB_BITS);

        glDisable(GL_DEPTH_TEST);
        glDisable(GL_CULL_FACE);
        glDisable(GL_ALPHA_TEST);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        glUseProgram(program->prog);
        glUniform1f(program->loc[kUniformTime], time);

        glBindVertexArray(vao_);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

        glUseProgram(static_cast<GLuint>(previous_program));
        glBindVertexArray(static_cast<GLuint>(previous_vao));

        glPopAttrib();

        glBindBuffer(GL_ARRAY_BUFFER, static_cast<GLuint>(previous_buffer));
    }


    void Test_Draw()
    {
        if (g_RenderCfg.test_quad == nullptr || g_RenderCfg.test_quad->value == 0.0f)
        {
            return;
        }

        if (!Device_CheckAvailable())
        {
            return;
        }

        GetRendererState().test_quad.Draw(static_cast<float>(Sys_FloatTime()));
    }
    TestQuad::TestQuad(RenderDevice& device, ShaderManager& shaders) :
        device_(device),
        shaders_(shaders),
        vertices_(device)
    {
        device_.Register(this);
    }

    TestQuad::~TestQuad()
    {
        device_.Unregister(this);
    }

    void TestQuad::OnDeviceLost()
    {
        vao_ = 0;
    }

    void TestQuad::Clear()
    {
        if (vao_ != 0 && device_.IsContextCurrent())
        {
            glDeleteVertexArrays(1, &vao_);
        }
        vao_ = 0;
        vertices_.Destroy();
    }
} // namespace render
