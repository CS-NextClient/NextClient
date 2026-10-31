#include <gtest/gtest.h>

#include "PerformanceTestCore.h"

#include <algorithm>
#include <array>
#include <cstdio>
#include <memory>
#include <string>

#include <glad/glad.h>

#include "RadarGlContext.h"
#include "ShaderBindingTrace.h"
#include "StudioMaterialProviders.h"
#include "StudioTestPose.h"
#include "graphics/render/material/MaterialCoverageRuntime.h"
#include "engine.h"
#include "common/cvar.h"
#include "graphics/render/RenderConfig.h"
#include "graphics/render/render_device.h"
#include "graphics/render/effects/StudioEffects.h"
#include "graphics/render/effects/studio_effects.h"
#include "graphics/render/shader_manager.h"
#include "graphics/render/model/StudioGeometry.h"
#include "graphics/render/StudioQueue.h"
#include "graphics/render/RendererState.h"
#include "graphics/render/StudioBindings.h"

TEST(StudioEffects, DisabledViewDoesNotAllocateTargets)
{
    render::StudioEffects_BeginView();
    EXPECT_EQ(render::StudioEffects_TargetCount(), 0u);
    render::StudioEffects_EndView();
    EXPECT_EQ(render::StudioEffects_TargetCount(), 0u);
}

nitroapi::NitroApiInterface* StudioTest_Engine();

namespace
{
    class EffectSource final : public StudioMaterialSourceFixture
    {
    public:
        unsigned int builds{};
        std::string failed_fragment;

        bool LoadSource(const char* name, std::string& out) override
        {
            const std::string path(name);
            const std::string program = path.substr(0, path.rfind('.'));
            if (manifests.contains(program))
            {
                return StudioMaterialSourceFixture::LoadSource(name, out);
            }

            return render::GetShaderBackend().LoadSource(name, out);
        }

        bool Build(const render::ProgramDesc& desc, ncl_gl::Program& program) override
        {
            ++builds;
            return failed_fragment != desc.fs_name && render::GetShaderBackend().Build(desc, program);
        }

        void DeleteProgram(unsigned int name) override
        {
            glDeleteProgram(name);
        }
    };

    class EffectDeletionProbe
    {
    private:
        PFNGLDELETEBUFFERSPROC delete_buffers_;
        PFNGLDELETEVERTEXARRAYSPROC delete_vertex_arrays_;
        PFNGLDELETEFRAMEBUFFERSPROC delete_framebuffers_;
        PFNGLDELETETEXTURESPROC delete_textures_;

        inline static size_t calls_{};

    public:
        EffectDeletionProbe() :
            delete_buffers_(glDeleteBuffers),
            delete_vertex_arrays_(glDeleteVertexArrays),
            delete_framebuffers_(glDeleteFramebuffers),
            delete_textures_(glDeleteTextures)
        {
            calls_ = 0;
            glDeleteBuffers = Count;
            glDeleteVertexArrays = Count;
            glDeleteFramebuffers = Count;
            glDeleteTextures = Count;
        }

        ~EffectDeletionProbe()
        {
            glDeleteBuffers = delete_buffers_;
            glDeleteVertexArrays = delete_vertex_arrays_;
            glDeleteFramebuffers = delete_framebuffers_;
            glDeleteTextures = delete_textures_;
        }

        size_t get_calls() const
        {
            return calls_;
        }

    private:
        static void APIENTRY Count(GLsizei, const GLuint*)
        {
            calls_++;
        }
    };

    void APIENTRY EffectDebug(GLenum, GLenum type, GLuint, GLenum, GLsizei, const GLchar* message, const void*)
    {
        if (type == GL_DEBUG_TYPE_ERROR)
        {
            std::fprintf(stderr, "GL: %s\n", message);
        }
    }

    class StudioEffectsGlTest : public testing::Test
    {
    private:
        RadarGlContext context_;
        goldsrc::cvar_t developer_{};
        GLuint framebuffer_{};
        GLuint image_{};
        GLuint depth_{};
        GLuint vao_{};
        GLuint vertices_{};
        GLuint indices_{};
        GLuint base_{};

    protected:
        render::StudioDrawPacket packet_{.bones = CapturePose()};
        EffectSource source_;
        performance_test::Core coverage_performance_;
        render::MaterialCoverageRuntime coverage_{source_.catalog, source_.cache, coverage_performance_.monitor};

