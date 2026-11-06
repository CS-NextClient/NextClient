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
#include "graphics/render/effects/StudioEffectPasses.h"
#include "graphics/render/ScopedGlState.h"
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

    class EffectReplayProbe
    {
    private:
        PFNGLDRAWELEMENTSPROC draw_elements_ = glDrawElements;
        PFNGLBLITFRAMEBUFFERPROC blit_ = glBlitFramebuffer;
        PFNGLPUSHATTRIBPROC push_ = glPushAttrib;

        inline static EffectReplayProbe* active_{};

    public:
        unsigned int elements{};
        unsigned int copies{};
        unsigned int scopes{};

        EffectReplayProbe()
        {
            active_ = this;
            glDrawElements = Draw;
            glBlitFramebuffer = Blit;
            glPushAttrib = Push;
        }

        ~EffectReplayProbe()
        {
            glDrawElements = draw_elements_;
            glBlitFramebuffer = blit_;
            glPushAttrib = push_;
            active_ = nullptr;
        }

    private:
        static void APIENTRY Draw(GLenum mode, GLsizei count, GLenum type, const void* indices)
        {
            ++active_->elements;
            active_->draw_elements_(mode, count, type, indices);
        }

        static void APIENTRY Blit(
            GLint a,
            GLint b,
            GLint c,
            GLint d,
            GLint e,
            GLint f,
            GLint g,
            GLint h,
            GLbitfield bits,
            GLenum filter
        )
        {
            ++active_->copies;
            active_->blit_(a, b, c, d, e, f, g, h, bits, filter);
        }

        static void APIENTRY Push(GLbitfield bits)
        {
            ++active_->scopes;
            active_->push_(bits);
        }
    };

    class EffectWorkProbe
    {
    private:
        PFNGLDRAWARRAYSPROC draw_arrays_;
        PFNGLCLEARPROC clear_;
        PFNGLTEXIMAGE2DPROC tex_image_;

        inline static EffectWorkProbe* active_{};

    public:
        unsigned int draws{};
        unsigned int depth_clears{};
        unsigned int seed_allocations{};
        int largest_draw_area{};
        int largest_seed_area{};

        EffectWorkProbe() :
            draw_arrays_(glDrawArrays),
            clear_(glClear),
            tex_image_(glTexImage2D)
        {
            active_ = this;
            glDrawArrays = DrawArrays;
            glClear = Clear;
            glTexImage2D = TexImage;
        }

        ~EffectWorkProbe()
        {
            glDrawArrays = draw_arrays_;
            glClear = clear_;
            glTexImage2D = tex_image_;
            active_ = nullptr;
        }

    private:
        static void APIENTRY DrawArrays(GLenum mode, GLint first, GLsizei count)
        {
            GLint viewport[4]{};
            glGetIntegerv(GL_VIEWPORT, viewport);
            int area = viewport[2] * viewport[3];

            if (glIsEnabled(GL_SCISSOR_TEST))
            {
                GLint clip[4]{};
                glGetIntegerv(GL_SCISSOR_BOX, clip);
                const int wide = std::max(0, std::min(viewport[0] + viewport[2], clip[0] + clip[2]) - std::max(viewport[0], clip[0]));
                const int tall = std::max(0, std::min(viewport[1] + viewport[3], clip[1] + clip[3]) - std::max(viewport[1], clip[1]));
                area = wide * tall;
            }

            ++active_->draws;
            active_->largest_draw_area = std::max(active_->largest_draw_area, area);
            active_->draw_arrays_(mode, first, count);
        }

        static void APIENTRY Clear(GLbitfield bits)
        {
            active_->depth_clears += (bits & GL_DEPTH_BUFFER_BIT) != 0 ? 1 : 0;
            active_->clear_(bits);
        }

        static void APIENTRY TexImage(
            GLenum target,
            GLint level,
            GLint format,
            GLsizei wide,
            GLsizei tall,
            GLint border,
            GLenum input,
            GLenum type,
            const void* data
        )
        {
            if (format == GL_RG32I)
            {
                ++active_->seed_allocations;
                active_->largest_seed_area = std::max(active_->largest_seed_area, wide * tall);
            }

            active_->tex_image_(target, level, format, wide, tall, border, input, type, data);
        }
    };


    class EffectAllocationProbe
    {
    public:
        struct Storage
        {
            GLuint image{};
            int wide{};
            int tall{};
            int samples{};
            GLenum format{};
        };

    private:
        PFNGLGENTEXTURESPROC gen_textures_ = glGenTextures;
        PFNGLGENFRAMEBUFFERSPROC gen_framebuffers_ = glGenFramebuffers;
        PFNGLTEXIMAGE2DPROC tex_image_ = glTexImage2D;
        PFNGLTEXIMAGE2DMULTISAMPLEPROC tex_image_ms_ = glTexImage2DMultisample;
        PFNGLDELETETEXTURESPROC delete_textures_ = glDeleteTextures;
        PFNGLDELETEFRAMEBUFFERSPROC delete_framebuffers_ = glDeleteFramebuffers;

        inline static EffectAllocationProbe* active_{};

    public:
        std::vector<GLuint> textures;
        std::vector<GLuint> framebuffers;
        std::vector<GLuint> deleted_textures;
        std::vector<GLuint> deleted_framebuffers;
        std::vector<Storage> storage;

        EffectAllocationProbe()
        {
            active_ = this;
            glGenTextures = GenTextures;
            glGenFramebuffers = GenFramebuffers;
            glTexImage2D = TexImage;
            glTexImage2DMultisample = TexImageMs;
            glDeleteTextures = DeleteTextures;
            glDeleteFramebuffers = DeleteFramebuffers;
        }

        ~EffectAllocationProbe()
        {
            glGenTextures = gen_textures_;
            glGenFramebuffers = gen_framebuffers_;
            glTexImage2D = tex_image_;
            glTexImage2DMultisample = tex_image_ms_;
            glDeleteTextures = delete_textures_;
            glDeleteFramebuffers = delete_framebuffers_;
            active_ = nullptr;
        }

    private:
        static void APIENTRY GenTextures(GLsizei count, GLuint* names)
        {
            active_->gen_textures_(count, names);
            active_->textures.insert(active_->textures.end(), names, names + count);
        }

        static void APIENTRY GenFramebuffers(GLsizei count, GLuint* names)
        {
            active_->gen_framebuffers_(count, names);
            active_->framebuffers.insert(active_->framebuffers.end(), names, names + count);
        }

        static void APIENTRY DeleteTextures(GLsizei count, const GLuint* names)
        {
            active_->deleted_textures.insert(active_->deleted_textures.end(), names, names + count);
            active_->delete_textures_(count, names);
        }

        static void APIENTRY DeleteFramebuffers(GLsizei count, const GLuint* names)
        {
            active_->deleted_framebuffers.insert(active_->deleted_framebuffers.end(), names, names + count);
            active_->delete_framebuffers_(count, names);
        }

        static void APIENTRY TexImage(
            GLenum target,
            GLint level,
            GLint format,
            GLsizei wide,
            GLsizei tall,
            GLint border,
            GLenum input,
            GLenum type,
            const void* data
        )
        {
            GLint image = 0;
            glGetIntegerv(GL_TEXTURE_BINDING_2D, &image);
            active_->storage.push_back({static_cast<GLuint>(image), wide, tall, 0, static_cast<GLenum>(format)});
            active_->tex_image_(target, level, format, wide, tall, border, input, type, data);
        }

        static void APIENTRY TexImageMs(GLenum target, GLsizei samples, GLenum format, GLsizei wide, GLsizei tall, GLboolean fixed)
        {
            GLint image = 0;
            glGetIntegerv(GL_TEXTURE_BINDING_2D_MULTISAMPLE, &image);
            active_->storage.push_back({static_cast<GLuint>(image), wide, tall, samples, format});
            active_->tex_image_ms_(target, samples, format, wide, tall, fixed);
        }
    };

    struct EffectMaskNames
    {
        std::array<GLuint, 5> images;
        std::array<GLuint, 2> framebuffers;

        explicit EffectMaskNames(const render::StudioEffectTargets::Mask& mask) :
            images{mask.get_fill(), mask.get_outline(), mask.get_shape(), 0, mask.get_object_depth()},
            framebuffers{mask.get_framebuffer(), mask.get_depth_framebuffer()}
        {
            glBindFramebuffer(GL_FRAMEBUFFER, mask.get_framebuffer());
            GLint image = 0;
            glGetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME, &image);
            images[3] = static_cast<GLuint>(image);
        }

        void ExpectLive(bool live) const
        {
            for (GLuint image : images)
            {
                EXPECT_EQ(glIsTexture(image) != 0, live) << image;
            }

            for (GLuint framebuffer : framebuffers)
            {
                EXPECT_EQ(glIsFramebuffer(framebuffer) != 0, live) << framebuffer;
            }
        }
    };

    class EffectTargetFixture
    {
    private:
        render::ScopedGlState saved_;

    public:
        render::StudioEffectTargets targets;
        render::StudioEffectTargets::Masks_t masks;
        render::StudioEffectTargets::Masks_t nested;

        explicit EffectTargetFixture(render::RenderDevice& device) :
            targets(device)
        {}

        ~EffectTargetFixture()
        {
            targets.ReleaseMasks(masks, true);
            targets.ReleaseMasks(nested, true);
            targets.Clear(true);
        }
    };

    std::array<float, 4> MaskPixel(GLuint framebuffer, GLenum attachment, int x, int y)
    {
        glBindFramebuffer(GL_READ_FRAMEBUFFER, framebuffer);
        glReadBuffer(attachment);
        std::array<float, 4> pixel{};
        glReadPixels(x, y, 1, 1, GL_RGBA, GL_FLOAT, pixel.data());

        return pixel;
    }

    float MaskDepth(GLuint framebuffer, int x, int y)
    {
        glBindFramebuffer(GL_READ_FRAMEBUFFER, framebuffer);
        float depth = 0;
        glReadPixels(x, y, 1, 1, GL_DEPTH_COMPONENT, GL_FLOAT, &depth);

        return depth;
    }

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

        void CheckDirectDepth(bool multisampled)
        {
            GLint output = 0;
            glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &output);
            GLuint framebuffer = 0;
            GLuint buffers[2]{};

            if (multisampled)
            {
                GLint maximum = 0;
                glGetIntegerv(GL_MAX_SAMPLES, &maximum);

                if (maximum < 2)
                {
                    GTEST_SKIP() << "Multisampling is unavailable";
                }

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
            }

            glClearDepth(.25);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            SetScalar("uOutlineWidth", 2);
            packet_.bones = CapturePose({{3, -.3f}}, packet_.bones);
            Capture(1, {1, 5, 22, 22});
            packet_.bones = CapturePose({{3, .3f}}, packet_.bones);
            Capture(1, {9, 5, 22, 22});
            SetColor("uOccludedColor", {0, 0, 1, 1});
            packet_.bones = CapturePose({{0, .25f}, {3, 0.0f}, {5, .25f}, {10, 1.0f}}, packet_.bones);
            Capture(2, {10, 10, 12, 12});

            {
                EffectReplayProbe replay;
                EffectAllocationProbe allocations;
                render::StudioEffects_EndView();

                EXPECT_EQ(replay.copies, 1u);
                EXPECT_EQ(replay.elements, 6u);

                const size_t depth_storage = std::count_if(
                    allocations.storage.begin(),
                    allocations.storage.end(),
                    [](const EffectAllocationProbe::Storage& image) {
                        return image.format == GL_DEPTH_COMPONENT24;
                    }
                );

                EXPECT_EQ(depth_storage, 3u);
            }

            if (multisampled)
            {
                glBindFramebuffer(GL_READ_FRAMEBUFFER, framebuffer);
                glBindFramebuffer(GL_DRAW_FRAMEBUFFER, output);
                glBlitFramebuffer(0, 0, 40, 40, 0, 0, 40, 40, GL_COLOR_BUFFER_BIT, GL_NEAREST);
                glBindFramebuffer(GL_FRAMEBUFFER, output);
            }

            EXPECT_GT(Pixel(6, 16)[0], 240);
            EXPECT_GT(Pixel(26, 16)[0], 240);
            EXPECT_GT(Pixel(16, 16)[2], 240);
            EXPECT_GT(Pixel(12, 16)[1], 200);

            glDeleteFramebuffers(1, &framebuffer);
            glDeleteRenderbuffers(2, buffers);
        }

        std::array<unsigned char, 4> Pixel(int x, int y)
        {
            std::array<unsigned char, 4> pixel{};
            glReadPixels(x + 4, y + 3, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixel.data());
            return pixel;
        }
    };
} // namespace

