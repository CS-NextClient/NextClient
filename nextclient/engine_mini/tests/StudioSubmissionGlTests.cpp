#include <gtest/gtest.h>

#include "PerformanceTestCore.h"

#include <algorithm>
#include <array>
#include <string>
#include <vector>

#include <glad/glad.h>
#include <ncl_gl/program.h>

#include "RadarGlContext.h"
#include "StudioMaterialProviders.h"
#include "StudioScopeTrace.h"
#include "graphics/render/render_device.h"
#include "graphics/render/StudioSubmit.h"
#include "graphics/render/render_resources.h"

namespace
{
    StudioScopeTrace* g_StudioTrace{};
    PFNGLBUFFERSUBDATAPROC g_StudioBoneUpload{};
    PFNGLUSEPROGRAMPROC g_StudioUseProgram{};
    PFNGLBINDVERTEXARRAYPROC g_StudioBindVao{};
    PFNGLLOADMATRIXFPROC g_StudioLoadMatrix{};
    PFNGLPOPATTRIBPROC g_StudioPopAttrib{};
    PFNGLUNIFORM1FPROC g_StudioUniform{};
    PFNGLBINDTEXTUREPROC g_StudioBindTexture{};
    PFNGLGETINTEGERVPROC g_StudioGetInteger{};
    std::vector<GLint> g_StudioMaterialQueries;
    std::vector<GLint> g_StudioMaterialWrites;

    bool IsMaterialUnit(GLint unit)
    {
        return unit >= GL_TEXTURE0 + render::kFirstMaterialUnit && unit < GL_TEXTURE0 + render::kFirstMaterialUnit + render::kStudioMaterialUnits;
    }

    void APIENTRY ObserveIntegerQuery(GLenum name, GLint* value)
    {
        if (name == GL_TEXTURE_BINDING_2D)
        {
            GLint unit = 0;
            g_StudioGetInteger(GL_ACTIVE_TEXTURE, &unit);
            if (IsMaterialUnit(unit))
            {
                g_StudioMaterialQueries.push_back(unit);
            }
        }
        g_StudioGetInteger(name, value);
    }

    void APIENTRY ObserveBoneUpload(GLenum target, GLintptr offset, GLsizeiptr size, const void* data)
    {
        EXPECT_EQ(g_StudioTrace->current(), performance::PerfStage::StudioBoneUpload);
        g_StudioBoneUpload(target, offset, size, data);
    }

    void APIENTRY ObserveProgramBind(GLuint program)
    {
        EXPECT_EQ(
            g_StudioTrace->current(), program ? performance::PerfStage::StudioProgramBind : performance::PerfStage::StudioRestoreBindings
        );
        g_StudioUseProgram(program);
    }

    void APIENTRY ObserveVaoBind(GLuint vao)
    {
        EXPECT_EQ(g_StudioTrace->current(), vao ? performance::PerfStage::StudioBufferBind : performance::PerfStage::StudioRestoreBindings);
        g_StudioBindVao(vao);
    }

    void APIENTRY ObserveMatrixLoad(const GLfloat* matrix)
    {
        EXPECT_EQ(g_StudioTrace->current(), performance::PerfStage::StudioStateApply);
        g_StudioLoadMatrix(matrix);
    }

    void APIENTRY ObserveAttribRestore()
    {
        EXPECT_EQ(g_StudioTrace->current(), performance::PerfStage::StudioRestoreMatrices);
        g_StudioPopAttrib();
    }

    void APIENTRY ObserveUniform(GLint location, GLfloat value)
    {
        EXPECT_EQ(g_StudioTrace->current(), performance::PerfStage::StudioUniformSetup);
        g_StudioUniform(location, value);
    }

    void APIENTRY ObserveTextureBind(GLenum target, GLuint texture)
    {
        ncl_perf::PerfScopeId_t current = g_StudioTrace->current();
        EXPECT_TRUE(current == performance::PerfStage::StudioTextureSetup || current == performance::PerfStage::StudioRestoreTextures);
        GLint unit = 0;
        g_StudioGetInteger(GL_ACTIVE_TEXTURE, &unit);
        if (IsMaterialUnit(unit))
        {
            g_StudioMaterialWrites.push_back(unit);
        }
        g_StudioBindTexture(target, texture);
    }