        void SetUp() override
        {
            std::string error;
            ASSERT_TRUE(context_.Initialize(error)) << error;
            developer = &developer_;
            g_NitroApi = StudioTest_Engine();
            render::g_RenderCfg = {};
            render::Device_OnContextCreated();
            source_.device.OnContextCreated();
            if (glDebugMessageCallback != nullptr)
            {
                glEnable(GL_DEBUG_OUTPUT);
                glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
                glDebugMessageCallback(EffectDebug, nullptr);
            }
            glGenTextures(1, &image_);
            glBindTexture(GL_TEXTURE_2D, image_);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 40, 40, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
            glGenFramebuffers(1, &framebuffer_);
            glBindFramebuffer(GL_FRAMEBUFFER, framebuffer_);
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, image_, 0);
            glGenRenderbuffers(1, &depth_);
            glBindRenderbuffer(GL_RENDERBUFFER, depth_);
            glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, 40, 40);
            glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depth_);
            ASSERT_EQ(glCheckFramebufferStatus(GL_FRAMEBUFFER), GLenum(GL_FRAMEBUFFER_COMPLETE));
            glViewport(4, 3, 32, 32);
            glDisable(GL_SCISSOR_TEST);
            glDisable(GL_ALPHA_TEST);
            glDisable(GL_BLEND);
            glDisable(GL_STENCIL_TEST);
            glDepthMask(GL_TRUE);
            glClearColor(0, 0, 0, 1);
            glClearDepth(1);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            glGenVertexArrays(1, &vao_);
            glBindVertexArray(vao_);
            glGenBuffers(1, &vertices_);
            glBindBuffer(GL_ARRAY_BUFFER, vertices_);
            const float points[] = {-.5f, -.5f, 0, -.5f, .5f, 0, .5f, .5f, 0, .5f, -.5f, 0};
            glBufferData(GL_ARRAY_BUFFER, sizeof(points), points, GL_STATIC_DRAW);
            glEnableVertexAttribArray(0);
            glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), nullptr);
            glVertexAttribI3ui(3, 0, 0, 0);
            const GLuint indices[] = {0, 1, 2, 0, 2, 3};
            glGenBuffers(1, &indices_);
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indices_);
            glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);
            glGenTextures(1, &base_);
            glBindTexture(GL_TEXTURE_2D, base_);
            const unsigned char white[] = {255, 255, 255, 255};
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, white);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
            packet_.vao = vao_;
            packet_.viewport = {4, 3, 32, 32};
            packet_.bone_count = 1;
            packet_.bones = CapturePose({{0, 1.0f}, {5, 1.0f}, {10, 1.0f}}, packet_.bones);
            packet_.modelview[0] = packet_.modelview[5] = packet_.modelview[10] = packet_.modelview[15] = 1;
            packet_.projection = packet_.modelview;
            packet_.blend = 1;
            packet_.raster.depth_range[1] = 1;
            packet_.meshes.emplace_back();
            render::StudioQueuedMesh& mesh = packet_.meshes.back();
            mesh.texture = base_;
            mesh.index_count = 6;
            mesh.texel_scale[0] = mesh.texel_scale[1] = 1;
            mesh.material.emplace();
            mesh.material->program = "studio";
            SetScalar("uOccludedAmount", 1);
            SetColor("uOccludedColor", {1, 0, 0, 1});
            SetColor("uOutlineColor", {0, 1, 0, 1});
            ASSERT_EQ(glGetError(), GLenum(GL_NO_ERROR));
            render::StudioEffects_BeginView();
        }

        void TearDown() override
        {
            EXPECT_EQ(glGetError(), GLenum(GL_NO_ERROR));
            render::StudioEffects_Clear();
            glDeleteFramebuffers(1, &framebuffer_);
            glDeleteTextures(1, &image_);
            glDeleteTextures(1, &base_);
            glDeleteRenderbuffers(1, &depth_);
            glDeleteVertexArrays(1, &vao_);
            glDeleteBuffers(1, &vertices_);
            glDeleteBuffers(1, &indices_);
            source_.shaders.Clear();
            source_.cache.Clear();
            source_.device.OnContextDestroyed();
            render::Device_OnContextDestroyed();
            g_NitroApi = nullptr;
            developer = nullptr;
        }

        void SetScalar(const char* name, float scalar)
        {
            ncl_material::ParamValue value;
            value.type = ncl_material::ParamType::Scalar;
            value.scalar = scalar;
            std::vector<ncl_material::Param>& params = packet_.meshes[0].material->params;
            auto found =
                std::find_if(params.begin(), params.end(), [name](const ncl_material::Param& param) { return param.name == name; });
            if (found != params.end())
            {
                found->value = value;
            }
            else
            {
                params.push_back({name, value});
            }
        }

        void SetColor(const char* name, const std::array<float, 4>& color)
        {
            ncl_material::ParamValue value;
            value.type = ncl_material::ParamType::Vec4;
            value.vec4 = color;
            std::vector<ncl_material::Param>& params = packet_.meshes[0].material->params;
            auto found =
                std::find_if(params.begin(), params.end(), [name](const ncl_material::Param& param) { return param.name == name; });
            if (found != params.end())
            {
                found->value = value;
            }
            else
            {
                params.push_back({name, value});
            }
        }

        void SetPackedDepth(GLenum format)
        {
            glBindRenderbuffer(GL_RENDERBUFFER, depth_);
            glRenderbufferStorage(GL_RENDERBUFFER, format, 40, 40);
            ASSERT_EQ(glCheckFramebufferStatus(GL_FRAMEBUFFER), GLenum(GL_FRAMEBUFFER_COMPLETE));
        }

        void Wall(int wide = 32)
        {
            glEnable(GL_SCISSOR_TEST);
            glScissor(4, 3, wide, 32);
            glClearDepth(.25);
            glClear(GL_DEPTH_BUFFER_BIT);
            glDisable(GL_SCISSOR_TEST);
        }

        void Capture(uintptr_t entity = 1, std::array<int, 4> bounds = {5, 5, 22, 22})
        {
            PrepareCoveragePacket(render::GetRendererState().coverage, packet_);
            render::StudioEffects_Capture(entity, packet_, bounds);
        }

        std::array<unsigned char, 4> Pixel(int x, int y)
        {
            std::array<unsigned char, 4> pixel{};
            glReadPixels(x + 4, y + 3, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixel.data());
            return pixel;
        }
    };
} // namespace

TEST_F(StudioEffectsGlTest, OwnerInstancesKeepMasksStatisticsAndRetainedTargetsIndependent)
{
    render::StudioEffects first(source_.device, source_.shaders, coverage_);
    render::StudioEffects second(source_.device, source_.shaders, coverage_);
    Wall();
    first.BeginView();
    second.BeginView();
    PrepareCoveragePacket(coverage_, packet_);
    first.Capture(1, packet_, {5, 5, 22, 22});
    EXPECT_TRUE(first.Contains(1));
    EXPECT_FALSE(second.Contains(1));
    EXPECT_EQ(second.get_target_count(), 0u);

    SetColor("uOccludedColor", {0, 0, 1, 1});
    PrepareCoveragePacket(coverage_, packet_);
    second.Capture(2, packet_, {5, 5, 22, 22});
    first.EndView();
    EXPECT_GT(Pixel(16, 16)[0], 200);
    EXPECT_EQ(first.get_last_stats().entities, 1u);
    EXPECT_EQ(second.get_last_stats().entities, 0u);
    EXPECT_TRUE(second.Contains(2));

    second.EndView();
    EXPECT_GT(Pixel(16, 16)[2], 200);
    first.Clear();
    first.BeginView();
    second.BeginView();
    PrepareCoveragePacket(coverage_, packet_);
    first.Capture(3, packet_, {5, 5, 22, 22});
    first.EndView();
    EXPECT_EQ(first.get_last_stats().reused_targets, 0u);
    PrepareCoveragePacket(coverage_, packet_);
    second.Capture(4, packet_, {5, 5, 22, 22});
    second.EndView();
    EXPECT_EQ(second.get_last_stats().reused_targets, 1u);
    first.Clear();
    EXPECT_EQ(first.get_last_stats().entities, 0u);
    EXPECT_EQ(second.get_last_stats().entities, 1u);
    second.Clear();
}