TEST_F(StudioEffectsGlTest, DirectDepthSingleSampleHasOneSceneCopyAndNoCroppedObjectStorage)
{
    CheckDirectDepth(false);
}

TEST_F(StudioEffectsGlTest, DirectDepthMultisampleHasOneSceneCopyAndNoCroppedObjectStorage)
{
    CheckDirectDepth(true);
}

TEST_F(StudioEffectsGlTest, DirectDepthPoolSeparatesImmediateMasksAndClearsReversedEntityDepth)
{
    render::StudioEffects effects(source_.device, source_.shaders, coverage_);

    for (int frame = 0; frame < 3; ++frame)
    {
        const bool deferred = frame != 1;
        glClearDepth(.75);
        glDepthMask(GL_TRUE);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        effects.BeginView();
        packet_.raster.depth_range[0] = 1;
        packet_.raster.depth_range[1] = 0;
        SetScalar("uOutlineWidth", 2);
        SetScalar("uOutlineMode", 1);
        SetColor("uOccludedColor", {1, 0, 0, 1});
        packet_.bones = CapturePose({{0, 1.0f}, {5, 1.0f}, {11, -.6f}}, packet_.bones);
        PrepareCoveragePacket(coverage_, packet_);

        {
            EffectReplayProbe replay;
            effects.Capture(1, packet_, {5, 5, 22, 22}, false, deferred);
            SetColor("uOccludedColor", {0, 0, 1, 1});
            packet_.bones = CapturePose({{0, .25f}, {5, .25f}, {11, 0.0f}}, packet_.bones);
            PrepareCoveragePacket(coverage_, packet_);
            effects.Capture(2, packet_, {10, 10, 12, 12}, false, deferred);
            effects.EndView();

            EXPECT_EQ(replay.copies, deferred ? 1u : 5u);
        }

        EXPECT_EQ(Pixel(10, 16)[0], 0);
        EXPECT_GT(Pixel(6, 16)[1], 200);
        EXPECT_GT(Pixel(16, 16)[2], 240);
        EXPECT_GT(Pixel(12, 16)[1], 200);
    }

    effects.Clear();
}