    class StudioGlObservers
    {
    public:
        explicit StudioGlObservers(StudioScopeTrace& trace)
        {
            g_StudioTrace = &trace;
            g_StudioBoneUpload = glad_glBufferSubData;
            g_StudioUseProgram = glad_glUseProgram;
            g_StudioBindVao = glad_glBindVertexArray;
            glad_glBufferSubData = ObserveBoneUpload;
            glad_glUseProgram = ObserveProgramBind;
            glad_glBindVertexArray = ObserveVaoBind;
            g_StudioLoadMatrix = glad_glLoadMatrixf;
            g_StudioPopAttrib = glad_glPopAttrib;
            g_StudioUniform = glad_glUniform1f;
            g_StudioBindTexture = glad_glBindTexture;
            glad_glLoadMatrixf = ObserveMatrixLoad;
            glad_glPopAttrib = ObserveAttribRestore;
            glad_glUniform1f = ObserveUniform;
            glad_glBindTexture = ObserveTextureBind;
            g_StudioGetInteger = glad_glGetIntegerv;
            g_StudioMaterialQueries.clear();
            g_StudioMaterialWrites.clear();
            glad_glGetIntegerv = ObserveIntegerQuery;
        }

        ~StudioGlObservers()
        {
            glad_glBufferSubData = g_StudioBoneUpload;
            glad_glUseProgram = g_StudioUseProgram;
            glad_glBindVertexArray = g_StudioBindVao;
            glad_glLoadMatrixf = g_StudioLoadMatrix;
            glad_glPopAttrib = g_StudioPopAttrib;
            glad_glUniform1f = g_StudioUniform;
            glad_glBindTexture = g_StudioBindTexture;
            glad_glGetIntegerv = g_StudioGetInteger;
            g_StudioTrace = nullptr;
        }
    };

    int g_StudioProgramBinds{};
    PFNGLMULTIDRAWELEMENTSPROC g_StudioMultiDraw{};
    std::vector<bool> g_StudioDepthWrites;
    std::vector<bool> g_StudioBlends;

    void APIENTRY ObserveStudioDraw(GLenum mode, const GLsizei* counts, GLenum type, const void* const* offsets, GLsizei count)
    {
        GLboolean depth_write = GL_FALSE;
        glGetBooleanv(GL_DEPTH_WRITEMASK, &depth_write);
        g_StudioDepthWrites.push_back(depth_write != GL_FALSE);
        g_StudioBlends.push_back(glIsEnabled(GL_BLEND) != GL_FALSE);
        g_StudioMultiDraw(mode, counts, type, offsets, count);
    }

    void BindStudioTexture(void*, unsigned int texture)
    {
        glBindTexture(GL_TEXTURE_2D, texture);
    }

    void CountStudioProgramBind(void*)
    {
        g_StudioProgramBinds++;
    }
} // namespace