TEST_F(StudioEffectsGlTest, NestedOwnerViewsKeepOuterMaskAndPoolUnavailableToInnerView)
{
    render::StudioEffects effects(source_.device, source_.shaders, coverage_);
    Wall();
    effects.BeginView();
    PrepareCoveragePacket(coverage_, packet_);
    effects.Capture(1, packet_, {5, 5, 22, 22});
    effects.BeginView();
    EXPECT_FALSE(effects.Contains(1));
    SetColor("uOccludedColor", {0, 0, 1, 1});
    PrepareCoveragePacket(coverage_, packet_);
    effects.Capture(2, packet_, {5, 5, 22, 22});
    effects.EndView();
    EXPECT_EQ(effects.get_last_stats().reused_targets, 0u);
    EXPECT_GT(Pixel(16, 16)[2], 200);
    EXPECT_TRUE(effects.Contains(1));
    EXPECT_FALSE(effects.Contains(2));
    EXPECT_EQ(effects.get_target_count(), 1u);

    effects.EndView();
    EXPECT_GT(Pixel(16, 16)[0], 200);
    EXPECT_EQ(effects.get_last_stats().entities, 1u);
    effects.Clear();
}

TEST_F(StudioEffectsGlTest, DeviceLossForgetsEveryOwnerViewAndRetainedTargets)
{
    render::StudioEffects effects(source_.device, source_.shaders, coverage_);
    render::StudioEffects other(source_.device, source_.shaders, coverage_);
    effects.BeginView();
    PrepareCoveragePacket(coverage_, packet_);
    effects.Capture(1, packet_, {5, 5, 22, 22});
    effects.EndView();
    effects.BeginView();
    PrepareCoveragePacket(coverage_, packet_);
    effects.Capture(2, packet_, {0, 0, 32, 32});
    effects.BeginView();
    PrepareCoveragePacket(coverage_, packet_);
    effects.Capture(3, packet_, {8, 8, 16, 16});
    other.BeginView();
    PrepareCoveragePacket(coverage_, packet_);
    other.Capture(9, packet_, {5, 5, 22, 22});
    {
        EffectDeletionProbe deletions;
        source_.device.OnContextDestroyed();
        EXPECT_EQ(deletions.get_calls(), 0u);
    }
    EXPECT_EQ(effects.get_target_count(), 0u);
    EXPECT_FALSE(effects.Contains(2));
    EXPECT_EQ(effects.get_last_stats().entities, 0u);
    EXPECT_EQ(other.get_target_count(), 0u);
    EXPECT_FALSE(other.Contains(9));
    source_.device.OnContextCreated();

    effects.BeginView();
    PrepareCoveragePacket(coverage_, packet_);
    effects.Capture(4, packet_, {5, 5, 22, 22});
    effects.EndView();
    EXPECT_EQ(effects.get_last_stats().reused_targets, 0u);
    effects.Clear();
}

TEST_F(StudioEffectsGlTest, DestroyedLocalOwnerUnregistersBeforeNextLossNotification)
{
    std::unique_ptr<render::StudioEffects> effects = std::make_unique<render::StudioEffects>(source_.device, source_.shaders, coverage_);
    effects->BeginView();
    PrepareCoveragePacket(coverage_, packet_);
    effects->Capture(1, packet_, {5, 5, 22, 22});
    effects->Clear();
    effects.reset();
    source_.device.OnContextDestroyed();
    source_.device.OnContextCreated();
}

TEST_F(StudioEffectsGlTest, ClearReleasesResourcesInMatchingContext)
{
    render::StudioEffects effects(source_.device, source_.shaders, coverage_);
    effects.BeginView();
    PrepareCoveragePacket(coverage_, packet_);
    effects.Capture(1, packet_, {5, 5, 22, 22});
    effects.EndView();
    effects.BeginView();
    PrepareCoveragePacket(coverage_, packet_);
    effects.Capture(2, packet_, {0, 0, 32, 32});
    ASSERT_TRUE(render::Device_IsContextCurrent());

    EffectDeletionProbe deletions;
    effects.Clear();
    EXPECT_GT(deletions.get_calls(), 0u);
    EXPECT_EQ(effects.get_target_count(), 0u);
    EXPECT_EQ(effects.get_last_stats().entities, 0u);
}

TEST_F(StudioEffectsGlTest, ClearForgetsNamesWithoutDeletingAgainstAnotherCurrentContext)
{
    render::StudioEffects effects(source_.device, source_.shaders, coverage_);
    effects.BeginView();
    PrepareCoveragePacket(coverage_, packet_);
    effects.Capture(1, packet_, {5, 5, 22, 22});
    effects.EndView();
    effects.BeginView();
    PrepareCoveragePacket(coverage_, packet_);
    effects.Capture(2, packet_, {0, 0, 32, 32});

    {
        RadarGlContext foreign_context;
        std::string error;
        ASSERT_TRUE(foreign_context.Initialize(error)) << error;
        ASSERT_FALSE(render::Device_IsContextCurrent());
        EffectDeletionProbe deletions;
        effects.Clear();
        EXPECT_EQ(deletions.get_calls(), 0u);
    }

    EXPECT_TRUE(render::Device_IsContextCurrent());
    EXPECT_EQ(effects.get_target_count(), 0u);
    EXPECT_EQ(effects.get_last_stats().entities, 0u);
}

TEST_F(StudioEffectsGlTest, OwnerDestructionForgetsLiveResourcesWithoutGlCalls)
{
    std::unique_ptr<render::StudioEffects> effects = std::make_unique<render::StudioEffects>(source_.device, source_.shaders, coverage_);
    effects->BeginView();
    PrepareCoveragePacket(coverage_, packet_);
    effects->Capture(1, packet_, {5, 5, 22, 22});

    EffectDeletionProbe deletions;
    effects.reset();
    EXPECT_EQ(deletions.get_calls(), 0u);
}

TEST_F(StudioEffectsGlTest, VisibleNearestSurfaceReceivesNoTintIncludingItsOwnRearSurface)
{
    glClearDepth(.5);
    glClear(GL_DEPTH_BUFFER_BIT);
    Capture();
    packet_.bones = CapturePose({{11, .5f}}, packet_.bones);
    Capture();
    render::StudioEffects_EndView();
    EXPECT_EQ(Pixel(16, 16)[0], 0);
}

