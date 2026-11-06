#include <gtest/gtest.h>

#include "PerformanceTestCore.h"

#include <algorithm>
#include <array>
#include <string>
#include <utility>
#include <vector>

#include <glad/glad.h>
#include <ncl_gl/program.h>

#include "RadarGlContext.h"
#include "StudioMaterialProviders.h"
#include "StudioTestPose.h"
#include "common/memory/ScratchArena.h"
#include "common/memory/g_scratch_allocator.h"
#include "graphics/render/render_device.h"
#include "graphics/render/StudioSubmit.h"
#include "graphics/render/render_resources.h"

namespace
{
    PFNGLBUFFERSUBDATAPROC g_OriginalUpload{};
    PFNGLMULTIDRAWELEMENTSPROC g_OriginalDraw{};
    PFNGLPUSHATTRIBPROC g_OriginalStateSave{};
    PFNGLPOPATTRIBPROC g_OriginalStateRestore{};
    int g_StateSaves{};
    int g_StateRestores{};
    std::vector<GLsizeiptr> g_UploadSizes;
    std::vector<float> g_UploadTails;
    std::vector<uintptr_t> g_DrawOffsets;
    std::vector<bool> g_DrawBlends;
    std::vector<bool> g_DrawDepthWrites;

    void APIENTRY ObserveStateSave(GLbitfield mask)
    {
        g_StateSaves++;
        g_OriginalStateSave(mask);
    }

    void APIENTRY ObserveStateRestore()
    {
        g_StateRestores++;
        g_OriginalStateRestore();
    }

    void APIENTRY ObserveUpload(GLenum target, GLintptr offset, GLsizeiptr size, const void* data)
    {
        g_UploadSizes.push_back(size);
        g_OriginalUpload(target, offset, size, data);
        if (size == 6144)
        {
            float tail = 0;
            glGetBufferSubData(GL_UNIFORM_BUFFER, 6140, sizeof(tail), &tail);
            g_UploadTails.push_back(tail);
        }
    }

    void APIENTRY ObserveDraw(GLenum mode, const GLsizei* counts, GLenum type, const void* const* offsets, GLsizei count)
    {
        for (GLsizei index = 0; index < count; ++index)
        {
            g_DrawOffsets.push_back(reinterpret_cast<uintptr_t>(offsets[index]));
        }
        g_DrawBlends.push_back(glIsEnabled(GL_BLEND) != GL_FALSE);
        GLboolean depth_write = GL_FALSE;
        glGetBooleanv(GL_DEPTH_WRITEMASK, &depth_write);
        g_DrawDepthWrites.push_back(depth_write != GL_FALSE);
        g_OriginalDraw(mode, counts, type, offsets, count);
    }

    void BindBaseTexture(void*, unsigned int texture)
    {
        glBindTexture(GL_TEXTURE_2D, texture);
    }

    struct SubmissionStateCallCounts
    {
        std::vector<GLenum> capabilities;
        std::vector<unsigned int> base_textures;
        int blend_functions{};
        int blend_equations{};
        int depth_masks{};
        int depth_functions{};
        int depth_ranges{};
        int cull_faces{};
        int alpha_functions{};
        int program_binds{};
    };

    SubmissionStateCallCounts g_SubmissionStateCalls;
    PFNGLENABLEPROC g_CacheOriginalEnable{};
    PFNGLDISABLEPROC g_CacheOriginalDisable{};
    PFNGLBLENDFUNCSEPARATEPROC g_CacheOriginalBlendFuncSeparate{};
    PFNGLBLENDFUNCPROC g_CacheOriginalBlendFunc{};
    PFNGLBLENDEQUATIONSEPARATEPROC g_CacheOriginalBlendEquationSeparate{};
    PFNGLDEPTHMASKPROC g_CacheOriginalDepthMask{};
    PFNGLDEPTHFUNCPROC g_CacheOriginalDepthFunc{};
    PFNGLDEPTHRANGEPROC g_CacheOriginalDepthRange{};
    PFNGLCULLFACEPROC g_CacheOriginalCullFace{};
    PFNGLALPHAFUNCPROC g_CacheOriginalAlphaFunc{};
    PFNGLUSEPROGRAMPROC g_CacheOriginalUseProgram{};

    void APIENTRY ObserveCacheEnable(GLenum cap)
    {
        g_SubmissionStateCalls.capabilities.push_back(cap);
        g_CacheOriginalEnable(cap);
    }

    void APIENTRY ObserveCacheDisable(GLenum cap)
    {
        g_SubmissionStateCalls.capabilities.push_back(cap);
        g_CacheOriginalDisable(cap);
    }

    void APIENTRY ObserveCacheBlendFuncSeparate(GLenum sr, GLenum dr, GLenum sa, GLenum da)
    {
        g_SubmissionStateCalls.blend_functions++;
        g_CacheOriginalBlendFuncSeparate(sr, dr, sa, da);
    }

    void APIENTRY ObserveCacheBlendFunc(GLenum src, GLenum dst)
    {
        g_SubmissionStateCalls.blend_functions++;
        g_CacheOriginalBlendFunc(src, dst);
    }

    void APIENTRY ObserveCacheBlendEquationSeparate(GLenum rgb, GLenum alpha)
    {
        g_SubmissionStateCalls.blend_equations++;
        g_CacheOriginalBlendEquationSeparate(rgb, alpha);
    }

    void APIENTRY ObserveCacheDepthMask(GLboolean enabled)
    {
        g_SubmissionStateCalls.depth_masks++;
        g_CacheOriginalDepthMask(enabled);
    }

    void APIENTRY ObserveCacheDepthFunc(GLenum func)
    {
        g_SubmissionStateCalls.depth_functions++;
        g_CacheOriginalDepthFunc(func);
    }

    void APIENTRY ObserveCacheDepthRange(GLdouble near_value, GLdouble far_value)
    {
        g_SubmissionStateCalls.depth_ranges++;
        g_CacheOriginalDepthRange(near_value, far_value);
    }