TEST_F(StudioEffectsGlTest, DirectDepthMultisampleCoverageSuppressesOutlineOutsideTheColorMask)
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

    glEnable(GL_MULTISAMPLE);
    glClearDepth(.25);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    GLint samples = 0;
    glGetIntegerv(GL_SAMPLES, &samples);
    float rightmost_sample = 0;

    for (int index = 0; index < samples; ++index)
    {
        GLfloat position[2]{};
        glGetMultisamplefv(GL_SAMPLE_POSITION, index, position);
        rightmost_sample = std::max(rightmost_sample, position[0]);
    }

    ASSERT_GT(rightmost_sample, .5f);

    const float edge_fraction = (.5f + rightmost_sample) * .5f;

    Capture(1, {0, 0, 32, 32});
    SetScalar("uOutlineWidth", 2);
    SetColor("uOccludedColor", {0, 0, 1, 1});
    packet_.bones = CapturePose({{0, .25f}, {3, edge_fraction / 16}, {5, .25f}}, packet_.bones);
    Capture(2, {10, 10, 12, 12});
    render::StudioEffects_EndView();
    glBindFramebuffer(GL_READ_FRAMEBUFFER, framebuffer);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, output);
    glBlitFramebuffer(0, 0, 40, 40, 0, 0, 40, 40, GL_COLOR_BUFFER_BIT, GL_NEAREST);
    glBindFramebuffer(GL_FRAMEBUFFER, output);

    const std::array<unsigned char, 4> edge = Pixel(14, 16);

    EXPECT_GT(edge[0], 240);
    EXPECT_LT(edge[1], 10);
    EXPECT_LT(edge[2], 10);
    EXPECT_GT(Pixel(16, 16)[2], 240);
    EXPECT_GT(Pixel(13, 16)[1], 200);

    glDeleteFramebuffers(1, &framebuffer);
    glDeleteRenderbuffers(2, buffers);
}

TEST_F(StudioEffectsGlTest, CaptureQueuesOwnedPacketsWithoutGpuWork)
{
    render::StudioEffects effects(source_.device, source_.shaders, coverage_);
    Wall();
    effects.BeginView();
    PrepareCoveragePacket(coverage_, packet_);
    const size_t builds = source_.builds;

    {
        EffectAllocationProbe allocations;
        EffectWorkProbe work;
        EffectReplayProbe replay;
        effects.Capture(1, packet_, {0, 0, 32, 32});
        effects.Capture(2, packet_, {0, 0, 32, 32});

        EXPECT_TRUE(effects.Contains(1));
        EXPECT_TRUE(effects.Contains(2));
        EXPECT_EQ(effects.get_target_count(), 2u);
        EXPECT_TRUE(allocations.storage.empty());
        EXPECT_TRUE(allocations.textures.empty());
        EXPECT_TRUE(allocations.framebuffers.empty());
        EXPECT_EQ(work.depth_clears, 0u);
        EXPECT_EQ(work.draws, 0u);
        EXPECT_EQ(replay.elements, 0u);
        EXPECT_EQ(replay.copies, 0u);
        EXPECT_EQ(replay.scopes, 0u);
        EXPECT_EQ(source_.builds, builds);
    }

    effects.EndView();

    EXPECT_GT(Pixel(16, 16)[0], 200);
    EXPECT_EQ(effects.get_last_stats().entities, 2u);
    EXPECT_EQ(effects.get_last_stats().packets, 2u);

    effects.Clear();
}

TEST_F(StudioEffectsGlTest, StreamingReusesOneTargetAcrossSixtyFourEntitiesAndFrames)
{
    GLuint image = 0;
    GLuint depth = 0;
    GLuint output = 0;
    glGenTextures(1, &image);
    glBindTexture(GL_TEXTURE_2D, image);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 128, 128, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glGenRenderbuffers(1, &depth);
    glBindRenderbuffer(GL_RENDERBUFFER, depth);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, 128, 128);
    glGenFramebuffers(1, &output);
    glBindFramebuffer(GL_FRAMEBUFFER, output);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, image, 0);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depth);

    ASSERT_EQ(glCheckFramebufferStatus(GL_FRAMEBUFFER), GLenum(GL_FRAMEBUFFER_COMPLETE));

    glViewport(0, 0, 128, 128);
    packet_.viewport = {0, 0, 128, 128};
    PrepareCoveragePacket(coverage_, packet_);
    render::StudioEffects single(source_.device, source_.shaders, coverage_);
    render::StudioEffects effects(source_.device, source_.shaders, coverage_);
    size_t single_storage = 0;
    size_t single_images = 0;
    size_t single_framebuffers = 0;

    for (int frame = -1; frame < 3; ++frame)
    {
        glClearDepth(.25);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        render::StudioEffects& owner = frame == -1 ? single : effects;
        owner.BeginView();

        {
            EffectAllocationProbe allocations;
            EffectReplayProbe replay;
            const int count = frame == -1 ? 1 : 64;

            for (int index = 0; index < count; ++index)
            {
                owner.Capture(1 + frame * 64 + index, packet_, {0, 0, 128, 128});
            }

            owner.EndView();

            EXPECT_EQ(replay.scopes, 1u);
            EXPECT_EQ(replay.elements, static_cast<unsigned int>(count * 2));

            if (frame == -1)
            {
                single_storage = allocations.storage.size();
                single_images = allocations.textures.size();
                single_framebuffers = allocations.framebuffers.size();
            }
            else if (frame == 0)
            {
                EXPECT_EQ(allocations.storage.size(), single_storage);
                EXPECT_EQ(allocations.textures.size(), single_images);
                EXPECT_EQ(allocations.framebuffers.size(), single_framebuffers);
            }
            else
            {
                EXPECT_TRUE(allocations.storage.empty());
                EXPECT_TRUE(allocations.textures.empty());
                EXPECT_TRUE(allocations.framebuffers.empty());
                EXPECT_TRUE(allocations.deleted_textures.empty());
                EXPECT_TRUE(allocations.deleted_framebuffers.empty());
            }
        }

        if (frame >= 0)
        {
            EXPECT_EQ(owner.get_last_stats().entities, 64u);
            EXPECT_EQ(owner.get_last_stats().packets, 64u);
            EXPECT_EQ(owner.get_last_stats().mask_pixels, 1048576u);
            EXPECT_EQ(owner.get_last_stats().reused_targets, frame == 0 ? 63u : 64u);
            EXPECT_EQ(owner.get_target_count(), 0u);
        }
    }

    effects.Clear();
    single.Clear();
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glDeleteFramebuffers(1, &output);
    glDeleteTextures(1, &image);
    glDeleteRenderbuffers(1, &depth);
}

TEST_F(StudioEffectsGlTest, InterleavedPartsPreserveFirstEntityOrderAndOwnedSnapshots)
{
    render::StudioEffects effects(source_.device, source_.shaders, coverage_);
    Wall();
    effects.BeginView();
    SetColor("uOccludedColor", {1, 0, 0, .5f});
    packet_.bones = CapturePose({{3, -.3f}}, packet_.bones);
    PrepareCoveragePacket(coverage_, packet_);
    effects.Capture(1, packet_, {0, 0, 32, 32});
    SetColor("uOccludedColor", {0, 0, 1, .5f});
    packet_.bones = CapturePose({{3, .3f}}, packet_.bones);
    PrepareCoveragePacket(coverage_, packet_);
    effects.Capture(2, packet_, {0, 0, 32, 32});
    SetColor("uOccludedColor", {1, 0, 0, .5f});
    PrepareCoveragePacket(coverage_, packet_);
    effects.Capture(1, packet_, {0, 0, 32, 32});
    packet_.meshes.clear();
    packet_.bones = {};
    packet_.projection = {};
    packet_.modelview = {};
    effects.EndView();
    const std::array<unsigned char, 4> overlap = Pixel(22, 16);

    EXPECT_NEAR(overlap[0], 64, 3);
    EXPECT_NEAR(overlap[2], 128, 3);
    EXPECT_NEAR(Pixel(5, 16)[0], 128, 3);
    EXPECT_EQ(effects.get_last_stats().entities, 2u);
    EXPECT_EQ(effects.get_last_stats().packets, 3u);

    effects.Clear();
}