TEST(StudioSubmissionGlTest, SameProgramUsesIndependentPosesAndUniformsAndRestoresCallerState)
{
    RadarGlContext context;
    std::string error;
    ASSERT_TRUE(context.Initialize(error)) << error;
    ASSERT_TRUE(GLAD_GL_VERSION_3_3);
    render::Device_OnContextCreated();

    GLuint image = 0;
    GLuint framebuffer = 0;
    GLuint vao = 0;
    GLuint buffers[3]{};
    glGenTextures(1, &image);
    glBindTexture(GL_TEXTURE_2D, image);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 32, 16, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glGenFramebuffers(1, &framebuffer);
    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, image, 0);
    ASSERT_EQ(glCheckFramebufferStatus(GL_FRAMEBUFFER), GLenum(GL_FRAMEBUFFER_COMPLETE));
    glViewport(0, 0, 32, 16);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_SCISSOR_TEST);
    glDisable(GL_BLEND);
    glClearColor(0.25f, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();

    const char* vertex =
        "#version 330 compatibility\n"
        "layout(location=0) in vec3 p;\n"
        "layout(std140) uniform Bones {vec4 uBones[384];};\n"
        "void main(){gl_Position=gl_ModelViewProjectionMatrix*vec4(p.x+uBones[0].x,p.yz,1);}";
    const char* fragment =
        "#version 330 compatibility\n"
        "uniform float uAmbient; uniform float uBlend;\n"
        "void main(){gl_FragColor=vec4(uAmbient,uBlend,0,1);}";
    std::string log;
    ncl_gl::Program program{};
    program.prog = ncl_gl::LinkProgram(vertex, fragment, nullptr, 0, &log);
    ASSERT_NE(program.prog, 0u) << log;
    std::fill(std::begin(program.loc), std::end(program.loc), -1);
    program.loc[render::kUniformAmbient] = glGetUniformLocation(program.prog, "uAmbient");
    program.loc[render::kUniformBlend] = glGetUniformLocation(program.prog, "uBlend");
    glUniformBlockBinding(program.prog, glGetUniformBlockIndex(program.prog, "Bones"), render::kStudioBonesBinding);

    const float vertices[] = {-0.35f, -0.8f, 0, 0, 0.8f, 0, 0.35f, -0.8f, 0};
    const unsigned int indices[] = {0, 1, 2};
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);
    glGenBuffers(3, buffers);
    glBindBuffer(GL_ARRAY_BUFFER, buffers[0]);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, nullptr);
    glEnableVertexAttribArray(0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, buffers[1]);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);
    glBindBuffer(GL_UNIFORM_BUFFER, buffers[2]);
    glBufferData(GL_UNIFORM_BUFFER, 16384, nullptr, GL_DYNAMIC_DRAW);
    glBindBufferRange(GL_UNIFORM_BUFFER, 0, buffers[2], 256, 6144);
    glBindVertexArray(0);
    glActiveTexture(GL_TEXTURE0 + render::kFirstMaterialUnit);
    glBindTexture(GL_TEXTURE_2D, image);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, 0);

    {
        render::StudioSubmissionCallbacks callbacks;
        callbacks.bind_texture = BindStudioTexture;
        callbacks.note_program_bind = CountStudioProgramBind;
        StudioProgramSourceFixture programs;
        StudioMaterialSourceFixture materials;
        performance_test::Core runtime_performance;
        render::StudioMaterialRuntime runtime(programs.manager, materials.catalog, materials.cache, runtime_performance.monitor);
        render::StudioGlSubmission sink(
            render::GetResources().device,
            runtime,
            callbacks,
            runtime_performance.monitor,
            runtime_performance.mapper,
            runtime_performance.studio_depth
        );
        ASSERT_TRUE(sink.EnsureBones());
        render::StudioDrawQueue queue(sink);
        StudioScopeTrace trace(runtime_performance.monitor);
        render::StudioDrawPacket first;
        render::Studio_CaptureRasterState(first, runtime_performance.monitor);
        first.vao = vao;
        first.bones[0] = -0.5f;
        first.lighting.ambient = 1;
        first.blend = 0;
        first.meshes.emplace_back();
        first.meshes[0].program = &program;
        render::StudioMaterialBindings bindings;
        runtime.PrepareBindings(bindings, program, nullptr);
        first.meshes[0].material_bindings = &bindings;
        first.meshes[0].index_count = 3;
        first.meshes[0].additive = true;
        render::StudioDrawPacket second = first;
        second.bones[0] = 0.5f;
        second.lighting.ambient = 0;
        second.blend = 1;
        second.meshes[0].additive = false;

        glEnable(GL_BLEND);
        glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ZERO);
        glDepthMask(GL_FALSE);
        glMatrixMode(GL_MODELVIEW);
        glTranslatef(9, 8, 7);
        g_StudioProgramBinds = 0;
        g_StudioDepthWrites.clear();
        g_StudioBlends.clear();
        g_StudioMultiDraw = glad_glMultiDrawElements;
        glad_glMultiDrawElements = ObserveStudioDraw;
        PreparePacketImages(runtime, first);
        PreparePacketImages(runtime, second);
        {
            StudioGlObservers observers(trace);
            queue.BeginOpaquePass(true);
            queue.Submit(std::move(first));
            queue.Submit(std::move(second));
            queue.EndOpaquePass();
        }
        glad_glMultiDrawElements = g_StudioMultiDraw;

        EXPECT_EQ(trace.calls[performance::PerfStage::StudioGlSave], 2u);
        EXPECT_EQ(trace.calls[performance::PerfStage::StudioGlRestore], 1u);
        EXPECT_EQ(trace.calls[performance::PerfStage::StudioProgramBind], 2u);
        EXPECT_EQ(trace.calls[performance::PerfStage::StudioBufferBind], 2u);
        EXPECT_EQ(trace.calls[performance::PerfStage::StudioBoneUpload], 2u);
        EXPECT_EQ(trace.calls[performance::PerfStage::StudioModelStateCapture], 1u);
        EXPECT_EQ(trace.calls[performance::PerfStage::StudioBatchStateSave], 1u);
        EXPECT_EQ(trace.calls[performance::PerfStage::StudioRestoreBindings], 2u);
        EXPECT_EQ(trace.calls[performance::PerfStage::StudioRestoreMatrices], 1u);
        EXPECT_EQ(trace.calls[performance::PerfStage::StudioRestoreTextures], 1u);
        EXPECT_GT(trace.calls[performance::PerfStage::StudioDataPrepare], 0u);
        EXPECT_EQ(trace.calls[performance::PerfStage::StudioMeshSort], 0u);
        EXPECT_EQ(trace.calls[performance::PerfStage::StudioStateApply], 4u);
        EXPECT_EQ(trace.calls[performance::PerfStage::StudioUniformSetup], 4u);
        EXPECT_EQ(trace.calls[performance::PerfStage::StudioTextureSetup], 4u);
        EXPECT_EQ(trace.parents[performance::PerfStage::StudioModelStateCapture], performance::PerfStage::StudioGlSave);
        EXPECT_EQ(trace.parents[performance::PerfStage::StudioBatchStateSave], performance::PerfStage::StudioGlSave);
        EXPECT_EQ(trace.parents[performance::PerfStage::StudioRestoreBindings], performance::PerfStage::StudioGlRestore);
        EXPECT_EQ(trace.parents[performance::PerfStage::StudioRestoreMatrices], performance::PerfStage::StudioGlRestore);
        EXPECT_EQ(trace.parents[performance::PerfStage::StudioRestoreTextures], performance::PerfStage::StudioGlRestore);
        EXPECT_EQ(g_StudioProgramBinds, 2);
        EXPECT_TRUE(g_StudioMaterialQueries.empty());
        EXPECT_TRUE(g_StudioMaterialWrites.empty());
        EXPECT_EQ(g_StudioDepthWrites, (std::vector<bool>{false, true}));
        EXPECT_EQ(g_StudioBlends, (std::vector<bool>{true, false}));
        GLint value = 0;
        glGetIntegerv(GL_CURRENT_PROGRAM, &value);
        EXPECT_EQ(value, 0);
        glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &value);
        EXPECT_EQ(value, 0);
        glGetIntegeri_v(GL_UNIFORM_BUFFER_BINDING, 0, &value);
        EXPECT_EQ(value, GLint(buffers[2]));
        GLint64 range_start = 0;
        glGetInteger64i_v(GL_UNIFORM_BUFFER_START, 0, &range_start);
        EXPECT_EQ(range_start, 256);
        glGetIntegerv(GL_DEPTH_WRITEMASK, &value);
        EXPECT_EQ(value, GL_FALSE);
        EXPECT_TRUE(glIsEnabled(GL_BLEND));
        glGetIntegerv(GL_BLEND_SRC_RGB, &value);
        EXPECT_EQ(value, GL_SRC_ALPHA);
        glGetIntegerv(GL_MATRIX_MODE, &value);
        EXPECT_EQ(value, GL_MODELVIEW);
        std::array<float, 16> matrix{};
        glGetFloatv(GL_MODELVIEW_MATRIX, matrix.data());
        EXPECT_EQ(matrix[12], 9);
        EXPECT_EQ(matrix[13], 8);
        glActiveTexture(GL_TEXTURE0 + render::kFirstMaterialUnit);
        glGetIntegerv(GL_TEXTURE_BINDING_2D, &value);
        EXPECT_EQ(value, GLint(image));
        glActiveTexture(GL_TEXTURE0);

        unsigned char left[4]{};
        unsigned char right[4]{};
        glReadPixels(8, 5, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, left);
        glReadPixels(24, 5, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, right);
        EXPECT_EQ(left[0], 255);
        EXPECT_EQ(left[1], 0);
        EXPECT_EQ(right[0], 0);
        EXPECT_EQ(right[1], 255);
        EXPECT_EQ(glGetError(), GLenum(GL_NO_ERROR));
    }

    glBindBufferBase(GL_UNIFORM_BUFFER, 0, 0);
    glDeleteBuffers(3, buffers);
    glDeleteVertexArrays(1, &vao);
    glDeleteProgram(program.prog);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glDeleteFramebuffers(1, &framebuffer);
    glDeleteTextures(1, &image);
    render::Device_OnContextDestroyed();
}