TEST_F(StudioEffectsGlTest, DistantOccludedSurfaceRetainsFillAndOutline)
{
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glFrustum(-4, 4, -4, 4, 4, 4096);
    glGetFloatv(GL_PROJECTION_MATRIX, packet_.projection.data());
    glMatrixMode(GL_MODELVIEW);
    const double wall_depth = (4096.0 / 4092.0) * (1.0 - 4.0 / 2000.0);
    glClearDepth(wall_depth);
    glClear(GL_DEPTH_BUFFER_BIT);
    packet_.bones = CapturePose({{0, 2008.0f}, {5, 2008.0f}}, packet_.bones);
    packet_.bones = CapturePose({{11, -2008.0f}}, packet_.bones);
    SetScalar("uOutlineWidth", 2);
    Capture();
    render::StudioEffects_EndView();
    EXPECT_GT(Pixel(16, 16)[0], 240);
    EXPECT_GT(Pixel(7, 16)[1], 240);
}

TEST_F(StudioEffectsGlTest, OnlyWallOccludedPixelsReceiveFill)
{
    Wall(16);
    Capture();
    render::StudioEffects_EndView();
    EXPECT_GT(Pixel(12, 16)[0], 240);
    EXPECT_EQ(Pixel(20, 16)[0], 0);
}

TEST_F(StudioEffectsGlTest, OutlineIsOutsideSilhouetteAndDoesNotTraceWallCut)
{
    Wall(16);
    SetScalar("uOccludedAmount", 0);
    SetScalar("uOutlineWidth", 2);
    Capture();
    render::StudioEffects_EndView();
    EXPECT_GT(Pixel(7, 16)[1], 240);
    EXPECT_EQ(Pixel(15, 16)[1], 0);
    EXPECT_EQ(Pixel(16, 16)[1], 0);
    EXPECT_EQ(Pixel(24, 16)[1], 0);
}

TEST_F(StudioEffectsGlTest, AlwaysOutlineIncludesVisibleSilhouetteAndGlowWithoutVisibleFill)
{
    glClearDepth(.5);
    glClear(GL_DEPTH_BUFFER_BIT);
    SetScalar("uOutlineMode", 1);
    SetScalar("uOutlineWidth", 2);
    SetScalar("uGlowRadius", 5);
    SetScalar("uGlowStrength", 1);
    Capture();
    render::StudioEffects_EndView();
    EXPECT_GT(Pixel(7, 16)[1], 240);
    EXPECT_GT(Pixel(4, 16)[1], 0);
    EXPECT_EQ(Pixel(16, 16)[0], 0);
}

TEST_F(StudioEffectsGlTest, AlwaysOutlineIncludesBothSidesOfAPartiallyOccludedModel)
{
    Wall(16);
    SetScalar("uOutlineMode", 1);
    SetScalar("uOutlineWidth", 2);
    Capture();
    render::StudioEffects_EndView();
    EXPECT_GT(Pixel(7, 16)[1], 240);
    EXPECT_GT(Pixel(24, 16)[1], 240);
    EXPECT_EQ(Pixel(15, 16)[1], 0);
    EXPECT_EQ(Pixel(16, 16)[1], 0);
    EXPECT_GT(Pixel(12, 16)[0], 240);
    EXPECT_EQ(Pixel(20, 16)[0], 0);
}

TEST_F(StudioEffectsGlTest, VisibleOnlyOutlineIncludesVisibleSilhouetteAndGlowWithoutFill)
{
    glClearDepth(.5);
    glClear(GL_DEPTH_BUFFER_BIT);
    SetScalar("uOutlineMode", 2);
    SetScalar("uOutlineWidth", 2);
    SetScalar("uGlowRadius", 5);
    SetScalar("uGlowStrength", 1);
    Capture();
    render::StudioEffects_EndView();
    EXPECT_GT(Pixel(7, 16)[1], 240);
    EXPECT_GT(Pixel(4, 16)[1], 0);
    EXPECT_EQ(Pixel(16, 16)[0], 0);
}

TEST_F(StudioEffectsGlTest, VisibleOnlyOutlineExcludesHiddenSilhouetteAndWallCut)
{
    Wall(16);
    SetScalar("uOutlineMode", 2);
    SetScalar("uOutlineWidth", 2);
    SetScalar("uGlowRadius", 5);
    SetScalar("uGlowStrength", 1);
    Capture();
    render::StudioEffects_EndView();
    EXPECT_EQ(Pixel(7, 16)[1], 0);
    EXPECT_EQ(Pixel(4, 16)[1], 0);
    EXPECT_GT(Pixel(24, 16)[1], 240);
    EXPECT_GT(Pixel(27, 16)[1], 0);
    EXPECT_EQ(Pixel(15, 16)[1], 0);
    EXPECT_EQ(Pixel(16, 16)[1], 0);
    EXPECT_GT(Pixel(12, 16)[0], 240);
    EXPECT_EQ(Pixel(20, 16)[0], 0);
}

TEST_F(StudioEffectsGlTest, FullyHiddenVisibleOnlyOutlineHasNoGlowAndRetainsOccludedFill)
{
    Wall();
    SetScalar("uOutlineMode", 2);
    SetScalar("uOutlineWidth", 2);
    SetScalar("uGlowRadius", 5);
    SetScalar("uGlowStrength", 1);
    Capture();
    render::StudioEffects_EndView();
    EXPECT_EQ(Pixel(7, 16)[1], 0);
    EXPECT_EQ(Pixel(24, 16)[1], 0);
    EXPECT_EQ(Pixel(4, 16)[1], 0);
    EXPECT_EQ(Pixel(27, 16)[1], 0);
    EXPECT_GT(Pixel(16, 16)[0], 240);
}