TEST_F(StudioEffectsGlTest, DeferredSnapshotsKeepPreparedCoverageImageIdentities)
{
    render::StudioEffects effects(source_.device, source_.shaders, coverage_);
    GLuint images[2]{};
    glGenTextures(2, images);
    const unsigned char values[2][4] = {{255, 255, 255, 255}, {0, 0, 0, 255}};

    for (int index = 0; index < 2; ++index)
    {
        glBindTexture(GL_TEXTURE_2D, images[index]);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, values[index]);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    }

    Wall();
    SetScalar("uDissolve", .5f);
    effects.BeginView();

    for (int index = 0; index < 2; ++index)
    {
        packet_.bones = CapturePose({{3, index == 0 ? -.3f : .3f}}, packet_.bones);
        PrepareCoveragePacket(coverage_, packet_);
        packet_.meshes[0].images.coverage_dissolve_map = images[index];
        effects.Capture(1, packet_, {0, 0, 32, 32});
    }

    packet_.meshes[0].images = {};
    packet_.meshes.clear();
    packet_.bones = {};
    effects.EndView();

    EXPECT_GT(Pixel(5, 16)[0], 240);
    EXPECT_EQ(Pixel(26, 16)[0], 0);

    effects.Clear();
    glDeleteTextures(2, images);
}

TEST_F(StudioEffectsGlTest, StreamingKeepsTheInnerSilhouetteOfAnIndependentEntity)
{
    render::StudioEffects effects(source_.device, source_.shaders, coverage_);
    Wall();
    effects.BeginView();
    PrepareCoveragePacket(coverage_, packet_);
    effects.Capture(1, packet_, {0, 0, 32, 32});
    SetScalar("uOccludedAmount", 0);
    SetScalar("uOutlineWidth", 2);
    SetColor("uOutlineColor", {0, 0, 1, 1});
    packet_.bones = CapturePose({{0, .5f}, {5, .5f}}, packet_.bones);
    PrepareCoveragePacket(coverage_, packet_);
    effects.Capture(2, packet_, {0, 0, 32, 32});
    effects.EndView();

    EXPECT_GT(Pixel(11, 16)[2], 240);
    EXPECT_GT(Pixel(16, 16)[0], 240);

    effects.Clear();
}

TEST_F(StudioEffectsGlTest, ResourceBarrierMaterializesEveryViewWithoutEarlyCompositionAndRetainsLaterParts)
{
    render::StudioEffects effects(source_.device, source_.shaders, coverage_);
    Wall();
    effects.BeginView();
    packet_.bones = CapturePose({{3, -.3f}}, packet_.bones);
    PrepareCoveragePacket(coverage_, packet_);
    effects.Capture(1, packet_, {0, 5, 22, 22});
    effects.BeginView();
    SetColor("uOccludedColor", {0, 0, 1, 1});
    PrepareCoveragePacket(coverage_, packet_);
    effects.Capture(2, packet_, {0, 5, 22, 22});

    {
        EffectReplayProbe replay;
        EffectWorkProbe work;
        effects.FlushPendingCaptures();

        EXPECT_EQ(replay.elements, 4u);
        EXPECT_EQ(work.draws, 0u);
        EXPECT_EQ(Pixel(5, 16)[0], 0);
        EXPECT_EQ(Pixel(5, 16)[2], 0);
    }

    source_.cache.Clear();

    {
        EffectReplayProbe replay;
        effects.EndView();

        EXPECT_EQ(replay.elements, 0u);
    }

    EXPECT_GT(Pixel(5, 16)[2], 240);

    SetColor("uOccludedColor", {0, 1, 0, 1});
    packet_.bones = CapturePose({{3, .3f}}, packet_.bones);
    PrepareCoveragePacket(coverage_, packet_);
    effects.Capture(1, packet_, {9, 5, 23, 22});
    effects.EndView();

    EXPECT_GT(Pixel(5, 16)[0], 240);
    EXPECT_GT(Pixel(26, 16)[1], 240);
    EXPECT_EQ(effects.get_last_stats().packets, 2u);

    effects.Clear();
}

TEST_F(StudioEffectsGlTest, ImmediateCaptureAndSessionDiscardPreserveScopeIdentities)
{
    render::StudioEffects effects(source_.device, source_.shaders, coverage_);
    const uint64_t outer = effects.BeginView();
    PrepareCoveragePacket(coverage_, packet_);
    effects.Capture(1, packet_, {5, 5, 22, 22});

    {
        EffectReplayProbe replay;
        effects.Capture(1, packet_, {5, 5, 22, 22}, false, false);

        EXPECT_EQ(replay.elements, 4u);
    }

    const uint64_t inner = effects.BeginView();
    effects.Capture(2, packet_, {5, 5, 22, 22});

    {
        EffectReplayProbe replay;
        effects.DiscardCaptures();

        EXPECT_EQ(replay.elements, 0u);
        EXPECT_EQ(replay.copies, 0u);
    }

    EXPECT_EQ(effects.get_view_count(), 2u);
    EXPECT_EQ(effects.get_target_count(), 0u);

    effects.CancelView(inner);
    effects.CancelView(outer);

    EXPECT_EQ(effects.get_view_count(), 0u);

    effects.Clear();
}

TEST_F(StudioEffectsGlTest, StaleTextureRevisionDiscardsPendingGeometryBeforeReplay)
{
    render::StudioEffects effects(source_.device, source_.shaders, coverage_);
    effects.BeginView();
    PrepareCoveragePacket(coverage_, packet_);
    effects.Capture(1, packet_, {5, 5, 22, 22});
    source_.cache.Clear();
    EffectReplayProbe replay;
    effects.EndView();

    EXPECT_EQ(replay.elements, 0u);
    EXPECT_EQ(effects.get_last_stats().entities, 0u);
    EXPECT_EQ(effects.get_last_stats().packets, 0u);

    effects.Clear();
}

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

TEST_F(StudioEffectsGlTest, NestedOwnerViewsKeepOuterPacketsIndependentAndReuseTheCompletedInnerMask)
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
    EXPECT_EQ(effects.get_last_stats().reused_targets, 1u);

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
        EffectReplayProbe replay;
        EffectWorkProbe work;
        source_.device.OnContextDestroyed();
        effects.EndView();
        other.EndView();

        EXPECT_EQ(deletions.get_calls(), 0u);
        EXPECT_EQ(replay.elements, 0u);
        EXPECT_EQ(replay.copies, 0u);
        EXPECT_EQ(replay.scopes, 0u);
        EXPECT_EQ(work.draws, 0u);
        EXPECT_EQ(work.depth_clears, 0u);
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

    effects->EndView();
    effects->BeginView();
    effects->Capture(2, packet_, {5, 5, 22, 22});
    EffectReplayProbe replay;
    EffectDeletionProbe deletions;
    effects.reset();
    EXPECT_EQ(deletions.get_calls(), 0u);
    EXPECT_EQ(replay.elements, 0u);
    EXPECT_EQ(replay.copies, 0u);
    EXPECT_EQ(replay.scopes, 0u);
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

    EXPECT_EQ(source_.builds - initial_builds, 2u);

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

    EXPECT_EQ(source_.builds, 0u);
    EXPECT_EQ(other.builds, 0u);
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
    EXPECT_EQ(other.builds, 2u);

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

        EXPECT_TRUE(effects.Contains(1));

        effects.EndView();
    }

    EXPECT_EQ(source_.builds, 2u);

    source_.failed_fragment.clear();
    effects.BeginView();
    PrepareCoveragePacket(coverage_, packet_);
    effects.Capture(1, packet_, {5, 5, 22, 22});

    EXPECT_TRUE(effects.Contains(1));

    effects.EndView();

    EXPECT_EQ(source_.builds, 2u);

    source_.shaders.Clear();
    effects.BeginView();
    PrepareCoveragePacket(coverage_, packet_);
    effects.Capture(1, packet_, {5, 5, 22, 22});
    EXPECT_TRUE(effects.Contains(1));
    effects.EndView();

    EXPECT_EQ(source_.builds, 4u);
    EXPECT_GT(Pixel(16, 16)[0], 200);
    effects.Clear();
}

