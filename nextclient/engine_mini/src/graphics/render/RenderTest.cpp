#include "engine.h"

#include <glad/glad.h>
#include "graphics/render/GpuBuffer.h"
#include "graphics/render/RenderDevice.h"

#include "common/sys_dll.h"
#include "graphics/render/RenderConfig.h"
#include "graphics/render/RenderTest.h"
#include "graphics/render/ShaderManager.h"

namespace
{
    enum
    {
        kUniformTime = 0,
        kUniformCount
    };

    constexpr const char* kUniformNames[kUniformCount] = {"uTime"};

    // No attribute name table: the shaders declare layout(location = 0), which GLSL 330
    // allows and which removes the bind-before-link discipline the GL 2.x path needs.
    constexpr render::ProgramDesc kProgram = {
        "test_quad.vert",
        "test_quad.frag",
        nullptr,
        kUniformNames,
        kUniformCount,
        nullptr,
        0
    };

    constexpr float kVertices[] = {
        -1.0f, -1.0f,
         1.0f, -1.0f,
        -1.0f,  1.0f,
         1.0f,  1.0f,
    };

    class TestQuad : public render::IDeviceResource
    {
    public:
        TestQuad()
        {
            render::Device_Register(this);
        }

        ~TestQuad() override
        {
            if (vao_ != 0 && render::Device_ContextAlive())
            {
                glDeleteVertexArrays(1, &vao_);
            }

            render::Device_Unregister(this);
        }

        TestQuad(const TestQuad&) = delete;
        TestQuad& operator=(const TestQuad&) = delete;

        void Draw();

        void OnDeviceLost() override
        {
            vao_ = 0;
        }

    private:
        bool Ensure();

        render::GpuBuffer vertices_;
        GLuint vao_ = 0;
    };

    bool TestQuad::Ensure()
    {
        if (vao_ != 0 && vertices_.ok())
        {
            return true;
        }

        if (!vertices_.ok() && !vertices_.Create(GL_ARRAY_BUFFER, kVertices, sizeof(kVertices)))
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
            glBindBuffer(GL_ARRAY_BUFFER, vertices_.name());
            glEnableVertexAttribArray(0);
            glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), nullptr);
        }

        glBindVertexArray(static_cast<GLuint>(previous_vao));
        glBindBuffer(GL_ARRAY_BUFFER, static_cast<GLuint>(previous_buffer));

        return vao_ != 0;
    }

    void TestQuad::Draw()
    {
        const ncl_gl::Program* program = render::Shaders_Get(kProgram);
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

        glPushAttrib(GL_ALL_ATTRIB_BITS);

        glDisable(GL_DEPTH_TEST);
        glDisable(GL_CULL_FACE);
        glDisable(GL_ALPHA_TEST);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        glUseProgram(program->prog);
        glUniform1f(program->loc[kUniformTime], static_cast<float>(Sys_FloatTime()));

        glBindVertexArray(vao_);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

        glUseProgram(0);
        glBindVertexArray(static_cast<GLuint>(previous_vao));

        glPopAttrib();

        glBindBuffer(GL_ARRAY_BUFFER, static_cast<GLuint>(previous_buffer));
    }
}

void render::Test_Draw()
{
    if (g_RenderCfg.test_quad == nullptr || g_RenderCfg.test_quad->value == 0.0f)
    {
        return;
    }

    if (!render::Device_Available())
    {
        return;
    }

    static TestQuad quad;
    quad.Draw();
}