    void APIENTRY ObserveCacheCullFace(GLenum face)
    {
        g_SubmissionStateCalls.cull_faces++;
        g_CacheOriginalCullFace(face);
    }

    void APIENTRY ObserveCacheAlphaFunc(GLenum func, GLfloat ref)
    {
        g_SubmissionStateCalls.alpha_functions++;
        g_CacheOriginalAlphaFunc(func, ref);
    }

    void APIENTRY ObserveCacheUseProgram(GLuint program)
    {
        g_SubmissionStateCalls.program_binds++;
        g_CacheOriginalUseProgram(program);
    }

    void ObserveCachedBaseTexture(void*, unsigned int texture)
    {
        g_SubmissionStateCalls.base_textures.push_back(texture);
        BindBaseTexture(nullptr, texture);
    }

    class SubmissionStateObservers
    {
    public:
        SubmissionStateObservers()
        {
            g_SubmissionStateCalls = {};
            g_CacheOriginalEnable = std::exchange(glad_glEnable, ObserveCacheEnable);
            g_CacheOriginalDisable = std::exchange(glad_glDisable, ObserveCacheDisable);
            g_CacheOriginalBlendFuncSeparate = std::exchange(glad_glBlendFuncSeparate, ObserveCacheBlendFuncSeparate);
            g_CacheOriginalBlendFunc = std::exchange(glad_glBlendFunc, ObserveCacheBlendFunc);
            g_CacheOriginalBlendEquationSeparate = std::exchange(glad_glBlendEquationSeparate, ObserveCacheBlendEquationSeparate);
            g_CacheOriginalDepthMask = std::exchange(glad_glDepthMask, ObserveCacheDepthMask);
            g_CacheOriginalDepthFunc = std::exchange(glad_glDepthFunc, ObserveCacheDepthFunc);
            g_CacheOriginalDepthRange = std::exchange(glad_glDepthRange, ObserveCacheDepthRange);
            g_CacheOriginalCullFace = std::exchange(glad_glCullFace, ObserveCacheCullFace);
            g_CacheOriginalAlphaFunc = std::exchange(glad_glAlphaFunc, ObserveCacheAlphaFunc);
            g_CacheOriginalUseProgram = std::exchange(glad_glUseProgram, ObserveCacheUseProgram);
        }

        ~SubmissionStateObservers()
        {
            glad_glEnable = g_CacheOriginalEnable;
            glad_glDisable = g_CacheOriginalDisable;
            glad_glBlendFuncSeparate = g_CacheOriginalBlendFuncSeparate;
            glad_glBlendFunc = g_CacheOriginalBlendFunc;
            glad_glBlendEquationSeparate = g_CacheOriginalBlendEquationSeparate;
            glad_glDepthMask = g_CacheOriginalDepthMask;
            glad_glDepthFunc = g_CacheOriginalDepthFunc;
            glad_glDepthRange = g_CacheOriginalDepthRange;
            glad_glCullFace = g_CacheOriginalCullFace;
            glad_glAlphaFunc = g_CacheOriginalAlphaFunc;
            glad_glUseProgram = g_CacheOriginalUseProgram;
        }
    };

    class StudioOptimizationMaterialSource : public StudioMaterialSourceFixture
    {
    public:
        GLuint texture{};
        bool scratch_restored{};
        std::vector<GLint> load_units;
        std::vector<std::string> load_paths;

        StudioOptimizationMaterialSource()
        {
            ncl_utils::ResultT<ncl_material::ProgramManifest> manifest = ncl_material::ParseProgramManifest(R"({
                "version": 1, "program": "custom", "parameters": {
                    "uFixtureImage": {"type": "texture"},
                    "uFixtureLayer": {"type": "texture", "default": "fixture/default.png"}
                }
            })");

            manifests.emplace("custom", std::move(*manifest));
        }

        GLuint Load(const char* path, tex::TextureLoadGuard) override
        {
            if (path == nullptr || *path == '\0')
            {
                return 0;
            }

            GLint active = 0;
            glGetIntegerv(GL_ACTIVE_TEXTURE, &active);
            load_units.push_back(active);
            load_paths.emplace_back(path);
            size_t before = g_ScratchAllocator.GetOffset();

            {
                ScratchArena texture_arena(g_ScratchAllocator);
                uint8_t* pixels = texture_arena.AllocateArray<uint8_t>(4096);
                std::fill_n(pixels, 4096, uint8_t{0xa5});
            }

            scratch_restored = g_ScratchAllocator.GetOffset() == before;
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, texture);