TEST_F(StudioEffectsGlTest, CompositionFailureAndSuccessRetryAllFixedProgramsAfterGenerationChange)
{
    SetScalar("uOutlineWidth", 3);
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

    EXPECT_EQ(source_.builds, 3u);
    EXPECT_EQ(Pixel(16, 16)[0], 0);

    source_.failed_fragment.clear();
    source_.shaders.Clear();
    effects.BeginView();
    PrepareCoveragePacket(coverage_, packet_);
    effects.Capture(1, packet_, {5, 5, 22, 22});
    effects.EndView();

    EXPECT_EQ(source_.builds, 7u);
    EXPECT_GT(Pixel(16, 16)[0], 200);

    source_.shaders.Clear();
    effects.BeginView();
    PrepareCoveragePacket(coverage_, packet_);
    effects.Capture(2, packet_, {5, 5, 22, 22});
    effects.EndView();

    EXPECT_EQ(source_.builds, 11u);
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
    Wall();
    const uint64_t outer = effects.BeginView();
    PrepareCoveragePacket(coverage_, packet_);
    effects.Capture(1, packet_, {5, 5, 22, 22});
    const uint64_t own = effects.BeginView();
    SetColor("uOccludedColor", {0, 0, 1, 1});
    PrepareCoveragePacket(coverage_, packet_);
    effects.Capture(1, packet_, {5, 5, 22, 22});
    ASSERT_EQ(effects.get_view_count(), 2u);
    ASSERT_EQ(effects.get_target_count(), 1u);

    {
        EffectReplayProbe replay;
        effects.CancelView(own);

        EXPECT_EQ(replay.elements, 0u);
        EXPECT_EQ(replay.copies, 0u);
    }

    EXPECT_EQ(effects.get_view_count(), 1u);
    EXPECT_EQ(effects.get_target_count(), 1u);
    EXPECT_TRUE(effects.Contains(1));
    EXPECT_EQ(Pixel(16, 16)[0], 0);

    effects.EndView();

    EXPECT_GT(Pixel(16, 16)[0], 240);
    EXPECT_EQ(Pixel(16, 16)[2], 0);

    effects.Clear();
    const uint64_t replacement = effects.BeginView();
    EXPECT_NE(replacement, outer);
    EXPECT_NE(replacement, own);
    effects.CancelView(own);
    effects.CancelView(outer);
    EXPECT_EQ(effects.get_view_count(), 1u);
    effects.EndView();
}

TEST_F(StudioEffectsGlTest, LocalPassesBoundWideWorkToTheInfluenceRect)
{
    Wall();
    SetScalar("uOutlineWidth", 3);
    SetScalar("uGlowRadius", 1);
    SetScalar("uGlowStrength", 1);
    Capture(1, {8, 8, 16, 16});
    EffectWorkProbe probe;
    render::StudioEffects_EndView();

    EXPECT_LT(probe.largest_draw_area, 32 * 32);
    EXPECT_LT(probe.largest_seed_area, 32 * 32);
    EXPECT_GT(probe.draws, 2u);
    EXPECT_GT(Pixel(16, 16)[0], 240);
    EXPECT_GT(Pixel(5, 16)[1], 240);
    EXPECT_EQ(Pixel(0, 16)[1], 0);
}

TEST_F(StudioEffectsGlTest, LocalPassesNarrowOutlineUsesTwoDrawsAndKeepsTheFringe)
{
    Wall();
    SetScalar("uOutlineWidth", 2);
    Capture(1, {8, 8, 16, 16});
    EffectWorkProbe probe;
    render::StudioEffects_EndView();

    EXPECT_EQ(probe.draws, 2u);
    EXPECT_LT(probe.largest_draw_area, 32 * 32);
    EXPECT_GT(Pixel(7, 16)[1], 240);
    EXPECT_GT(Pixel(6, 16)[1], 240);
    EXPECT_GT(Pixel(6, 7)[1], 0);
    EXPECT_EQ(Pixel(5, 16)[1], 0);
    EXPECT_GT(Pixel(16, 16)[0], 240);
}

TEST_F(StudioEffectsGlTest, LocalPassesFillOnlySkipsSeedsAndUnavailableDistancePrograms)
{
    render::StudioEffects effects(source_.device, source_.shaders, coverage_);
    source_.failed_fragment = "studio_effect_distance.frag";
    SetScalar("uOutlineWidth", 0);
    Wall();
    effects.BeginView();
    PrepareCoveragePacket(coverage_, packet_);
    effects.Capture(1, packet_, {8, 8, 16, 16});
    EffectWorkProbe probe;
    effects.EndView();

    EXPECT_EQ(probe.draws, 1u);
    EXPECT_EQ(probe.seed_allocations, 0u);
    EXPECT_EQ(source_.builds, 2u);
    EXPECT_GT(Pixel(16, 16)[0], 240);
    EXPECT_EQ(Pixel(7, 16)[0], 0);

    effects.Clear();
}

TEST_F(StudioEffectsGlTest, LocalPassesCaptureOnlyClearsTheTwoRetainedDepthTargets)
{
    Wall();
    EffectWorkProbe probe;
    Capture(1, {8, 8, 16, 16});

    EXPECT_EQ(probe.depth_clears, 0u);

    render::StudioEffects_EndView();

    EXPECT_EQ(probe.depth_clears, 2u);
    EXPECT_GT(Pixel(16, 16)[0], 240);
}

TEST_F(StudioEffectsGlTest, LocalPassesLateStylesAndMaskGrowthRetainTheWidestEarlierOutline)
{
    Wall();
    SetScalar("uOutlineWidth", 3);
    packet_.bones = CapturePose({{3, -.3f}}, packet_.bones);
    Capture(1, {0, 5, 22, 22});
    SetScalar("uOutlineWidth", 0);
    packet_.bones = CapturePose({{3, .3f}}, packet_.bones);
    Capture(1, {9, 5, 23, 22});
    EffectWorkProbe probe;
    render::StudioEffects_EndView();

    EXPECT_GT(probe.draws, 2u);
    EXPECT_GT(Pixel(1, 16)[1], 240);
    EXPECT_GT(Pixel(26, 16)[0], 240);
}

TEST_F(StudioEffectsGlTest, LocalPassesNarrowSearchBreaksEqualDistancesBySeedRowThenColumn)
{
    Wall();
    SetScalar("uOccludedAmount", 0);
    SetScalar("uOutlineWidth", 2);
    SetColor("uOutlineColor", {1, 0, 0, 1});
    packet_.bones = CapturePose({{0, .25f}, {5, .25f}, {3, -.25f}}, packet_.bones);
    Capture(1, {8, 12, 8, 8});
    SetColor("uOutlineColor", {0, 0, 1, 1});
    packet_.bones = CapturePose({{3, .1875f}}, packet_.bones);
    Capture(1, {15, 12, 8, 8});
    render::StudioEffects_EndView();

    EXPECT_GT(Pixel(15, 16)[0], 240);
    EXPECT_EQ(Pixel(15, 16)[2], 0);
}