TEST_F(StudioEffectsGlTest, AlwaysAndVisibleOutlineRespectCoverageAndZeroWidth)
{
    for (const char* program : {"studio", "weapon_finish"})
    {
        packet_.meshes[0].material->program = program;
        for (float outline_mode : {1.0f, 2.0f})
        {
            SetScalar("uOutlineMode", outline_mode);
            SetScalar("uGlowRadius", 5);
            SetScalar("uGlowStrength", 1);
            for (const char* coverage : {"uDissolve", "uNoise"})
            {
                SetScalar("uOutlineWidth", 2);
                SetScalar(coverage, 1);
                Capture();
                render::StudioEffects_EndView();
                EXPECT_EQ(Pixel(7, 16)[1], 0);
                EXPECT_EQ(Pixel(4, 16)[1], 0);
                SetScalar(coverage, 0);
                render::StudioEffects_BeginView();
            }
            SetScalar("uOutlineWidth", 0);
            SetScalar("uOccludedAmount", 0);
            Capture();
            EXPECT_EQ(render::StudioEffects_TargetCount(), 0u);
            render::StudioEffects_EndView();
            EXPECT_EQ(Pixel(7, 16)[1], 0);
            render::StudioEffects_BeginView();
        }
    }
}

TEST_F(StudioEffectsGlTest, NearestMaterialRetainsItsOccludedOnlyModeOverAnAlwaysOutlinedSurface)
{
    glClearDepth(.5);
    glClear(GL_DEPTH_BUFFER_BIT);
    SetScalar("uOutlineWidth", 2);
    SetScalar("uOutlineMode", 1);
    packet_.bones = CapturePose({{11, .3f}}, packet_.bones);
    Capture(1, {6, 6, 20, 20});
    SetScalar("uOutlineMode", 0);
    packet_.bones = CapturePose({{11, 0.0f}}, packet_.bones);
    Capture(1, {3, 3, 26, 26});
    render::StudioEffects_EndView();
    EXPECT_EQ(Pixel(7, 16)[1], 0);
    EXPECT_EQ(Pixel(24, 16)[1], 0);
    EXPECT_EQ(Pixel(16, 16)[0], 0);
}

TEST_F(StudioEffectsGlTest, FullViewportOutlineExtendsOutsideTheStoredModelMask)
{
    Wall();
    SetScalar("uOccludedAmount", 0);
    SetScalar("uOutlineWidth", 2);
    Capture(1, {8, 8, 16, 16});
    render::StudioEffects_EndView();
    EXPECT_GT(Pixel(7, 16)[1], 240);
    EXPECT_GT(Pixel(24, 16)[1], 240);
    EXPECT_EQ(Pixel(16, 16)[1], 0);
}

TEST_F(StudioEffectsGlTest, GlowExtendsBeyondOutlineAndWidthZeroDisablesIt)
{
    Wall();
    SetScalar("uOutlineWidth", 1);
    SetScalar("uGlowRadius", 5);
    SetScalar("uGlowStrength", 1);
    Capture(1, {0, 0, 32, 32});
    render::StudioEffects_EndView();
    EXPECT_GT(Pixel(4, 16)[1], 0);
    EXPECT_EQ(Pixel(0, 16)[1], 0);
}

TEST_F(StudioEffectsGlTest, CoverageRemovesFillOutlineAndHalo)
{
    Wall();
    SetScalar("uDissolve", 1);
    SetScalar("uOutlineWidth", 2);
    SetScalar("uGlowRadius", 5);
    SetScalar("uGlowStrength", 1);
    Capture();
    render::StudioEffects_EndView();
    EXPECT_EQ(Pixel(16, 16)[0], 0);
    EXPECT_EQ(Pixel(7, 16)[1], 0);
}

TEST_F(StudioEffectsGlTest, LaterNonEffectSurfaceOccludesEarlierEffectAndExpandedBoundsPreservePixels)
{
    Wall();
    Capture(1, {6, 6, 20, 20});
    packet_.bones = CapturePose({{11, -.1f}}, packet_.bones);
    SetScalar("uOccludedAmount", 0);
    SetScalar("uOutlineWidth", 0);
    Capture(1, {3, 3, 26, 26});
    render::StudioEffects_EndView();
    EXPECT_EQ(Pixel(16, 16)[0], 0);
}

TEST_F(StudioEffectsGlTest, CroppedProjectedBoneBoundsAndNearPlaneFallback)
{
    render::StudioGpuModel model{};
    model.bounds.push_back({0, {-.5f, -.5f, 0}, {.5f, .5f, 0}});
    PrepareCoveragePacket(render::GetRendererState().coverage, packet_);
    EXPECT_EQ(render::StudioEffects_ProjectBounds(model, packet_), (std::array<int, 4>{6, 6, 20, 20}));
    packet_.bones = CapturePose({{11, -1.0f}}, packet_.bones);
    PrepareCoveragePacket(render::GetRendererState().coverage, packet_);
    EXPECT_EQ(render::StudioEffects_ProjectBounds(model, packet_), (std::array<int, 4>{0, 0, 32, 32}));
}

TEST_F(StudioEffectsGlTest, RestoresSeparateFramebuffersAndAllTouchedSamplerBindings)
{
    Wall();
    GLint output = 0;
    glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &output);
    GLuint reader = 0;
    glGenFramebuffers(1, &reader);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, reader);
    GLuint samplers[render::kStudioTouchedTextureUnits]{};
    glGenSamplers(render::kStudioTouchedTextureUnits, samplers);
    for (int unit = 0; unit < render::kStudioTouchedTextureUnits; unit++)
    {
        glBindSampler(unit, samplers[unit]);
    }
    GLuint unpack = 0;
    glGenBuffers(1, &unpack);
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, unpack);
    glBufferData(GL_PIXEL_UNPACK_BUFFER, 4, nullptr, GL_STATIC_DRAW);
    GLuint multisample[2]{};
    glGenTextures(2, multisample);
    for (int index = 0; index < 2; index++)
    {
        glActiveTexture(GL_TEXTURE6 + index);
        glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, multisample[index]);
        glTexImage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE, 4, GL_DEPTH_COMPONENT24, 1, 1, GL_TRUE);
    }
    glActiveTexture(GL_TEXTURE7);
    Capture();
    render::StudioEffects_EndView();
    GLint value = 0;
    glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &value);
    EXPECT_EQ(value, static_cast<GLint>(reader));
    glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &value);
    EXPECT_EQ(value, output);
    glGetIntegerv(GL_ACTIVE_TEXTURE, &value);
    EXPECT_EQ(value, GL_TEXTURE7);
    for (int unit = 0; unit < render::kStudioTouchedTextureUnits; unit++)
    {
        glGetIntegeri_v(GL_SAMPLER_BINDING, unit, &value);
        EXPECT_EQ(value, static_cast<GLint>(samplers[unit]));
        glBindSampler(unit, 0);
    }
    for (int index = 0; index < 2; index++)
    {
        glActiveTexture(GL_TEXTURE6 + index);
        glGetIntegerv(GL_TEXTURE_BINDING_2D_MULTISAMPLE, &value);
        EXPECT_EQ(value, static_cast<GLint>(multisample[index]));
    }
    glBindFramebuffer(GL_READ_FRAMEBUFFER, output);
    EXPECT_GT(Pixel(16, 16)[0], 240);
    glGetIntegerv(GL_PIXEL_UNPACK_BUFFER_BINDING, &value);
    EXPECT_EQ(value, static_cast<GLint>(unpack));
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
    glDeleteBuffers(1, &unpack);
    glDeleteSamplers(render::kStudioTouchedTextureUnits, samplers);
    glDeleteTextures(2, multisample);
    glDeleteFramebuffers(1, &reader);
}