            return texture;
        }

        GLuint CreateNeutral() override
        {
            return texture;
        }
    };

    class StudioOptimizationGlTest : public testing::Test
    {
    private:
        RadarGlContext context_;
        ncl_gl::Program program_{};
        GLuint vao_{};
        GLuint buffers_[2]{};
        GLuint framebuffer_{};
        GLuint image_{};
        std::array<GLuint, 10> textures_{};
        bool initialized_{};

    protected:
        render::StudioMaterialBindings bindings_;
        StudioProgramSourceFixture program_source_;
        StudioOptimizationMaterialSource material_source_;

        void SetUp() override
        {
            std::string error;
            ASSERT_TRUE(context_.Initialize(error)) << error;
            ASSERT_TRUE(GLAD_GL_VERSION_3_3);
            render::Device_OnContextCreated();
            program_source_.device.OnContextCreated();
            material_source_.device.OnContextCreated();
            initialized_ = true;
            g_OriginalUpload = glad_glBufferSubData;
            g_OriginalDraw = glad_glMultiDrawElements;
            g_OriginalStateSave = glad_glPushAttrib;
            g_OriginalStateRestore = glad_glPopAttrib;
            glad_glPushAttrib = ObserveStateSave;
            glad_glPopAttrib = ObserveStateRestore;
            g_UploadSizes.clear();
            g_UploadTails.clear();
            g_DrawOffsets.clear();
            g_DrawBlends.clear();
            g_DrawDepthWrites.clear();

            const char* vertex =
                "#version 330 compatibility\n"
                "layout(location=0) in vec3 p;\n"
                "layout(std140) uniform Bones {vec4 b[384];};\n"
                "void main(){gl_Position=gl_ModelViewProjectionMatrix*vec4(p.x+b[0].x,p.yz,1);}";
            const char* fragment =
                "#version 330 compatibility\n"
                "uniform sampler2D uFixtureImage; uniform sampler2D uFixtureLayer;\n"
                "void main(){gl_FragColor=vec4(1,0,0,1)+1e-8*(texture(uFixtureImage,vec2(0.5))"
                "+texture(uFixtureLayer,vec2(0.5)));}";
            std::string log;
            program_.prog = ncl_gl::LinkProgram(vertex, fragment, nullptr, 0, &log);
            ASSERT_NE(program_.prog, 0u) << log;
            std::fill(std::begin(program_.loc), std::end(program_.loc), -1);
            glUniformBlockBinding(program_.prog, glGetUniformBlockIndex(program_.prog, "Bones"), render::kStudioBonesBinding);

            glGenVertexArrays(1, &vao_);
            glBindVertexArray(vao_);
            glGenBuffers(2, buffers_);
            const float vertices[] = {-0.3f, -0.6f, 0, 0, 0.6f, 0, 0.3f, -0.6f, 0};
            const unsigned int indices[] = {0, 1, 2, 0, 1, 2, 0, 1, 2, 0, 1, 2, 0, 1, 2};
            glBindBuffer(GL_ARRAY_BUFFER, buffers_[0]);
            glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
            glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, nullptr);
            glEnableVertexAttribArray(0);
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, buffers_[1]);
            glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);
            glBindVertexArray(0);

            glGenTextures(1, &image_);
            glBindTexture(GL_TEXTURE_2D, image_);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 32, 16, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
            glGenFramebuffers(1, &framebuffer_);
            glBindFramebuffer(GL_FRAMEBUFFER, framebuffer_);
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, image_, 0);
            ASSERT_EQ(glCheckFramebufferStatus(GL_FRAMEBUFFER), GLenum(GL_FRAMEBUFFER_COMPLETE));
            glViewport(0, 0, 32, 16);
            glDisable(GL_DEPTH_TEST);
            glDisable(GL_BLEND);
            glDisable(GL_SCISSOR_TEST);
            glClearColor(0, 0, 0, 1);
            glMatrixMode(GL_MODELVIEW);
            glLoadIdentity();
            glMatrixMode(GL_PROJECTION);
            glLoadIdentity();
            glGenTextures(static_cast<GLsizei>(textures_.size()), textures_.data());
            for (int slot = 0; slot < render::kStudioMaterialUnits; ++slot)
            {
                glActiveTexture(GL_TEXTURE0 + render::kFirstMaterialUnit + slot);
                glBindTexture(GL_TEXTURE_2D, textures_[slot]);
            }
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, textures_[8]);
            material_source_.texture = textures_[9];
            material_source_.scratch_restored = false;
            material_source_.load_units.clear();
            material_source_.load_paths.clear();
            glBindTexture(GL_TEXTURE_2D, material_source_.texture);
            const uint8_t white[] = {255, 255, 255, 255};
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, white);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
            glBindTexture(GL_TEXTURE_2D, textures_[8]);
            glad_glBufferSubData = ObserveUpload;
            glad_glMultiDrawElements = ObserveDraw;
        }

        void TearDown() override
        {
            if (!initialized_)
            {
                return;
            }
            glad_glBufferSubData = g_OriginalUpload;
            glad_glMultiDrawElements = g_OriginalDraw;
            glad_glPushAttrib = g_OriginalStateSave;
            glad_glPopAttrib = g_OriginalStateRestore;
            glBindBufferBase(GL_UNIFORM_BUFFER, render::kStudioBonesBinding, 0);
            glDeleteBuffers(2, buffers_);
            glDeleteVertexArrays(1, &vao_);
            glDeleteProgram(program_.prog);
            glDeleteFramebuffers(1, &framebuffer_);
            glDeleteTextures(1, &image_);
            glDeleteTextures(static_cast<GLsizei>(textures_.size()), textures_.data());
            EXPECT_EQ(glGetError(), GLenum(GL_NO_ERROR));
            render::Device_OnContextDestroyed();
        }

        render::StudioDrawPacket MakePacket(render::StudioMaterialRuntime& runtime, performance::PerformanceMonitor& monitor, int bones = 1)
        {
            render::StudioDrawPacket packet{.bones = CapturePose()};
            render::Studio_CaptureRasterState(packet, monitor);
            packet.vao = vao_;
            packet.bone_count = bones;
            packet.meshes.emplace_back();
            packet.meshes[0].program = &program_;
            runtime.PrepareBindings(bindings_, program_, nullptr);
            packet.meshes[0].material_bindings = &bindings_;
            packet.meshes[0].index_count = 3;
            return packet;
        }

        void ExpectRedPixel(int x)
        {
            std::array<uint8_t, 4> pixel{};
            glReadPixels(x, 8, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixel.data());
            EXPECT_EQ(pixel, (std::array<uint8_t, 4>{255, 0, 0, 255}));
        }

        void ExpectOriginalBindings()
        {
            for (int slot = 0; slot < render::kStudioMaterialUnits; ++slot)
            {
                glActiveTexture(GL_TEXTURE0 + render::kFirstMaterialUnit + slot);
                GLint texture = 0;
                glGetIntegerv(GL_TEXTURE_BINDING_2D, &texture);
                EXPECT_EQ(texture, static_cast<GLint>(textures_[slot]));
            }
            glActiveTexture(GL_TEXTURE0);
            GLint texture = 0;
            glGetIntegerv(GL_TEXTURE_BINDING_2D, &texture);
            EXPECT_EQ(texture, static_cast<GLint>(textures_[8]));
        }
    };
} // namespace