TEST_F(StudioEffectsGlTest, LocalPassesScratchRetainsCapacityWithoutReadingStaleSeeds)
{
    Wall();
    SetScalar("uOutlineWidth", 3);
    Capture(1, {0, 0, 32, 32});
    render::StudioEffects_EndView();
    glClear(GL_COLOR_BUFFER_BIT);
    render::StudioEffects_BeginView();
    packet_.bones = CapturePose({{0, .25f}, {5, .25f}}, packet_.bones);
    SetColor("uOutlineColor", {0, 0, 1, 1});
    Capture(2, {14, 14, 4, 4});
    EffectWorkProbe probe;
    render::StudioEffects_EndView();

    EXPECT_EQ(probe.seed_allocations, 0u);
    EXPECT_LT(probe.largest_draw_area, 32 * 32);
    EXPECT_EQ(Pixel(7, 16)[1], 0);
    EXPECT_EQ(Pixel(7, 16)[2], 0);
    EXPECT_GT(Pixel(11, 16)[2], 240);
    EXPECT_GT(Pixel(16, 16)[0], 240);
}

TEST_F(StudioEffectsGlTest, LocalPassesFillNarrowAndWideBindingsPrepareWithinOneGeneration)
{
    render::StudioEffects effects(source_.device, source_.shaders, coverage_);
    Wall();

    for (int width : {0, 2, 3})
    {
        SetScalar("uOutlineWidth", static_cast<float>(width));
        effects.BeginView();
        PrepareCoveragePacket(coverage_, packet_);
        effects.Capture(1, packet_, {8, 8, 16, 16});
        effects.EndView();

        EXPECT_EQ(source_.builds, width == 0 ? 2u : (width == 2 ? 3u : 4u));
        EXPECT_GT(Pixel(16, 16)[0], 240);

        if (width > 0)
        {
            EXPECT_GT(Pixel(7, 16)[1], 240);
        }
    }

    effects.Clear();
}


TEST_F(StudioEffectsGlTest, CapacityPoolWorkingSetBoundsGrowingOversizedTargetsAndReusesTheLatest)
{
    EffectTargetFixture fixture(source_.device);
    EffectAllocationProbe allocations;
    const std::array<render::effects::Rect_t, 4> rectangles = {
        render::effects::Rect_t{0, 0, 736, 736},
        render::effects::Rect_t{0, 0, 752, 752},
        render::effects::Rect_t{0, 0, 768, 768},
        render::effects::Rect_t{0, 0, 784, 752}
    };
    const auto live_images = [&allocations]() {
        std::vector<GLuint> names = allocations.textures;
        std::sort(names.begin(), names.end());
        names.erase(std::unique(names.begin(), names.end()), names.end());

        return std::count_if(
            names.begin(),
            names.end(),
            [](GLuint image) {
                return glIsTexture(image) != 0;
            }
        );
    };

    const auto live_pixels = [&allocations]() {
        size_t pixels = 0;

        for (size_t index = 0; index < allocations.storage.size(); ++index)
        {
            const EffectAllocationProbe::Storage& image = allocations.storage[index];
            const bool replaced = std::any_of(
                allocations.storage.begin() + index + 1,
                allocations.storage.end(),
                [&image](const EffectAllocationProbe::Storage& next) {
                    return image.image == next.image;
                }
            );

            if (!replaced && glIsTexture(image.image))
            {
                pixels += static_cast<size_t>(image.wide) * image.tall;
            }
        }

        return pixels;
    };

    GLuint latest = 0;

    for (size_t index = 0; index < rectangles.size(); ++index)
    {
        bool reused = false;
        render::StudioEffectTargets::Mask* mask =
            fixture.targets.AcquireMask(
                fixture.masks,
                index + 1,
                rectangles[index],
                false,
                0,
                reused
            );

        ASSERT_NE(mask, nullptr);
        EXPECT_FALSE(reused);

        latest = mask->get_fill();
        fixture.targets.RecycleWorkingMasks(fixture.masks);

        EXPECT_EQ(live_images(), 5);
        EXPECT_EQ(live_pixels(), static_cast<size_t>(rectangles[index][2]) * rectangles[index][3] * 5);
    }

    const size_t storage = allocations.storage.size();
    bool reused = false;
    render::StudioEffectTargets::Mask* mask = fixture.targets.AcquireMask(
        fixture.masks,
        9,
        {0, 0, 128, 128},
        false,
        0,
        reused
    );

    ASSERT_NE(mask, nullptr);
    EXPECT_TRUE(reused);
    EXPECT_EQ(mask->get_fill(), latest);
    EXPECT_EQ(allocations.storage.size(), storage);

    fixture.targets.RecycleWorkingMasks(fixture.masks);

    EXPECT_EQ(live_images(), 5);

    fixture.targets.RecycleMasks(fixture.masks);

    EXPECT_EQ(live_images(), 0);
}

TEST_F(StudioEffectsGlTest, CapacityPoolReusesRoundedStorageAcrossSmallLogicalChanges)
{
    EffectTargetFixture owner(source_.device);
    EffectAllocationProbe probe;
    bool reused = false;
    render::StudioEffectTargets::Mask* mask = owner.targets.AcquireMask(
        owner.masks,
        1,
        {7, 9, 49, 65},
        false,
        0,
        reused
    );

    ASSERT_NE(mask, nullptr);

    const EffectMaskNames first(*mask);

    EXPECT_EQ(mask->get_rect(), (render::effects::Rect_t{7, 9, 49, 65}));
    EXPECT_EQ(probe.textures.size(), 5u);
    EXPECT_EQ(probe.framebuffers.size(), 2u);
    ASSERT_EQ(probe.storage.size(), 5u);

    for (const EffectAllocationProbe::Storage& storage : probe.storage)
    {
        EXPECT_EQ(storage.wide, 64);
        EXPECT_EQ(storage.tall, 80);
    }

    owner.targets.RecycleMasks(owner.masks);
    mask = owner.targets.AcquireMask(
        owner.masks,
        2,
        {3, 2, 53, 70},
        true,
        0,
        reused
    );

    ASSERT_NE(mask, nullptr);
    EXPECT_TRUE(reused);

    const EffectMaskNames second(*mask);

    EXPECT_EQ(second.images, first.images);
    EXPECT_EQ(second.framebuffers, first.framebuffers);
    EXPECT_EQ(mask->get_rect(), (render::effects::Rect_t{3, 2, 53, 70}));
    EXPECT_TRUE(mask->is_reversed());
    EXPECT_EQ(probe.storage.size(), 5u);
    EXPECT_TRUE(probe.deleted_textures.empty());
    EXPECT_TRUE(probe.deleted_framebuffers.empty());
    EXPECT_EQ(glGetError(), GLenum(GL_NO_ERROR));
}