TEST_F(StudioEffectsGlTest, CompositeRespectsCallerClip)
{
    Wall();
    glEnable(GL_SCISSOR_TEST);
    glScissor(20, 3, 16, 32);
    Capture();
    render::StudioEffects_EndView();
    EXPECT_TRUE(glIsEnabled(GL_SCISSOR_TEST));
    EXPECT_EQ(Pixel(12, 16)[0], 0);
    EXPECT_GT(Pixel(20, 16)[0], 240);
}

TEST_F(StudioEffectsGlTest, ReversedDepthRangeComparesInTheCorrectDirection)
{
    glClearDepth(.75);
    glClear(GL_DEPTH_BUFFER_BIT);
    packet_.raster.depth_range[0] = 1;
    packet_.raster.depth_range[1] = 0;
    Capture();
    render::StudioEffects_EndView();
    EXPECT_GT(Pixel(16, 16)[0], 240);
}

TEST_F(StudioEffectsGlTest, GrowingOneEntityMaskPreservesPreviousBodyPartPixels)
{
    Wall();
    packet_.bones = CapturePose({{3, -.3f}}, packet_.bones);
    Capture(1, {1, 5, 22, 22});
    packet_.bones = CapturePose({{3, .3f}}, packet_.bones);
    SetColor("uOccludedColor", {0, 0, 1, 1});
    Capture(1, {9, 5, 22, 22});
    render::StudioEffects_EndView();
    EXPECT_GT(Pixel(6, 16)[0], 240);
    EXPECT_GT(Pixel(26, 16)[2], 240);
}

TEST_F(StudioEffectsGlTest, IndependentEntitiesRetainTheirOwnColors)
{
    Wall();
    packet_.bones = CapturePose({{3, -.3f}}, packet_.bones);
    Capture(1, {0, 0, 32, 32});
    packet_.bones = CapturePose({{3, .3f}}, packet_.bones);
    SetColor("uOccludedColor", {0, 0, 1, 1});
    Capture(2, {0, 0, 32, 32});
    render::StudioEffects_EndView();
    EXPECT_GT(Pixel(6, 16)[0], 240);
    EXPECT_GT(Pixel(26, 16)[2], 240);
}

TEST_F(StudioEffectsGlTest, WeaponFinishNoiseAndSourceAlphaAlsoRemoveEffectCoverage)
{
    Wall();
    packet_.meshes[0].material->program = "weapon_finish";
    SetScalar("uNoise", 1);
    SetScalar("uOutlineWidth", 2);
    Capture();
    render::StudioEffects_EndView();
    EXPECT_EQ(Pixel(16, 16)[0], 0);
    EXPECT_EQ(Pixel(7, 16)[1], 0);
    render::StudioEffects_BeginView();
    SetScalar("uNoise", 0);
    packet_.meshes[0].masked = true;
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, packet_.meshes[0].texture);
    const unsigned char empty[] = {255, 255, 255, 0};
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, empty);
    Capture();
    render::StudioEffects_EndView();
    EXPECT_EQ(Pixel(16, 16)[0], 0);
    EXPECT_EQ(Pixel(7, 16)[1], 0);
}

TEST_F(StudioEffectsGlTest, WidthZeroDisablesHaloAndDoesNotAllocateAMask)
{
    SetScalar("uOccludedAmount", 0);
    SetScalar("uOutlineWidth", 0);
    SetScalar("uGlowRadius", 8);
    SetScalar("uGlowStrength", 2);
    Capture();
    EXPECT_EQ(render::StudioEffects_TargetCount(), 0u);
    render::StudioEffects_EndView();
}

TEST_F(StudioEffectsGlTest, PackedDepthWithoutAStencilAttachmentRetainsFillAndOutline)
{
    SetPackedDepth(GL_DEPTH24_STENCIL8);
    Wall();
    SetScalar("uOutlineWidth", 2);
    Capture();
    render::StudioEffects_EndView();
    EXPECT_GT(Pixel(16, 16)[0], 240);
    EXPECT_GT(Pixel(7, 16)[1], 240);
}

TEST_F(StudioEffectsGlTest, FloatingPackedDepthWithoutAStencilAttachmentRetainsFill)
{
    SetPackedDepth(GL_DEPTH32F_STENCIL8);
    Wall();
    Capture();
    render::StudioEffects_EndView();
    EXPECT_GT(Pixel(16, 16)[0], 240);
}