TEST_F(StudioOptimizationGlTest, SmallAndLargePosesKeepTheirPixelsAndUploadOnlyTheirBones)
{
    performance_test::Core runtime_performance;
    render::StudioMaterialRuntime runtime(
        program_source_.manager, material_source_.catalog, material_source_.cache, runtime_performance.monitor
    );
    render::StudioGlSubmission sink(
        render::GetResources().device,
        runtime,
        {BindBaseTexture, nullptr},
        runtime_performance.monitor,
        runtime_performance.mapper,
        runtime_performance.studio_depth
    );
    ASSERT_TRUE(sink.EnsureBones());
    for (int bones : {1, 128, 1})
    {
        render::StudioDrawPacket packet = MakePacket(runtime, runtime_performance.monitor, bones);
        packet.bones = CapturePose({{0, bones == 128 ? 0.5f : -0.5f}}, packet.bones);
        glClear(GL_COLOR_BUFFER_BIT);
        size_t before = g_ScratchAllocator.GetOffset();
        PreparePacketImages(runtime, packet);
        sink.Begin();
        sink.Draw(packet);
        sink.End();
        EXPECT_EQ(g_ScratchAllocator.GetOffset(), before);
        ExpectRedPixel(bones == 128 ? 24 : 8);
    }
    EXPECT_EQ(g_UploadSizes, (std::vector<GLsizeiptr>{48, 6144, 48}));
}

TEST_F(StudioOptimizationGlTest, AuthoredMaterialKeepsFullPoseAndRestoresBindingsAfterNestedTextureWork)
{
    ScratchArena outer(g_ScratchAllocator);
    uint8_t* sentinel = outer.AllocateArray<uint8_t>(1024);
    std::fill_n(sentinel, 1024, uint8_t{0x3c});
    size_t before = g_ScratchAllocator.GetOffset();
    performance_test::Core runtime_performance;
    render::StudioMaterialRuntime runtime(
        program_source_.manager, material_source_.catalog, material_source_.cache, runtime_performance.monitor
    );
    render::StudioGlSubmission sink(
        render::GetResources().device,
        runtime,
        {BindBaseTexture, nullptr},
        runtime_performance.monitor,
        runtime_performance.mapper,
        runtime_performance.studio_depth
    );
    ASSERT_TRUE(sink.EnsureBones());
    render::StudioDrawPacket packet = MakePacket(runtime, runtime_performance.monitor);
    packet.bones = CapturePose({{render::kStudioBoneFloats - 1, 17.0f}}, packet.bones);
    packet.meshes[0].material.emplace();
    packet.meshes[0].material->program = "custom";
    ncl_material::ParamValue image;
    image.type = ncl_material::ParamType::TextureRef;
    image.texture_ref = "fixture/image.png";
    packet.meshes[0].material->params.push_back({"uFixtureImage", image});
    runtime.PrepareBindings(bindings_, *packet.meshes[0].program, &*packet.meshes[0].material);
    glClear(GL_COLOR_BUFFER_BIT);
    PreparePacketImages(runtime, packet);
    sink.Begin();
    sink.Draw(packet);
    packet.meshes[0].material.reset();
    sink.Draw(packet);
    sink.End();
    EXPECT_EQ(g_UploadSizes, (std::vector<GLsizeiptr>{6144, 48}));
    EXPECT_EQ(g_UploadTails, (std::vector<float>{17}));
    EXPECT_TRUE(material_source_.scratch_restored);
    EXPECT_EQ(material_source_.load_units, (std::vector<GLint>{GL_TEXTURE0, GL_TEXTURE0}));
    EXPECT_EQ(material_source_.load_paths, (std::vector<std::string>{"fixture/image.png", "fixture/default.png"}));
    EXPECT_EQ(g_ScratchAllocator.GetOffset(), before);
    EXPECT_TRUE(std::all_of(sentinel, sentinel + 1024, [](uint8_t value) { return value == 0x3c; }));
    ExpectOriginalBindings();
    ExpectRedPixel(16);
}

TEST_F(StudioOptimizationGlTest, ExhaustedScratchFallsBackToOwnedArraysWithoutChangingOuterMemory)
{
    ScratchArena outer(g_ScratchAllocator);
    size_t bytes = g_ScratchAllocator.GetCapacity() - g_ScratchAllocator.GetOffset() - 16;
    uint8_t* sentinel = outer.AllocateArray<uint8_t>(bytes);
    sentinel[0] = 23;
    sentinel[bytes - 1] = 41;
    size_t before = g_ScratchAllocator.GetOffset();
    performance_test::Core runtime_performance;
    render::StudioMaterialRuntime runtime(
        program_source_.manager, material_source_.catalog, material_source_.cache, runtime_performance.monitor
    );
    render::StudioGlSubmission sink(
        render::GetResources().device,
        runtime,
        {BindBaseTexture, nullptr},
        runtime_performance.monitor,
        runtime_performance.mapper,
        runtime_performance.studio_depth
    );
    ASSERT_TRUE(sink.EnsureBones());
    render::StudioDrawPacket packet = MakePacket(runtime, runtime_performance.monitor);
    glClear(GL_COLOR_BUFFER_BIT);
    PreparePacketImages(runtime, packet);
    sink.Begin();
    sink.Draw(packet);
    sink.End();
    EXPECT_EQ(g_ScratchAllocator.GetOffset(), before);
    EXPECT_EQ(sentinel[0], 23);
    EXPECT_EQ(sentinel[bytes - 1], 41);
    ExpectRedPixel(16);
}