TEST_F(StudioEffectsGlTest, CapacityPoolSelectsSmallestFittingAreaAndMatchingSamples)
{
    EffectTargetFixture owner(source_.device);
    EffectAllocationProbe probe;
    bool reused = false;
    render::StudioEffectTargets::Mask* large = owner.targets.AcquireMask(
        owner.masks,
        1,
        {0, 0, 80, 80},
        false,
        0,
        reused
    );
    render::StudioEffectTargets::Mask* small = owner.targets.AcquireMask(
        owner.masks,
        2,
        {0, 0, 64, 80},
        false,
        0,
        reused
    );
    render::StudioEffectTargets::Mask* ms = owner.targets.AcquireMask(
        owner.masks,
        3,
        {0, 0, 64, 80},
        false,
        4,
        reused
    );

    ASSERT_NE(large, nullptr);
    ASSERT_NE(small, nullptr);
    ASSERT_NE(ms, nullptr);

    const EffectMaskNames small_names(*small);
    const EffectMaskNames ms_names(*ms);

    EXPECT_EQ(probe.storage.back().samples, 4);

    glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, ms->get_object_depth());
    GLint samples = 0;
    glGetTexLevelParameteriv(GL_TEXTURE_2D_MULTISAMPLE, 0, GL_TEXTURE_SAMPLES, &samples);

    EXPECT_EQ(samples, 4);

    owner.targets.RecycleMasks(owner.masks);
    render::StudioEffectTargets::Mask* mask = owner.targets.AcquireMask(
        owner.masks,
        4,
        {0, 0, 60, 70},
        false,
        4,
        reused
    );

    ASSERT_NE(mask, nullptr);
    EXPECT_TRUE(reused);
    EXPECT_EQ(EffectMaskNames(*mask).images, ms_names.images);

    mask = owner.targets.AcquireMask(
        owner.masks,
        5,
        {0, 0, 60, 70},
        false,
        0,
        reused
    );

    ASSERT_NE(mask, nullptr);
    EXPECT_TRUE(reused);
    EXPECT_EQ(EffectMaskNames(*mask).images, small_names.images);
    EXPECT_EQ(probe.storage.size(), 15u);
    EXPECT_EQ(glGetError(), GLenum(GL_NO_ERROR));
}

TEST_F(StudioEffectsGlTest, CapacityPoolAdmitsCurrentWorkingSetBeforeIncompatibleOldPool)
{
    EffectTargetFixture owner(source_.device);
    EffectAllocationProbe probe;
    bool reused = false;
    render::StudioEffectTargets::Mask* mask = owner.targets.AcquireMask(
        owner.masks,
        1,
        {0, 0, 1024, 512},
        false,
        0,
        reused
    );

    ASSERT_NE(mask, nullptr);

    const EffectMaskNames old(*mask);
    owner.targets.RecycleMasks(owner.masks);
    mask = owner.targets.AcquireMask(
        owner.masks,
        2,
        {0, 0, 512, 1024},
        false,
        0,
        reused
    );

    ASSERT_NE(mask, nullptr);

    const EffectMaskNames current(*mask);

    EXPECT_FALSE(reused);

    owner.targets.RecycleMasks(owner.masks);
    old.ExpectLive(false);
    current.ExpectLive(true);

    EXPECT_EQ(probe.deleted_textures.size(), 5u);
    EXPECT_EQ(probe.deleted_framebuffers.size(), 2u);

    mask = owner.targets.AcquireMask(
        owner.masks,
        3,
        {0, 0, 512, 1024},
        false,
        0,
        reused
    );

    ASSERT_NE(mask, nullptr);
    EXPECT_TRUE(reused);
    EXPECT_EQ(EffectMaskNames(*mask).images, current.images);
    EXPECT_EQ(probe.storage.size(), 10u);
    EXPECT_EQ(glGetError(), GLenum(GL_NO_ERROR));
}

TEST_F(StudioEffectsGlTest, CapacityPoolBudgetCountsAllocatedPixelsAndKeepsStableCurrentOrder)
{
    EffectTargetFixture owner(source_.device);
    EffectAllocationProbe probe;
    bool reused = false;
    std::vector<EffectMaskNames> names;

    for (uintptr_t entity = 1; entity <= 4; ++entity)
    {
        render::StudioEffectTargets::Mask* mask = owner.targets.AcquireMask(
            owner.masks,
            entity,
            {0, 0, 497, 257},
            false,
            0,
            reused
        );

        ASSERT_NE(mask, nullptr);

        names.emplace_back(*mask);
    }

    ASSERT_EQ(probe.storage.size(), 20u);

    for (const EffectAllocationProbe::Storage& storage : probe.storage)
    {
        EXPECT_EQ(storage.wide, 512);
        EXPECT_EQ(storage.tall, 272);
    }

    owner.targets.RecycleMasks(owner.masks);
    names[0].ExpectLive(true);
    names[1].ExpectLive(true);
    names[2].ExpectLive(true);
    names[3].ExpectLive(false);

    EXPECT_EQ(probe.deleted_textures.size(), 5u);
    EXPECT_EQ(probe.deleted_framebuffers.size(), 2u);

    for (uintptr_t entity = 5; entity <= 8; ++entity)
    {
        render::StudioEffectTargets::Mask* mask = owner.targets.AcquireMask(
            owner.masks,
            entity,
            {0, 0, 497, 257},
            false,
            0,
            reused
        );

        ASSERT_NE(mask, nullptr);
        EXPECT_EQ(reused, entity < 8);

        if (entity < 8)
        {
            EXPECT_EQ(EffectMaskNames(*mask).images, names[entity - 5].images);
        }
    }

    EXPECT_EQ(probe.storage.size(), 25u);
    EXPECT_EQ(glGetError(), GLenum(GL_NO_ERROR));
}

TEST_F(StudioEffectsGlTest, CapacityPoolOversizedCandidateLeavesUsableOldStorage)
{
    EffectTargetFixture owner(source_.device);
    bool reused = false;
    render::StudioEffectTargets::Mask* mask = owner.targets.AcquireMask(
        owner.masks,
        1,
        {0, 0, 64, 64},
        false,
        0,
        reused
    );

    ASSERT_NE(mask, nullptr);

    const EffectMaskNames old(*mask);
    owner.targets.RecycleMasks(owner.masks);
    mask = owner.targets.AcquireMask(
        owner.masks,
        2,
        {0, 0, 1024, 528},
        false,
        0,
        reused
    );

    ASSERT_NE(mask, nullptr);

    const EffectMaskNames oversized(*mask);
    owner.targets.RecycleMasks(owner.masks);
    oversized.ExpectLive(false);
    old.ExpectLive(true);
    mask = owner.targets.AcquireMask(
        owner.masks,
        3,
        {0, 0, 64, 64},
        false,
        0,
        reused
    );

    ASSERT_NE(mask, nullptr);
    EXPECT_TRUE(reused);
    EXPECT_EQ(EffectMaskNames(*mask).images, old.images);
    EXPECT_EQ(glGetError(), GLenum(GL_NO_ERROR));
}

TEST_F(StudioEffectsGlTest, CapacityPoolReuseClearsEveryColorAndDepthTexelAndStyle)
{
    EffectTargetFixture owner(source_.device);
    bool reused = false;
    render::StudioEffectTargets::Mask* mask = owner.targets.AcquireMask(
        owner.masks,
        1,
        {0, 0, 64, 80},
        false,
        0,
        reused
    );

    ASSERT_NE(mask, nullptr);

    const EffectMaskNames old(*mask);
    mask->IncludeStyle(8, 4, 1);
    const float color[] = {1, .5f, .25f, 1};

    for (int attachment = 0; attachment < 3; ++attachment)
    {
        glClearBufferfv(GL_COLOR, attachment, color);
    }

    glClearDepth(.3);
    glClear(GL_DEPTH_BUFFER_BIT);
    glBindFramebuffer(GL_FRAMEBUFFER, mask->get_depth_framebuffer());
    glClear(GL_DEPTH_BUFFER_BIT);
    owner.targets.RecycleMasks(owner.masks);
    mask = owner.targets.AcquireMask(
        owner.masks,
        2,
        {7, 5, 9, 11},
        true,
        0,
        reused
    );

    ASSERT_NE(mask, nullptr);
    EXPECT_TRUE(reused);
    EXPECT_EQ(EffectMaskNames(*mask).images, old.images);
    EXPECT_EQ(mask->get_outline_width(), 0);
    EXPECT_EQ(mask->get_influence(), 0);
    EXPECT_FALSE(mask->has_glow());

    for (const std::array<int, 2>& point : {std::array<int, 2>{4, 5}, std::array<int, 2>{63, 79}})
    {
        for (GLenum attachment : {GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1, GL_COLOR_ATTACHMENT2})
        {
            EXPECT_EQ(MaskPixel(mask->get_framebuffer(), attachment, point[0], point[1]), (std::array<float, 4>{}));
        }

        EXPECT_FLOAT_EQ(MaskDepth(mask->get_framebuffer(), point[0], point[1]), 0);
        EXPECT_FLOAT_EQ(MaskDepth(mask->get_depth_framebuffer(), point[0], point[1]), 0);
    }

    EXPECT_EQ(glGetError(), GLenum(GL_NO_ERROR));
}