TEST_F(StudioEffectsGlTest, MultisampledDrawDepthAndGrowingMasksRetainEarlierBodyParts)
{
    GLint maximum = 0;
    glGetIntegerv(GL_MAX_SAMPLES, &maximum);
    if (maximum < 2)
    {
        GTEST_SKIP() << "Multisampling is unavailable";
    }
    GLint output = 0;
    glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &output);
    GLuint framebuffer = 0;
    GLuint buffers[2]{};
    glGenFramebuffers(1, &framebuffer);
    glGenRenderbuffers(2, buffers);
    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
    glBindRenderbuffer(GL_RENDERBUFFER, buffers[0]);
    glRenderbufferStorageMultisample(GL_RENDERBUFFER, 2, GL_RGBA8, 40, 40);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, buffers[0]);
    glBindRenderbuffer(GL_RENDERBUFFER, buffers[1]);
    glRenderbufferStorageMultisample(GL_RENDERBUFFER, 2, GL_DEPTH_COMPONENT24, 40, 40);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, buffers[1]);
    ASSERT_EQ(glCheckFramebufferStatus(GL_FRAMEBUFFER), GLenum(GL_FRAMEBUFFER_COMPLETE));
    glClearDepth(.25);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    packet_.bones = CapturePose({{3, -.3f}}, packet_.bones);
    Capture(1, {1, 5, 22, 22});
    packet_.bones = CapturePose({{3, .3f}}, packet_.bones);
    Capture(1, {9, 5, 22, 22});
    render::StudioEffects_EndView();
    glBindFramebuffer(GL_READ_FRAMEBUFFER, framebuffer);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, output);
    glBlitFramebuffer(0, 0, 40, 40, 0, 0, 40, 40, GL_COLOR_BUFFER_BIT, GL_NEAREST);
    glBindFramebuffer(GL_FRAMEBUFFER, output);
    EXPECT_GT(Pixel(16, 16)[0], 240);
    EXPECT_GT(Pixel(6, 16)[0], 240);
    EXPECT_GT(Pixel(26, 16)[0], 240);
    glDeleteFramebuffers(1, &framebuffer);
    glDeleteRenderbuffers(2, buffers);
}

TEST_F(StudioEffectsGlTest, EarlierNonEffectParentOccludesItsLaterEffectAttachment)
{
    glClearDepth(.5);
    glClear(GL_DEPTH_BUFFER_BIT);
    SetScalar("uOccludedAmount", 0);
    SetColor("uBaseTint", {1, 1, 1, 0});
    PrepareCoveragePacket(render::GetRendererState().coverage, packet_);
    render::StudioEffects_Capture(1, packet_, {5, 5, 22, 22}, true);
    packet_.bones = CapturePose({{11, .5f}}, packet_.bones);
    SetColor("uBaseTint", {1, 1, 1, 1});
    SetScalar("uOccludedAmount", 1);
    Capture();
    render::StudioEffects_EndView();
    EXPECT_EQ(Pixel(16, 16)[0], 0);
}

TEST_F(StudioEffectsGlTest, ClearedEffectColorsUseTheManifestDefaults)
{
    for (const char* program : {"studio", "weapon_finish"})
    {
        packet_.meshes[0].material->program = program;
        packet_.meshes[0].material->params.clear();
        SetScalar("uOccludedAmount", 1);
        SetScalar("uOutlineWidth", 2);
        SetScalar("uGlowRadius", 0);
        ASSERT_TRUE(render::StudioEffects_IsEnabled(*packet_.meshes[0].material));
        glClear(GL_COLOR_BUFFER_BIT);
        Wall();
        Capture();
        render::StudioEffects_EndView();
        EXPECT_GT(Pixel(16, 16)[0], 120) << program;
        EXPECT_GT(Pixel(7, 16)[0], 240) << program;
        render::StudioEffects_BeginView();
    }
}

TEST_F(StudioEffectsGlTest, ClearedGlitchDisplacementUsesTheSameDefaultInProjectedBounds)
{
    render::StudioGpuModel model{};
    model.bounds.push_back({0, {-.25f, -.25f, 0}, {.25f, .25f, 0}});
    for (const char* program : {"studio", "weapon_finish"})
    {
        packet_.meshes[0].material->program = program;
        packet_.meshes[0].material->params.clear();
        SetScalar("uGlitchAmount", 1);
        SetScalar("uOutlineWidth", 0);
        SetScalar("uGlowRadius", 0);
        PrepareCoveragePacket(render::GetRendererState().coverage, packet_);
        EXPECT_EQ(render::StudioEffects_ProjectBounds(model, packet_), (std::array<int, 4>{0, 0, 32, 32})) << program;
    }
}

TEST_F(StudioEffectsGlTest, CheckedProfileBaseAlphaControlsMaskForBothPrograms)
{
    for (const char* program : {"studio", "weapon_finish"})
    {
        packet_.meshes[0].material->program = program;
        packet_.meshes[0].material->params.clear();
        SetScalar("uOccludedAmount", 1);
        SetColor("uBaseTint", {1, 1, 1, 0});
        glClear(GL_COLOR_BUFFER_BIT);
        Wall();
        Capture();
        render::StudioEffects_EndView();
        EXPECT_EQ(Pixel(16, 16)[0], 0) << program;
        render::StudioEffects_BeginView();

        packet_.meshes[0].material->params.clear();
        SetScalar("uOccludedAmount", 1);
        glClear(GL_COLOR_BUFFER_BIT);
        Wall();
        Capture();
        render::StudioEffects_EndView();
        EXPECT_GT(Pixel(16, 16)[0], 120) << program;
        render::StudioEffects_BeginView();
    }
}

TEST_F(StudioEffectsGlTest, FixedProgramsAreResolvedOnceAcrossRepeatedViews)
{
    render::StudioEffects effects(source_.device, source_.shaders, coverage_);
    const unsigned int initial_builds = source_.builds;
    Wall();

    for (int index = 0; index < 3; ++index)
    {
        effects.BeginView();
        PrepareCoveragePacket(coverage_, packet_);
        effects.Capture(1, packet_, {5, 5, 22, 22});
        effects.EndView();
    }

    EXPECT_EQ(source_.builds - initial_builds, 4u);
    effects.Clear();
}
TEST_F(StudioEffectsGlTest, SuppliedDevicesCatalogsAndShaderOwnersRouteIndependently)
{
    EffectSource other;
    performance_test::Core other_coverage_performance;
    render::MaterialCoverageRuntime other_coverage(other.catalog, other.cache, other_coverage_performance.monitor);
    render::StudioEffects first(source_.device, source_.shaders, coverage_);
    render::StudioEffects second(other.device, other.shaders, other_coverage);
    std::string json;
    ASSERT_TRUE(render::GetShaderBackend().LoadSource("studio.json", json));
    ncl_utils::ResultT<ncl_material::ProgramManifest> parsed = ncl_material::ParseProgramManifest(json);
    ASSERT_FALSE(parsed.has_error());
    other.manifests["studio"] = *parsed;
    other.manifests["studio"].render_contract.clear();
    EXPECT_TRUE(first.CheckEnabled(*packet_.meshes[0].material));
    EXPECT_FALSE(second.CheckEnabled(*packet_.meshes[0].material));

    Wall();
    first.BeginView();
    second.BeginView();
    PrepareCoveragePacket(coverage_, packet_);
    first.Capture(1, packet_, {5, 5, 22, 22});
    PrepareCoveragePacket(other_coverage, packet_);
    second.Capture(2, packet_, {5, 5, 22, 22}, true);
    EXPECT_EQ(source_.builds, 1u);
    EXPECT_EQ(other.builds, 1u);
    EXPECT_TRUE(first.Contains(1));
    EXPECT_TRUE(second.Contains(2));
    {
        EffectDeletionProbe deletions;
        source_.device.OnContextDestroyed();
        EXPECT_EQ(deletions.get_calls(), 0u);
    }
    EXPECT_FALSE(first.Contains(1));
    EXPECT_TRUE(second.Contains(2));
    EXPECT_TRUE(render::Device_IsContextCurrent());

    second.EndView();
    EXPECT_EQ(Pixel(16, 16)[0], 0);
    EXPECT_EQ(other.builds, 4u);
    second.Clear();
    other.shaders.Clear();
    other.device.OnContextDestroyed();
    source_.device.OnContextCreated();
}