TEST_F(StudioOptimizationGlTest, MeshesKeepOriginalOrderAcrossSkinsAndAdditiveModes)
{
    performance_test::Core runtime_performance;
    render::StudioMaterialRuntime runtime(
        program_source_.manager, material_source_.catalog, material_source_.cache, runtime_performance.monitor
    );
    render::StudioGlSubmission sink(
        render::GetResources().device,
        runtime,
        {BindBaseTexture, nullptr},
        runtime_performance.monitor,
        runtime_performance.mapper,
        runtime_performance.studio_depth
    );
    ASSERT_TRUE(sink.EnsureBones());
    render::StudioDrawPacket packet = MakePacket(runtime, runtime_performance.monitor);
    packet.meshes.resize(5, packet.meshes[0]);
    packet.raster.depth_test = true;
    packet.raster.depth_write = true;
    packet.raster.depth_func = GL_LEQUAL;
    const int skins[] = {5, 2, 1, 1, 0};
    for (size_t index = 0; index < packet.meshes.size(); ++index)
    {
        packet.meshes[index].skin = skins[index];
        packet.meshes[index].index_offset = index * 12;
        packet.meshes[index].additive = index == 0 || index == 4;
        packet.meshes[index].chrome = index == 3;
    }
    for (bool authored : {false, true})
    {
        if (authored)
        {
            packet.meshes[0].material.emplace();
        }
        g_DrawOffsets.clear();
        PreparePacketImages(runtime, packet);
        sink.Begin();
        sink.Draw(packet);
        sink.End();
        EXPECT_EQ(g_DrawOffsets, (std::vector<uintptr_t>{0, 12, 24, 36, 48}));
    }
}

TEST(StudioMaterialStateTest, CapturesEachTouchedSlotOnceAndLeavesOtherSlotsAlone)
{
    RadarGlContext context;
    std::string error;
    ASSERT_TRUE(context.Initialize(error)) << error;
    ASSERT_TRUE(GLAD_GL_VERSION_3_3);
    std::array<GLuint, 18> textures{};
    glGenTextures(static_cast<GLsizei>(textures.size()), textures.data());
    for (int unit = 0; unit < 16; ++unit)
    {
        glActiveTexture(GL_TEXTURE0 + unit);
        glBindTexture(GL_TEXTURE_2D, textures[unit]);
    }

    render::StudioMaterialState state;
    performance_test::Core state_performance;
    state.BindTexture(0, textures[16], state_performance.monitor);
    state.BindTexture(0, textures[17], state_performance.monitor);
    state.BindTexture(render::kStudioMaterialUnits - 1, textures[16], state_performance.monitor);
    glActiveTexture(GL_TEXTURE0 + render::kFirstMaterialUnit + 2);
    glBindTexture(GL_TEXTURE_2D, 0);
    state.Restore();

    GLint binding = 0;
    for (int unit = 0; unit < 16; ++unit)
    {
        glActiveTexture(GL_TEXTURE0 + unit);
        glGetIntegerv(GL_TEXTURE_BINDING_2D, &binding);
        const GLint expected = unit == render::kFirstMaterialUnit + 2 ? 0 : static_cast<GLint>(textures[unit]);
        EXPECT_EQ(binding, expected) << unit;
    }

    glActiveTexture(GL_TEXTURE0 + render::kFirstMaterialUnit);
    glBindTexture(GL_TEXTURE_2D, textures[17]);
    state.Restore();
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &binding);
    EXPECT_EQ(binding, static_cast<GLint>(textures[17]));
    glDeleteTextures(static_cast<GLsizei>(textures.size()), textures.data());
    EXPECT_EQ(glGetError(), GLenum(GL_NO_ERROR));
}
TEST_F(StudioOptimizationGlTest, AlphaBlendedMeshesKeepTheirPixelsInImmediateAndDeferredDraws)
{
    const char* vertex =
        "#version 330 compatibility\n"
        "layout(location=0) in vec3 p;\n"
        "void main(){gl_Position=gl_ModelViewProjectionMatrix*vec4(p,1);}";
    const char* fragment =
        "#version 330 compatibility\n"
        "uniform sampler2D image;\n"
        "void main(){gl_FragColor=texture2D(image,vec2(0.5));}";
    std::string log;
    ncl_gl::Program program{};
    program.prog = ncl_gl::LinkProgram(vertex, fragment, nullptr, 0, &log);
    ASSERT_NE(program.prog, 0u) << log;
    std::fill(std::begin(program.loc), std::end(program.loc), -1);

    GLuint textures[2]{};
    glGenTextures(2, textures);
    const uint8_t colors[2][4] = {{255, 0, 0, 128}, {0, 0, 255, 128}};
    for (int index = 0; index < 2; ++index)
    {
        glBindTexture(GL_TEXTURE_2D, textures[index]);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, colors[index]);
    }

    {
        performance_test::Core runtime_performance;
        render::StudioMaterialRuntime runtime(
            program_source_.manager, material_source_.catalog, material_source_.cache, runtime_performance.monitor
        );
        render::StudioGlSubmission sink(
            render::GetResources().device,
            runtime,
            {BindBaseTexture, nullptr},
            runtime_performance.monitor,
            runtime_performance.mapper,
            runtime_performance.studio_depth
        );
        ASSERT_TRUE(sink.EnsureBones());
        render::StudioDrawQueue queue(sink);

        for (bool additive : {false, true})
        {
            for (bool deferred : {false, true})
            {
                render::StudioDrawPacket packet = MakePacket(runtime, runtime_performance.monitor);
                packet.raster.blend = true;
                packet.raster.blend_src_rgb = GL_SRC_ALPHA;
                packet.raster.blend_dst_rgb = GL_ONE_MINUS_SRC_ALPHA;
                packet.raster.blend_src_alpha = GL_ONE;
                packet.raster.blend_dst_alpha = GL_ZERO;
                packet.raster.blend_equation_rgb = GL_FUNC_ADD;
                packet.raster.blend_equation_alpha = GL_FUNC_ADD;
                packet.raster.depth_write = false;
                runtime.PrepareBindings(bindings_, program, nullptr);

                std::array<render::StudioDrawPacket, 3> packets = {packet, packet, packet};
                for (size_t index = 0; index < packets.size(); ++index)
                {
                    render::StudioQueuedMesh& mesh = packets[index].meshes[0];
                    mesh.program = &program;
                    mesh.material_bindings = &bindings_;
                    mesh.skin = static_cast<int>(index % 2);
                    mesh.texture = textures[index % 2];
                    mesh.index_offset = index * 12;
                    mesh.additive = additive && index == 1;
                    PreparePacketImages(runtime, packets[index]);
                }

                glClear(GL_COLOR_BUFFER_BIT);
                g_StateSaves = 0;
                g_StateRestores = 0;
                GLint original_program = 0;
                GLint original_texture = 0;
                GLint original_vao = 0;
                glGetIntegerv(GL_CURRENT_PROGRAM, &original_program);
                glGetIntegerv(GL_TEXTURE_BINDING_2D, &original_texture);
                glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &original_vao);
                bool original_blend = glIsEnabled(GL_BLEND) != GL_FALSE;

                queue.BeginTransparentPass(deferred);
                for (render::StudioDrawPacket& item : packets)
                {
                    queue.Submit(std::move(item));
                    GLint preparing_program = -1;
                    glGetIntegerv(GL_CURRENT_PROGRAM, &preparing_program);
                    EXPECT_EQ(preparing_program, original_program);
                }
                queue.EndTransparentPass();

                EXPECT_EQ(g_StateSaves, deferred ? 1 : 3);
                EXPECT_EQ(g_StateRestores, deferred ? 1 : 3);
                GLint restored = -1;
                glGetIntegerv(GL_CURRENT_PROGRAM, &restored);
                EXPECT_EQ(restored, original_program);
                glGetIntegerv(GL_TEXTURE_BINDING_2D, &restored);
                EXPECT_EQ(restored, original_texture);
                glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &restored);
                EXPECT_EQ(restored, original_vao);
                EXPECT_EQ(glIsEnabled(GL_BLEND) != GL_FALSE, original_blend);

                std::array<uint8_t, 4> pixel{};
                glReadPixels(16, 8, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixel.data());
                EXPECT_NEAR(pixel[0], additive ? 192 : 160, 1) << "deferred=" << deferred;
                EXPECT_EQ(pixel[1], 0) << "deferred=" << deferred;
                EXPECT_NEAR(pixel[2], additive ? 127 : 64, 1) << "deferred=" << deferred;
            }
        }
    }

    glDeleteTextures(2, textures);
    glDeleteProgram(program.prog);
}