TEST_F(StudioEffectsGlTest, CapacityPoolCaptureClipsColorAndDepthToLogicalRect)
{
    EffectTargetFixture owner(source_.device);
    render::StudioEffectPasses passes(owner.targets, source_.shaders, coverage_);
    PrepareCoveragePacket(coverage_, packet_);
    const ncl_gl::Program* program = passes.PrepareCapture();

    ASSERT_NE(program, nullptr);

    bool reused = false;
    render::StudioEffectTargets::Mask* mask = owner.targets.AcquireMask(
        owner.masks,
        1,
        {8, 8, 9, 9},
        false,
        0,
        reused
    );

    ASSERT_NE(mask, nullptr);
    ASSERT_TRUE(owner.targets.EnsureCaptureDepth(packet_.viewport, 0));

    passes.CaptureMask(*mask, packet_, *program);
    passes.CaptureDepth(*mask, packet_, *program);

    EXPECT_GT(MaskPixel(mask->get_framebuffer(), GL_COLOR_ATTACHMENT0, 4, 4)[0], .9f);
    EXPECT_NEAR(MaskDepth(mask->get_framebuffer(), 4, 4), .5f, .00001f);
    EXPECT_EQ(MaskPixel(mask->get_framebuffer(), GL_COLOR_ATTACHMENT0, 12, 12), (std::array<float, 4>{}));
    EXPECT_FLOAT_EQ(MaskDepth(mask->get_framebuffer(), 12, 12), 1);
    EXPECT_FLOAT_EQ(MaskDepth(mask->get_depth_framebuffer(), 12, 12), 1);
    EXPECT_EQ(glGetError(), GLenum(GL_NO_ERROR));
}

TEST_F(StudioEffectsGlTest, CapacityPoolLatePartRestoresDepthAtNonzeroWindowOrigin)
{
    EffectTargetFixture owner(source_.device);
    render::StudioEffectPasses passes(owner.targets, source_.shaders, coverage_);
    PrepareCoveragePacket(coverage_, packet_);
    const ncl_gl::Program* program = passes.PrepareCapture();

    ASSERT_NE(program, nullptr);

    bool reused = false;
    render::StudioEffectTargets::Mask* mask = owner.targets.AcquireMask(
        owner.masks,
        1,
        {8, 8, 9, 9},
        false,
        0,
        reused
    );

    ASSERT_NE(mask, nullptr);
    ASSERT_TRUE(owner.targets.EnsureCaptureDepth(packet_.viewport, 0));

    passes.CaptureMask(*mask, packet_, *program);
    passes.CaptureDepth(*mask, packet_, *program);
    glBindFramebuffer(GL_FRAMEBUFFER, owner.targets.get_capture_depth_framebuffer());
    glDisable(GL_SCISSOR_TEST);
    glClearDepth(1);
    glClear(GL_DEPTH_BUFFER_BIT);
    packet_.bones = CapturePose({{11, .5f}}, packet_.bones);
    passes.CaptureMask(*mask, packet_, *program);
    passes.CaptureDepth(*mask, packet_, *program);

    EXPECT_NEAR(MaskDepth(mask->get_depth_framebuffer(), 8, 8), .5f, .00001f);
    EXPECT_EQ(glGetError(), GLenum(GL_NO_ERROR));
}

TEST_F(StudioEffectsGlTest, CapacityPoolGrowthMovesLogicalPixelsAndBothDepthSurfacesWithStyle)
{
    EffectTargetFixture owner(source_.device);
    bool reused = false;
    render::StudioEffectTargets::Mask* mask = owner.targets.AcquireMask(
        owner.masks,
        1,
        {8, 8, 49, 49},
        false,
        0,
        reused
    );

    ASSERT_NE(mask, nullptr);

    mask->IncludeStyle(5, 3, 1);
    glBindFramebuffer(GL_FRAMEBUFFER, mask->get_framebuffer());
    const float color[] = {1, .5f, .25f, 1};
    glEnable(GL_SCISSOR_TEST);
    glScissor(3, 4, 1, 1);

    for (int attachment = 0; attachment < 3; ++attachment)
    {
        glClearBufferfv(GL_COLOR, attachment, color);
    }

    glClearDepth(.3);
    glClear(GL_DEPTH_BUFFER_BIT);
    glBindFramebuffer(GL_FRAMEBUFFER, mask->get_depth_framebuffer());
    glClearDepth(.2);
    glClear(GL_DEPTH_BUFFER_BIT);
    mask = owner.targets.AcquireMask(
        owner.masks,
        1,
        {0, 0, 32, 32},
        false,
        0,
        reused
    );

    ASSERT_NE(mask, nullptr);
    EXPECT_EQ(mask->get_rect(), (render::effects::Rect_t{0, 0, 57, 57}));
    EXPECT_EQ(mask->get_outline_width(), 5);
    EXPECT_EQ(mask->get_influence(), 8);
    EXPECT_TRUE(mask->has_glow());

    for (GLenum attachment : {GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1, GL_COLOR_ATTACHMENT2})
    {
        EXPECT_GT(MaskPixel(mask->get_framebuffer(), attachment, 11, 12)[0], .9f);
        EXPECT_EQ(MaskPixel(mask->get_framebuffer(), attachment, 3, 4), (std::array<float, 4>{}));
    }

    EXPECT_NEAR(MaskDepth(mask->get_framebuffer(), 11, 12), .3f, .00001f);
    EXPECT_NEAR(MaskDepth(mask->get_depth_framebuffer(), 11, 12), .2f, .00001f);
    EXPECT_EQ(glGetError(), GLenum(GL_NO_ERROR));
}

TEST_F(StudioEffectsGlTest, CapacityPoolNestedMasksCannotBorrowActiveOuterStorage)
{
    EffectTargetFixture owner(source_.device);
    bool reused = false;
    render::StudioEffectTargets::Mask* outer = owner.targets.AcquireMask(
        owner.masks,
        1,
        {0, 0, 49, 65},
        false,
        0,
        reused
    );

    ASSERT_NE(outer, nullptr);

    const EffectMaskNames outer_names(*outer);
    const float color[] = {1, 0, 0, 1};
    glClearBufferfv(GL_COLOR, 0, color);
    render::StudioEffectTargets::Mask* inner = owner.targets.AcquireMask(
        owner.nested,
        2,
        {0, 0, 49, 65},
        false,
        0,
        reused
    );

    ASSERT_NE(inner, nullptr);
    EXPECT_FALSE(reused);
    EXPECT_NE(inner->get_fill(), outer->get_fill());

    owner.targets.RecycleMasks(owner.nested);
    inner = owner.targets.AcquireMask(
        owner.nested,
        3,
        {0, 0, 45, 60},
        false,
        0,
        reused
    );

    ASSERT_NE(inner, nullptr);
    EXPECT_TRUE(reused);
    EXPECT_NE(inner->get_fill(), outer->get_fill());

    outer_names.ExpectLive(true);

    EXPECT_GT(MaskPixel(outer->get_framebuffer(), GL_COLOR_ATTACHMENT0, 40, 50)[0], .9f);
    EXPECT_EQ(glGetError(), GLenum(GL_NO_ERROR));
}