TEST_F(StudioEffectsGlTest, FailedFixedProgramsStayCachedUntilTheirShaderGenerationChanges)
{
    render::StudioEffects effects(source_.device, source_.shaders, coverage_);
    source_.failed_fragment = "studio_effect_mask.frag";
    Wall();

    for (int index = 0; index < 3; ++index)
    {
        effects.BeginView();
        PrepareCoveragePacket(coverage_, packet_);
        effects.Capture(1, packet_, {5, 5, 22, 22});
        EXPECT_FALSE(effects.Contains(1));
        effects.EndView();
    }
    EXPECT_EQ(source_.builds, 1u);

    source_.failed_fragment.clear();
    effects.BeginView();
    PrepareCoveragePacket(coverage_, packet_);
    effects.Capture(1, packet_, {5, 5, 22, 22});
    EXPECT_FALSE(effects.Contains(1));
    effects.EndView();
    EXPECT_EQ(source_.builds, 1u);

    source_.shaders.Clear();
    effects.BeginView();
    PrepareCoveragePacket(coverage_, packet_);
    effects.Capture(1, packet_, {5, 5, 22, 22});
    EXPECT_TRUE(effects.Contains(1));
    effects.EndView();
    EXPECT_EQ(source_.builds, 5u);
    EXPECT_GT(Pixel(16, 16)[0], 200);
    effects.Clear();
}

TEST_F(StudioEffectsGlTest, CompositionFailureAndSuccessRetryAllFixedProgramsAfterGenerationChange)
{
    render::StudioEffects effects(source_.device, source_.shaders, coverage_);
    source_.failed_fragment = "studio_effect_seed.frag";
    Wall();

    for (int index = 0; index < 3; ++index)
    {
        effects.BeginView();
        PrepareCoveragePacket(coverage_, packet_);
        effects.Capture(1, packet_, {5, 5, 22, 22});
        effects.EndView();
    }
    EXPECT_EQ(source_.builds, 4u);
    EXPECT_EQ(Pixel(16, 16)[0], 0);

    source_.failed_fragment.clear();
    source_.shaders.Clear();
    effects.BeginView();
    PrepareCoveragePacket(coverage_, packet_);
    effects.Capture(1, packet_, {5, 5, 22, 22});
    effects.EndView();
    EXPECT_EQ(source_.builds, 8u);
    EXPECT_GT(Pixel(16, 16)[0], 200);

    source_.shaders.Clear();
    effects.BeginView();
    PrepareCoveragePacket(coverage_, packet_);
    effects.Capture(2, packet_, {5, 5, 22, 22});
    effects.EndView();
    EXPECT_EQ(source_.builds, 12u);
    EXPECT_GT(Pixel(16, 16)[0], 200);
    effects.Clear();
}

TEST_F(StudioEffectsGlTest, PreparedBindingsRefreshAfterReloadAndStayWarmAcrossMeshesAndViews)
{
    render::StudioEffects effects(source_.device, source_.shaders, coverage_);
    packet_.meshes.resize(3, packet_.meshes.front());
    ShaderBindingTrace trace;
    unsigned int cold_uniforms = 0;
    unsigned int cold_blocks = 0;

    for (int generation = 0; generation < 2; ++generation)
    {
        for (int view = 0; view < 3; ++view)
        {
            Wall();
            effects.BeginView();
            PrepareCoveragePacket(coverage_, packet_);
            effects.Capture(1, packet_, {5, 5, 22, 22});
            effects.EndView();
            EXPECT_GT(Pixel(16, 16)[0], 200);

            if (view == 0)
            {
                EXPECT_GT(trace.get_uniforms(), cold_uniforms);
                EXPECT_GT(trace.get_blocks(), cold_blocks);
                cold_uniforms = trace.get_uniforms();
                cold_blocks = trace.get_blocks();
            }
            else
            {
                EXPECT_EQ(trace.get_uniforms(), cold_uniforms);
                EXPECT_EQ(trace.get_blocks(), cold_blocks);
            }
        }

        source_.shaders.Clear();
    }

    effects.Clear();
}

TEST_F(StudioEffectsGlTest, CancelViewPreservesExternalViewsAndCannotCancelAReplacementAfterClear)
{
    render::StudioEffects effects(source_.device, source_.shaders, coverage_);
    const uint64_t outer = effects.BeginView();
    const uint64_t own = effects.BeginView();
    PrepareCoveragePacket(coverage_, packet_);
    effects.Capture(1, packet_, {5, 5, 22, 22});
    ASSERT_EQ(effects.get_view_count(), 2u);
    ASSERT_EQ(effects.get_target_count(), 1u);
    effects.CancelView(own);
    EXPECT_EQ(effects.get_view_count(), 1u);
    EXPECT_EQ(effects.get_target_count(), 0u);
    EXPECT_EQ(Pixel(16, 16)[0], 0);
    effects.Clear();
    const uint64_t replacement = effects.BeginView();
    EXPECT_NE(replacement, outer);
    EXPECT_NE(replacement, own);
    effects.CancelView(own);
    effects.CancelView(outer);
    EXPECT_EQ(effects.get_view_count(), 1u);
    effects.EndView();
}