TEST_F(StudioOptimizationGlTest, PreservedMeshOrderRestoresRasterStateAfterAdditiveMesh)
{
    performance_test::Core runtime_performance;
    render::StudioMaterialRuntime runtime(
        program_source_.manager, material_source_.catalog, material_source_.cache, runtime_performance.monitor
    );
    render::StudioGlSubmission sink(
        render::GetResources().device,
        runtime,
        {BindBaseTexture, nullptr},
        runtime_performance.monitor,
        runtime_performance.mapper,
        runtime_performance.studio_depth
    );
    ASSERT_TRUE(sink.EnsureBones());
    render::StudioDrawPacket packet = MakePacket(runtime, runtime_performance.monitor);
    packet.raster.blend = false;
    packet.raster.depth_test = false;
    packet.raster.depth_write = true;
    packet.meshes.resize(2, packet.meshes[0]);
    packet.meshes[0].additive = true;
    packet.meshes[1].index_offset = 12;

    PreparePacketImages(runtime, packet);
    sink.Begin();
    sink.Draw(packet);
    sink.End();

    EXPECT_EQ(g_DrawOffsets, (std::vector<uintptr_t>{0, 12}));
    EXPECT_EQ(g_DrawBlends, (std::vector<bool>{true, false}));
    EXPECT_EQ(g_DrawDepthWrites, (std::vector<bool>{false, true}));
}

TEST_F(StudioOptimizationGlTest, CoplanarMeshesKeepAuthoredSurfaceForLessAndLequal)
{
    const char* vertex =
        "#version 330 compatibility\n"
        "layout(location=0) in vec3 p;\n"
        "void main(){gl_Position=gl_ModelViewProjectionMatrix*vec4(p,1);}";
    const char* fragment =
        "#version 330 compatibility\n"
        "uniform sampler2D image;\n"
        "void main(){gl_FragColor=texture2D(image,vec2(0.5));}";
    std::string log;
    ncl_gl::Program program{};
    program.prog = ncl_gl::LinkProgram(vertex, fragment, nullptr, 0, &log);
    ASSERT_NE(program.prog, 0u) << log;
    std::fill(std::begin(program.loc), std::end(program.loc), -1);

    GLuint depth = 0;
    glGenRenderbuffers(1, &depth);
    glBindRenderbuffer(GL_RENDERBUFFER, depth);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, 32, 16);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depth);
    ASSERT_EQ(glCheckFramebufferStatus(GL_FRAMEBUFFER), GLenum(GL_FRAMEBUFFER_COMPLETE));

    GLuint textures[2]{};
    glGenTextures(2, textures);
    const uint8_t colors[2][4] = {{255, 0, 0, 255}, {0, 0, 255, 255}};

    for (int index = 0; index < 2; ++index)
    {
        glBindTexture(GL_TEXTURE_2D, textures[index]);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, colors[index]);
    }

    {
        performance_test::Core runtime_performance;
        render::StudioMaterialRuntime runtime(
            program_source_.manager, material_source_.catalog, material_source_.cache, runtime_performance.monitor
        );
        render::StudioGlSubmission sink(
            render::GetResources().device,
            runtime,
            {BindBaseTexture, nullptr},
            runtime_performance.monitor,
            runtime_performance.mapper,
            runtime_performance.studio_depth
        );
        ASSERT_TRUE(sink.EnsureBones());

        for (GLenum depth_func : {GL_LESS, GL_LEQUAL})
        {
            std::array<std::array<uint8_t, 4>, 2> pixels{};

            for (int blended = 0; blended < 2; ++blended)
            {
                render::StudioDrawPacket packet = MakePacket(runtime, runtime_performance.monitor);
                packet.raster.depth_test = true;
                packet.raster.depth_write = true;
                packet.raster.depth_func = depth_func;
                packet.raster.blend = blended == 0;
                packet.raster.blend_src_rgb = GL_ONE;
                packet.raster.blend_dst_rgb = GL_ZERO;
                packet.raster.blend_src_alpha = GL_ONE;
                packet.raster.blend_dst_alpha = GL_ZERO;
                packet.raster.blend_equation_rgb = GL_FUNC_ADD;
                packet.raster.blend_equation_alpha = GL_FUNC_ADD;
                packet.meshes.resize(2, packet.meshes[0]);
                runtime.PrepareBindings(bindings_, program, nullptr);

                for (size_t index = 0; index < packet.meshes.size(); ++index)
                {
                    packet.meshes[index].program = &program;
                    packet.meshes[index].material_bindings = &bindings_;
                    packet.meshes[index].skin = 1 - static_cast<int>(index);
                    packet.meshes[index].texture = textures[index];
                    packet.meshes[index].index_offset = index * 12;
                }

                glDepthMask(GL_TRUE);
                glClearDepth(1.0);
                glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
                PreparePacketImages(runtime, packet);
                sink.Begin();
                sink.Draw(packet);
                sink.End();
                glReadPixels(16, 8, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixels[blended].data());
            }

            const std::array<uint8_t, 4> expected =
                depth_func == GL_LESS ? std::array<uint8_t, 4>{255, 0, 0, 255} : std::array<uint8_t, 4>{0, 0, 255, 255};
            EXPECT_EQ(pixels[0], expected) << "baseline depth_func=" << depth_func;
            EXPECT_EQ(pixels[1], pixels[0]) << "coplanar surface changed; depth_func=" << depth_func;
        }
    }

    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, 0);
    glDeleteRenderbuffers(1, &depth);
    glDeleteTextures(2, textures);
    glDeleteProgram(program.prog);
    EXPECT_EQ(glGetError(), GLenum(GL_NO_ERROR));
}

TEST_F(StudioOptimizationGlTest, StateCacheSkipsRepeatedSettersAndKeepsPacketWork)
{
    performance_test::Core performance;
    render::StudioMaterialRuntime runtime(
        program_source_.manager,
        material_source_.catalog,
        material_source_.cache,
        performance.monitor
    );
    render::StudioGlSubmission sink(
        render::GetResources().device,
        runtime,
        {ObserveCachedBaseTexture, nullptr},
        performance.monitor,
        performance.mapper,
        performance.studio_depth
    );
    ASSERT_TRUE(sink.EnsureBones());

    render::StudioDrawPacket packet = MakePacket(runtime, performance.monitor);
    packet.meshes.resize(2, packet.meshes[0]);
    packet.meshes[1].skin = 1;
    packet.meshes[1].index_offset = 12;
    PreparePacketImages(runtime, packet);

    SubmissionStateObservers observers;
    sink.Begin();
    sink.Draw(packet);

    packet.bones = CapturePose({{0, 0.25f}}, packet.bones);
    sink.Draw(packet);

    for (GLenum cap : {GL_BLEND, GL_DEPTH_TEST, GL_CULL_FACE, GL_ALPHA_TEST})
    {
        EXPECT_EQ(
            std::count(g_SubmissionStateCalls.capabilities.begin(), g_SubmissionStateCalls.capabilities.end(), cap),
            1
        ) << cap;
    }

    EXPECT_EQ(g_SubmissionStateCalls.blend_functions, 1);
    EXPECT_EQ(g_SubmissionStateCalls.blend_equations, 1);
    EXPECT_EQ(g_SubmissionStateCalls.depth_masks, 1);
    EXPECT_EQ(g_SubmissionStateCalls.depth_functions, 1);
    EXPECT_EQ(g_SubmissionStateCalls.depth_ranges, 1);
    EXPECT_EQ(g_SubmissionStateCalls.cull_faces, 1);
    EXPECT_EQ(g_SubmissionStateCalls.alpha_functions, 0);
    EXPECT_EQ(g_SubmissionStateCalls.base_textures, (std::vector<unsigned int>{0}));
    EXPECT_EQ(g_SubmissionStateCalls.program_binds, 2);
    EXPECT_EQ(g_UploadSizes, (std::vector<GLsizeiptr>{48, 48}));
    EXPECT_EQ(g_DrawOffsets, (std::vector<uintptr_t>{0, 12, 0, 12}));

    sink.End();
    EXPECT_EQ(g_SubmissionStateCalls.base_textures.size(), 2u);
    ExpectOriginalBindings();
}

TEST_F(StudioOptimizationGlTest, StateCacheAppliesChangedRasterAndResetsAtScopeBoundary)
{
    performance_test::Core performance;
    render::StudioMaterialRuntime runtime(
        program_source_.manager,
        material_source_.catalog,
        material_source_.cache,
        performance.monitor
    );
    render::StudioGlSubmission sink(
        render::GetResources().device,
        runtime,
        {ObserveCachedBaseTexture, nullptr},
        performance.monitor,
        performance.mapper,
        performance.studio_depth
    );
    ASSERT_TRUE(sink.EnsureBones());

    render::StudioDrawPacket packet = MakePacket(runtime, performance.monitor);
    PreparePacketImages(runtime, packet);

    SubmissionStateObservers observers;
    sink.Begin();
    sink.Draw(packet);

    packet.raster.blend = true;
    packet.raster.blend_src_rgb = GL_SRC_ALPHA;
    packet.raster.blend_dst_rgb = GL_ONE_MINUS_SRC_ALPHA;
    packet.raster.blend_src_alpha = GL_ZERO;
    packet.raster.blend_dst_alpha = GL_ONE;
    packet.raster.blend_equation_rgb = GL_FUNC_REVERSE_SUBTRACT;
    packet.raster.blend_equation_alpha = GL_FUNC_SUBTRACT;
    packet.raster.depth_test = true;
    packet.raster.depth_write = !packet.raster.depth_write;
    packet.raster.depth_func = GL_GREATER;
    packet.raster.depth_range[0] = 0.8;
    packet.raster.depth_range[1] = 0.2;
    packet.flipped = true;

    sink.Draw(packet);

    render::StudioDrawPacket applied{.bones = CapturePose()};
    render::Studio_CaptureRasterState(applied, performance.monitor);

    EXPECT_TRUE(applied.raster.blend);
    EXPECT_EQ(applied.raster.blend_src_rgb, GL_SRC_ALPHA);
    EXPECT_EQ(applied.raster.blend_dst_rgb, GL_ONE_MINUS_SRC_ALPHA);
    EXPECT_EQ(applied.raster.blend_src_alpha, GL_ZERO);
    EXPECT_EQ(applied.raster.blend_dst_alpha, GL_ONE);
    EXPECT_EQ(applied.raster.blend_equation_rgb, GL_FUNC_REVERSE_SUBTRACT);
    EXPECT_EQ(applied.raster.blend_equation_alpha, GL_FUNC_SUBTRACT);
    EXPECT_TRUE(applied.raster.depth_test);
    EXPECT_EQ(applied.raster.depth_write, packet.raster.depth_write);
    EXPECT_EQ(applied.raster.depth_func, GL_GREATER);
    EXPECT_NEAR(applied.raster.depth_range[0], 0.8, 1e-6);
    EXPECT_NEAR(applied.raster.depth_range[1], 0.2, 1e-6);
    EXPECT_FALSE(applied.raster.cull);
    EXPECT_EQ(g_SubmissionStateCalls.blend_functions, 2);
    EXPECT_EQ(g_SubmissionStateCalls.blend_equations, 2);
    EXPECT_EQ(g_SubmissionStateCalls.depth_functions, 2);
    EXPECT_EQ(g_SubmissionStateCalls.depth_ranges, 2);

    sink.End();
    ExpectOriginalBindings();

    sink.Begin();
    sink.Draw(packet);

    EXPECT_EQ(g_SubmissionStateCalls.blend_functions, 3);
    EXPECT_EQ(g_SubmissionStateCalls.blend_equations, 3);
    EXPECT_EQ(g_SubmissionStateCalls.depth_functions, 3);
    EXPECT_EQ(g_SubmissionStateCalls.depth_ranges, 3);
    EXPECT_EQ(g_SubmissionStateCalls.cull_faces, 2);
    EXPECT_EQ(g_SubmissionStateCalls.base_textures.size(), 3u);

    sink.End();
    ExpectOriginalBindings();
}

TEST_F(StudioOptimizationGlTest, StateCacheTracksMaskedAndAdditiveOverrides)
{
    performance_test::Core performance;
    render::StudioMaterialRuntime runtime(
        program_source_.manager,
        material_source_.catalog,
        material_source_.cache,
        performance.monitor
    );
    render::StudioGlSubmission sink(
        render::GetResources().device,
        runtime,
        {ObserveCachedBaseTexture, nullptr},
        performance.monitor,
        performance.mapper,
        performance.studio_depth
    );
    ASSERT_TRUE(sink.EnsureBones());

    render::StudioDrawPacket packet = MakePacket(runtime, performance.monitor);
    packet.raster.blend = false;
    packet.raster.depth_write = false;
    packet.meshes.resize(4, packet.meshes[0]);

    for (size_t i = 0; i < packet.meshes.size(); ++i)
    {
        packet.meshes[i].index_offset = i * 12;
    }

    packet.meshes[0].masked = true;
    packet.meshes[1].additive = true;
    packet.meshes[2].masked = true;
    PreparePacketImages(runtime, packet);

    SubmissionStateObservers observers;
    sink.Begin();
    sink.Draw(packet);

    EXPECT_EQ(g_DrawOffsets, (std::vector<uintptr_t>{0, 12, 24, 36}));
    EXPECT_EQ(g_DrawBlends, (std::vector<bool>{false, true, false, false}));
    EXPECT_EQ(g_DrawDepthWrites, (std::vector<bool>{true, false, true, false}));
    EXPECT_EQ(g_SubmissionStateCalls.alpha_functions, 1);

    sink.End();
}

TEST_F(StudioOptimizationGlTest, StateCachePreservesBaseTextureAcrossMaterialSamplerWork)
{
    performance_test::Core performance;
    render::StudioMaterialRuntime runtime(
        program_source_.manager,
        material_source_.catalog,
        material_source_.cache,
        performance.monitor
    );
    render::StudioGlSubmission sink(
        render::GetResources().device,
        runtime,
        {ObserveCachedBaseTexture, nullptr},
        performance.monitor,
        performance.mapper,
        performance.studio_depth
    );
    ASSERT_TRUE(sink.EnsureBones());

    render::StudioDrawPacket packet = MakePacket(runtime, performance.monitor);
    packet.meshes[0].material.emplace();
    packet.meshes[0].material->program = "custom";
    runtime.PrepareBindings(bindings_, *packet.meshes[0].program, &*packet.meshes[0].material);
    PreparePacketImages(runtime, packet);

    SubmissionStateObservers observers;
    sink.Begin();
    sink.Draw(packet);
    sink.Draw(packet);

    GLint active = 0;
    GLint bound = -1;
    glGetIntegerv(GL_ACTIVE_TEXTURE, &active);
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &bound);

    EXPECT_EQ(active, GL_TEXTURE0);
    EXPECT_EQ(bound, 0);
    EXPECT_EQ(g_SubmissionStateCalls.base_textures, (std::vector<unsigned int>{0}));

    sink.End();
    ExpectOriginalBindings();
}
