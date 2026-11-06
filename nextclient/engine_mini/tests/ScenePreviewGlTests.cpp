#include <gtest/gtest.h>

#include <array>
#include <string>

#include <glad/glad.h>

#include "RadarGlContext.h"
#include "engine.h"
#include "PreviewTestOwners.h"
#include "graphics/gl_local.h"
#include "graphics/preview/ScenePreview.h"
#include "graphics/render/ScopedGlState.h"
#include "graphics/render/render_device.h"
#include "graphics/preview/preview_sun_gizmo.h"

namespace
{
    int ScreenInfo(goldsrc::SCREENINFO* info)
    {
        info->iWidth = 32;
        info->iHeight = 32;
        return 1;
    }

    class ScenePreviewGlTest : public testing::Test
    {
    private:
        RadarGlContext context_;
        goldsrc::cl_enginefunc_t saved_engine_{};
        goldsrc::cvar_t quiet_{};
        goldsrc::cvar_t* saved_developer_{};
        GLuint framebuffers_[2]{};
        GLuint textures_[2]{};
        GLuint depth_buffers_[2]{};

    protected:
        void SetUp() override
        {
            std::string error;
            ASSERT_TRUE(context_.Initialize(error)) << error;
            ASSERT_TRUE(GLAD_GL_VERSION_3_3);
            saved_developer_ = developer;
            developer = &quiet_;
            render::Device_OnContextCreated();
            saved_engine_ = gEngfuncs;
            gEngfuncs.pfnGetScreenInfo = ScreenInfo;
            preview_test::BindEngine(render::GetRendererState());
            qglGetIntegerv = glad_glGetIntegerv;
            qglMatrixMode = glad_glMatrixMode;
            qglPushMatrix = glad_glPushMatrix;
            qglPopMatrix = glad_glPopMatrix;
            qglIsEnabled = glad_glIsEnabled;
            qglGetBooleanv = glad_glGetBooleanv;
            qglScissor = glad_glScissor;
            qglViewport = glad_glViewport;
            qglEnable = glad_glEnable;
            qglDisable = glad_glDisable;
            qglDepthMask = glad_glDepthMask;
            qglDepthFunc = glad_glDepthFunc;
            qglClearDepth = glad_glClearDepth;
            qglColorMask = glad_glColorMask;
            qglClearColor = glad_glClearColor;
            qglClear = glad_glClear;
            qglColor4f = glad_glColor4f;

            glGenFramebuffers(2, framebuffers_);
            glGenTextures(2, textures_);
            glGenRenderbuffers(2, depth_buffers_);

            for (int index = 0; index < 2; index++)
            {
                glBindTexture(GL_TEXTURE_2D, textures_[index]);
                glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 32, 32, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
                glBindFramebuffer(GL_FRAMEBUFFER, framebuffers_[index]);
                glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, textures_[index], 0);
                glBindRenderbuffer(GL_RENDERBUFFER, depth_buffers_[index]);
                glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, 32, 32);
                glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depth_buffers_[index]);
                ASSERT_EQ(glCheckFramebufferStatus(GL_FRAMEBUFFER), GLenum(GL_FRAMEBUFFER_COMPLETE));
            }

            glBindFramebuffer(GL_READ_FRAMEBUFFER, framebuffers_[0]);
            glBindFramebuffer(GL_DRAW_FRAMEBUFFER, framebuffers_[1]);
            glViewport(0, 0, 32, 32);
        }

        void TearDown() override
        {
            render::Device_OnContextDestroyed();
            developer = saved_developer_;
            gEngfuncs = saved_engine_;
            glDeleteFramebuffers(2, framebuffers_);
            glDeleteTextures(2, textures_);
            glDeleteRenderbuffers(2, depth_buffers_);
        }
    };
} // namespace

TEST_F(ScenePreviewGlTest, RestoresCallerStateWithIndependentReadAndDrawFramebuffers)
{
    glEnable(GL_SCISSOR_TEST);
    glScissor(2, 3, 4, 5);
    glDepthMask(GL_FALSE);
    glEnable(GL_BLEND);
    glCullFace(GL_BACK);
    glFrontFace(GL_CW);
    glClearColor(0.2f, 0.3f, 0.4f, 0.5f);
    glColor4f(0.1f, 0.2f, 0.3f, 0.4f);
    glMatrixMode(GL_TEXTURE);
    glActiveTexture(GL_TEXTURE3);

    GLint read_framebuffer = 0;
    GLint draw_framebuffer = 0;
    glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &read_framebuffer);
    glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &draw_framebuffer);

    render::preview::ScenePreview preview = preview_test::MakeScenePreview(render::GetRendererState());
    PreviewDrawParams params{};
    params.clear = true;
    params.clear_color[0] = 1.0f;
    PreviewRect rect{4, 4, 16, 16};
    EXPECT_EQ(preview.Draw(params, rect, rect), PreviewDrawStatus::Ready);

    GLint value = 0;
    glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &value);
    EXPECT_EQ(value, read_framebuffer);
    glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &value);
    EXPECT_EQ(value, draw_framebuffer);
    glGetIntegerv(GL_MATRIX_MODE, &value);
    EXPECT_EQ(value, GL_TEXTURE);
    glGetIntegerv(GL_ACTIVE_TEXTURE, &value);
    EXPECT_EQ(value, GL_TEXTURE3);
    std::array<GLint, 4> box{};
    glGetIntegerv(GL_SCISSOR_BOX, box.data());
    EXPECT_EQ(box, (std::array<GLint, 4>{2, 3, 4, 5}));
    std::array<GLfloat, 4> color{};
    glGetFloatv(GL_COLOR_CLEAR_VALUE, color.data());
    EXPECT_EQ(color, (std::array<GLfloat, 4>{0.2f, 0.3f, 0.4f, 0.5f}));
    glGetFloatv(GL_CURRENT_COLOR, color.data());
    EXPECT_EQ(color, (std::array<GLfloat, 4>{0.1f, 0.2f, 0.3f, 0.4f}));
    GLboolean depth_write = GL_TRUE;
    glGetBooleanv(GL_DEPTH_WRITEMASK, &depth_write);
    EXPECT_EQ(depth_write, GL_FALSE);
    EXPECT_TRUE(glIsEnabled(GL_BLEND));
    EXPECT_EQ(glGetError(), GLenum(GL_NO_ERROR));
}

TEST_F(ScenePreviewGlTest, InvalidViewportLeavesTheCallerStateUntouched)
{
    glMatrixMode(GL_TEXTURE);
    render::preview::ScenePreview preview = preview_test::MakeScenePreview(render::GetRendererState());
    PreviewDrawParams params{};
    PreviewRect rect{0, 0, 0, 32};

    EXPECT_EQ(preview.Draw(params, rect, rect), PreviewDrawStatus::InvalidViewport);

    GLint mode = 0;
    glGetIntegerv(GL_MATRIX_MODE, &mode);
    EXPECT_EQ(mode, GL_TEXTURE);
    EXPECT_EQ(glGetError(), GLenum(GL_NO_ERROR));
}

TEST_F(ScenePreviewGlTest, SunGizmoUsesLightColorAndDirectionAndRestoresViewport)
{
    GLint output = 0;
    glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &output);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, static_cast<GLuint>(output));
    PreviewCamera camera{};
    PreviewLight light{};
    light.direction[0] = 1;
    light.color[0] = 1;
    light.color[1] = 0.2f;
    light.color[2] = 0.1f;
    const auto draw = [&camera](const PreviewLight& input) {
        glClearColor(0, 0, 0, 1);
        glClear(GL_COLOR_BUFFER_BIT);
        glViewport(2, 3, 24, 25);
        glMatrixMode(GL_TEXTURE);
        PreviewSunGizmo_Draw(camera, input, {0, 0, 32, 32});
        std::array<unsigned char, 32 * 32 * 4> pixels{};
        glReadPixels(0, 0, 32, 32, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
        return pixels;
    };
    const auto first = draw(light);
    light.direction[0] = 0;
    light.direction[2] = -1;
    const auto second = draw(light);
    EXPECT_NE(first, second);
    light.color[0] = 0.1f;
    light.color[1] = 0.2f;
    light.color[2] = 1;
    const auto blue = draw(light);
    EXPECT_NE(second, blue);
    int red_pixels = 0;
    int blue_pixels = 0;

    for (size_t index = 0; index < second.size(); index += 4)
    {
        red_pixels += second[index] > second[index + 1] && second[index + 1] >= second[index + 2];
        blue_pixels += blue[index + 2] > blue[index + 1] && blue[index + 1] > blue[index];
    }

    EXPECT_GT(red_pixels, 10);
    EXPECT_GT(blue_pixels, 10);
    std::array<GLint, 4> viewport{};
    glGetIntegerv(GL_VIEWPORT, viewport.data());
    EXPECT_EQ(viewport, (std::array<GLint, 4>{2, 3, 24, 25}));
    GLint mode = 0;
    glGetIntegerv(GL_MATRIX_MODE, &mode);
    EXPECT_EQ(mode, GL_TEXTURE);
    EXPECT_EQ(glGetError(), GLenum(GL_NO_ERROR));
}

TEST_F(ScenePreviewGlTest, RestoresDefaultFramebufferBeforeItsReadAndDrawBuffers)
{
    GLint offscreen = 0;
    glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &offscreen);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glDrawBuffer(GL_FRONT);
    glReadBuffer(GL_FRONT);

    {
        render::ScopedGlState saved;
        glBindFramebuffer(GL_FRAMEBUFFER, static_cast<GLuint>(offscreen));
        glDrawBuffer(GL_COLOR_ATTACHMENT0);
        glReadBuffer(GL_COLOR_ATTACHMENT0);
    }

    EXPECT_EQ(glGetError(), GLenum(GL_NO_ERROR));
    GLint binding = -1;
    glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &binding);
    EXPECT_EQ(binding, 0);
    glGetIntegerv(GL_DRAW_BUFFER, &binding);
    EXPECT_EQ(binding, GL_FRONT);
}

TEST_F(ScenePreviewGlTest, RestoresIndexedBoneRangeAndIndependentGenericBinding)
{
    GLuint buffers[2]{};
    glGenBuffers(2, buffers);
    GLint alignment = 0;
    glGetIntegerv(GL_UNIFORM_BUFFER_OFFSET_ALIGNMENT, &alignment);
    glBindBuffer(GL_UNIFORM_BUFFER, buffers[0]);
    glBufferData(GL_UNIFORM_BUFFER, alignment + 64, nullptr, GL_STATIC_DRAW);
    glBindBufferRange(GL_UNIFORM_BUFFER, 0, buffers[0], alignment, 64);
    glBindBuffer(GL_UNIFORM_BUFFER, buffers[1]);
    glBufferData(GL_UNIFORM_BUFFER, 128, nullptr, GL_STATIC_DRAW);

    {
        render::ScopedGlState saved;
        glBindBufferBase(GL_UNIFORM_BUFFER, 0, buffers[1]);
        glBindBuffer(GL_UNIFORM_BUFFER, buffers[0]);
    }

    GLint binding = 0;
    GLint64 offset = 0;
    GLint64 size = 0;
    glGetIntegeri_v(GL_UNIFORM_BUFFER_BINDING, 0, &binding);
    EXPECT_EQ(static_cast<GLuint>(binding), buffers[0]);
    glGetInteger64i_v(GL_UNIFORM_BUFFER_START, 0, &offset);
    EXPECT_EQ(offset, alignment);
    glGetInteger64i_v(GL_UNIFORM_BUFFER_SIZE, 0, &size);
    EXPECT_EQ(size, 64);
    glGetIntegerv(GL_UNIFORM_BUFFER_BINDING, &binding);
    EXPECT_EQ(static_cast<GLuint>(binding), buffers[1]);
    glDeleteBuffers(2, buffers);
    EXPECT_EQ(glGetError(), GLenum(GL_NO_ERROR));
}

TEST_F(ScenePreviewGlTest, ClearCannotEscapeTheViewportThroughALargerClip)
{
    glDisable(GL_SCISSOR_TEST);
    glClearColor(0.0f, 1.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    GLint framebuffer = 0;
    glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &framebuffer);
    render::preview::ScenePreview preview = preview_test::MakeScenePreview(render::GetRendererState());
    PreviewDrawParams params{};
    params.clear = true;
    params.clear_color[0] = 1.0f;

    EXPECT_EQ(preview.Draw(params, {4, 4, 16, 16}, {0, 0, 32, 32}), PreviewDrawStatus::Ready);

    glBindFramebuffer(GL_READ_FRAMEBUFFER, framebuffer);
    std::array<unsigned char, 4> pixel{};
    glReadPixels(1, 1, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixel.data());
    EXPECT_EQ(pixel, (std::array<unsigned char, 4>{0, 255, 0, 255}));
    glReadPixels(8, 16, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixel.data());
    EXPECT_EQ(pixel, (std::array<unsigned char, 4>{255, 0, 0, 255}));
}

TEST_F(ScenePreviewGlTest, RestoresEngineTextureBookkeepingAcrossBothUnits)
{
    int cached_texture = 77;
    int* saved_texture = p_currenttexture;
    GLenum saved_target = oldtarget;
    bool saved_multitexture = gl_mtexable;
    const auto saved_select = qglSelectTextureSGIS;
    p_currenttexture = &cached_texture;
    oldtarget = TEXTURE0_SGIS;
    gl_mtexable = true;
    qglSelectTextureSGIS = [](GLenum target) { glActiveTexture(GL_TEXTURE0 + target - TEXTURE0_SGIS); };
    GL_SelectTexture(TEXTURE1_SGIS);
    cached_texture = 88;
    GL_SelectTexture(TEXTURE0_SGIS);

    {
        render::ScopedGlState state;
        GL_SelectTexture(TEXTURE1_SGIS);
        cached_texture = 99;
        GL_SelectTexture(TEXTURE0_SGIS);
        cached_texture = 100;
    }

    EXPECT_EQ(cached_texture, 77);
    EXPECT_EQ(oldtarget, GLenum(TEXTURE0_SGIS));
    GL_SelectTexture(TEXTURE1_SGIS);
    EXPECT_EQ(cached_texture, 88);

    p_currenttexture = saved_texture;
    oldtarget = saved_target;
    gl_mtexable = saved_multitexture;
    qglSelectTextureSGIS = saved_select;
}

TEST_F(ScenePreviewGlTest, ClearUsesEveryColorChannelAndRestoresTheCallerMask)
{
    glClearColor(0.0f, 1.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
    GLint framebuffer = 0;
    glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &framebuffer);
    render::preview::ScenePreview preview = preview_test::MakeScenePreview(render::GetRendererState());
    PreviewDrawParams params{};
    params.clear = true;
    params.clear_color[0] = 1.0f;

    EXPECT_EQ(preview.Draw(params, {0, 0, 32, 32}, {0, 0, 32, 32}), PreviewDrawStatus::Ready);

    std::array<GLboolean, 4> mask{};
    glGetBooleanv(GL_COLOR_WRITEMASK, mask.data());
    EXPECT_EQ(mask, (std::array<GLboolean, 4>{GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE}));
    glBindFramebuffer(GL_READ_FRAMEBUFFER, framebuffer);
    std::array<unsigned char, 4> pixel{};
    glReadPixels(8, 16, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixel.data());
    EXPECT_EQ(pixel, (std::array<unsigned char, 4>{255, 0, 0, 255}));
}

TEST_F(ScenePreviewGlTest, DepthClearUsesTheFarPlaneAndRestoresTheCallerValue)
{
    glClearDepth(0.125);
    glDepthFunc(GL_GREATER);
    GLint framebuffer = 0;
    glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &framebuffer);
    render::preview::ScenePreview preview = preview_test::MakeScenePreview(render::GetRendererState());
    PreviewDrawParams params{};
    params.clear = true;

    EXPECT_EQ(preview.Draw(params, {0, 0, 32, 32}, {0, 0, 32, 32}), PreviewDrawStatus::Ready);

    double clear_depth = 0.0;
    glGetDoublev(GL_DEPTH_CLEAR_VALUE, &clear_depth);
    EXPECT_DOUBLE_EQ(clear_depth, 0.125);
    GLint depth_function = 0;
    glGetIntegerv(GL_DEPTH_FUNC, &depth_function);
    EXPECT_EQ(depth_function, GL_GREATER);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, framebuffer);
    float depth = 0.0f;
    glReadPixels(8, 16, 1, 1, GL_DEPTH_COMPONENT, GL_FLOAT, &depth);
    EXPECT_FLOAT_EQ(depth, 1.0f);
}

TEST_F(ScenePreviewGlTest, MaterialTestOccluderWritesColorAndDepthWithinTheView)
{
    goldsrc::cvar_t quiet{};
    goldsrc::cvar_t* saved_developer = developer;
    developer = &quiet;
    render::Device_OnContextCreated();
    render::preview::ScenePreview preview = preview_test::MakeScenePreview(render::GetRendererState());
    PreviewDrawParams params{};
    params.mode = PreviewRenderMode::MaterialAuthoring;
    params.occluder = true;
    params.clear = true;
    const PreviewRect view{4, 4, 16, 16};
    EXPECT_EQ(preview.Draw(params, view, view), PreviewDrawStatus::Ready);
    GLint output = 0;
    glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &output);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, output);
    float depth = 0;
    glReadPixels(8, 20, 1, 1, GL_DEPTH_COMPONENT, GL_FLOAT, &depth);
    EXPECT_NEAR(depth, .0005f, .00001f);
    std::array<unsigned char, 4> color{};
    glReadPixels(8, 20, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, color.data());
    EXPECT_NEAR(color[0], 61, 1);
    glReadPixels(17, 20, 1, 1, GL_DEPTH_COMPONENT, GL_FLOAT, &depth);
    EXPECT_FLOAT_EQ(depth, 1);
    EXPECT_EQ(glGetError(), GLenum(GL_NO_ERROR));
    preview.FreeAll();
    render::Device_OnContextDestroyed();
    developer = saved_developer;
}

TEST_F(ScenePreviewGlTest, AuthoringTargetCopiesBackgroundAndPresentsOnlyTheClippedColorAndDepth)
{
    goldsrc::cvar_t quiet{};
    goldsrc::cvar_t* saved_developer = developer;
    developer = &quiet;
    render::Device_OnContextCreated();
    GLint caller_draw = 0;
    GLint caller_read = 0;
    glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &caller_draw);
    glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &caller_read);
    glDisable(GL_SCISSOR_TEST);
    glClearColor(0, 1, 0, 1);
    glClearDepth(.75);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glViewport(2, 3, 28, 27);
    glScissor(1, 2, 3, 4);
    glEnable(GL_SCISSOR_TEST);
    render::preview::PreviewSceneTarget target(render::GetRendererState().resources.device);
    const PreviewRect view{4, 6, 16, 16};
    ASSERT_TRUE(target.Begin(view, true));
    GLint owned = 0;
    glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &owned);
    EXPECT_NE(owned, caller_draw);
    std::array<GLint, 4> viewport{};
    glGetIntegerv(GL_VIEWPORT, viewport.data());
    EXPECT_EQ(viewport, (std::array<GLint, 4>{0, 0, 16, 16}));
    std::array<unsigned char, 4> pixel{};
    glReadPixels(2, 2, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixel.data());
    EXPECT_EQ(pixel, (std::array<unsigned char, 4>{0, 255, 0, 255}));
    glClearColor(1, 0, 0, 1);
    glClearDepth(.25);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    target.Present(view, {8, 10, 4, 4});
    GLint binding = 0;
    glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &binding);
    EXPECT_EQ(binding, caller_draw);
    glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &binding);
    EXPECT_EQ(binding, caller_read);
    glGetIntegerv(GL_VIEWPORT, viewport.data());
    EXPECT_EQ(viewport, (std::array<GLint, 4>{2, 3, 28, 27}));
    glGetIntegerv(GL_SCISSOR_BOX, viewport.data());
    EXPECT_EQ(viewport, (std::array<GLint, 4>{1, 2, 3, 4}));
    EXPECT_TRUE(glIsEnabled(GL_SCISSOR_TEST));
    glBindFramebuffer(GL_READ_FRAMEBUFFER, caller_draw);
    glReadPixels(9, 11, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixel.data());
    EXPECT_EQ(pixel, (std::array<unsigned char, 4>{255, 0, 0, 255}));
    float depth = 0;
    glReadPixels(9, 11, 1, 1, GL_DEPTH_COMPONENT, GL_FLOAT, &depth);
    EXPECT_NEAR(depth, .25f, 1e-6f);
    glReadPixels(5, 7, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixel.data());
    EXPECT_EQ(pixel, (std::array<unsigned char, 4>{0, 255, 0, 255}));
    glReadPixels(5, 7, 1, 1, GL_DEPTH_COMPONENT, GL_FLOAT, &depth);
    EXPECT_NEAR(depth, .75f, 1e-6f);
    target.Clear();
    EXPECT_FALSE(glIsFramebuffer(owned));
    ASSERT_TRUE(target.Begin(view, false));
    target.Present(view, view);
    target.Clear();
    EXPECT_EQ(glGetError(), GLenum(GL_NO_ERROR));
    render::Device_OnContextDestroyed();
    developer = saved_developer;
}

TEST_F(ScenePreviewGlTest, AuthoringTargetPreservesColorFormatsSamplesAndPackedDepth)
{
    goldsrc::cvar_t quiet{};
    goldsrc::cvar_t* saved_developer = developer;
    developer = &quiet;
    render::Device_OnContextCreated();
    struct FormatCase
    {
        GLenum color;
        GLenum depth;
        int samples;
    };
    const FormatCase cases[] = {
        {GL_RGBA8, GL_DEPTH_COMPONENT24, 0},
        {GL_SRGB8_ALPHA8, GL_DEPTH24_STENCIL8, 0},
        {GL_RGBA16F, GL_DEPTH32F_STENCIL8, 0},
        {GL_RGBA8, GL_DEPTH_COMPONENT16, 4},
        {GL_RGBA8, GL_DEPTH24_STENCIL8, 4},
    };
    render::preview::PreviewSceneTarget target(render::GetRendererState().resources.device);
    for (const FormatCase& item : cases)
    {
        SCOPED_TRACE(item.color);
        SCOPED_TRACE(item.depth);
        SCOPED_TRACE(item.samples);
        GLuint output = 0;
        GLuint images[2]{};
        glGenFramebuffers(1, &output);
        glGenRenderbuffers(2, images);
        glBindFramebuffer(GL_FRAMEBUFFER, output);
        for (int index = 0; index < 2; index++)
        {
            glBindRenderbuffer(GL_RENDERBUFFER, images[index]);
            glRenderbufferStorageMultisample(GL_RENDERBUFFER, item.samples, index ? item.depth : item.color, 32, 32);
            glFramebufferRenderbuffer(GL_FRAMEBUFFER, index ? GL_DEPTH_ATTACHMENT : GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, images[index]);
        }
        if (item.depth == GL_DEPTH24_STENCIL8 || item.depth == GL_DEPTH32F_STENCIL8)
        {
            glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_RENDERBUFFER, images[1]);
        }
        ASSERT_EQ(glCheckFramebufferStatus(GL_FRAMEBUFFER), GLenum(GL_FRAMEBUFFER_COMPLETE));
        glDisable(GL_SCISSOR_TEST);
        glDisable(GL_FRAMEBUFFER_SRGB);
        glClearColor(.25f, .5f, .75f, 1);
        glClearStencil(7);
        glClear(GL_COLOR_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
        GLint expected_samples = 0;
        glGetIntegerv(GL_SAMPLES, &expected_samples);
        glReadBuffer(GL_NONE);
        glEnable(GL_FRAMEBUFFER_SRGB);
        const PreviewRect view{4, 4, 16, 16};
        ASSERT_TRUE(target.Begin(view, true));
        GLint saved_renderbuffer = 0;
        GLint color_attachment = 0;
        GLint color_format = 0;
        glGetIntegerv(GL_RENDERBUFFER_BINDING, &saved_renderbuffer);
        glGetFramebufferAttachmentParameteriv(
            GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME, &color_attachment
        );
        glBindRenderbuffer(GL_RENDERBUFFER, static_cast<GLuint>(color_attachment));
        glGetRenderbufferParameteriv(GL_RENDERBUFFER, GL_RENDERBUFFER_INTERNAL_FORMAT, &color_format);
        glBindRenderbuffer(GL_RENDERBUFFER, static_cast<GLuint>(saved_renderbuffer));
        EXPECT_EQ(static_cast<GLenum>(color_format), item.color);

        GLint samples = 0;
        glGetIntegerv(GL_SAMPLES, &samples);
        EXPECT_EQ(samples, expected_samples);
        EXPECT_TRUE(glIsEnabled(GL_FRAMEBUFFER_SRGB));
        if (item.samples == 0 && (item.depth == GL_DEPTH24_STENCIL8 || item.depth == GL_DEPTH32F_STENCIL8))
        {
            unsigned char stencil = 0;
            glReadPixels(8, 8, 1, 1, GL_STENCIL_INDEX, GL_UNSIGNED_BYTE, &stencil);
            EXPECT_EQ(stencil, 7);
            glClearStencil(3);
            glClear(GL_STENCIL_BUFFER_BIT);
        }
        glClearDepth(.5);
        glClear(GL_DEPTH_BUFFER_BIT);
        target.Present(view, view);
        GLint read_buffer = 0;
        glGetIntegerv(GL_READ_BUFFER, &read_buffer);
        EXPECT_EQ(read_buffer, GL_NONE);
        EXPECT_TRUE(glIsEnabled(GL_FRAMEBUFFER_SRGB));
        if (item.samples == 0)
        {
            glReadBuffer(GL_COLOR_ATTACHMENT0);
            std::array<float, 4> pixel{};
            glReadPixels(8, 8, 1, 1, GL_RGBA, GL_FLOAT, pixel.data());
            EXPECT_NEAR(pixel[0], .25f, .005f);
            EXPECT_NEAR(pixel[1], .5f, .005f);
            EXPECT_NEAR(pixel[2], .75f, .005f);
            float depth = 0;
            glReadPixels(8, 8, 1, 1, GL_DEPTH_COMPONENT, GL_FLOAT, &depth);
            EXPECT_NEAR(depth, .5f, 1e-6f);
            if (item.depth == GL_DEPTH24_STENCIL8 || item.depth == GL_DEPTH32F_STENCIL8)
            {
                unsigned char stencil = 0;
                glReadPixels(8, 8, 1, 1, GL_STENCIL_INDEX, GL_UNSIGNED_BYTE, &stencil);
                EXPECT_EQ(stencil, 7);
            }
        }
        EXPECT_EQ(glGetError(), GLenum(GL_NO_ERROR));
        glDeleteFramebuffers(1, &output);
        glDeleteRenderbuffers(2, images);
    }
    target.Clear();
    render::Device_OnContextDestroyed();
    developer = saved_developer;
}
TEST_F(ScenePreviewGlTest, AuthoringTargetLeavesPixelBuffersAndWriteMasksUntouchedAcrossDeviceReset)
{
    goldsrc::cvar_t quiet{};
    goldsrc::cvar_t* saved_developer = developer;
    developer = &quiet;
    render::Device_OnContextCreated();
    render::preview::PreviewSceneTarget target(render::GetRendererState().resources.device);
    GLuint buffers[2]{};
    glGenBuffers(2, buffers);
    glBindBuffer(GL_PIXEL_PACK_BUFFER, buffers[0]);
    glBufferData(GL_PIXEL_PACK_BUFFER, 64, nullptr, GL_STREAM_READ);
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, buffers[1]);
    glBufferData(GL_PIXEL_UNPACK_BUFFER, 64, nullptr, GL_STREAM_DRAW);
    glColorMask(GL_FALSE, GL_TRUE, GL_FALSE, GL_TRUE);
    glDepthMask(GL_FALSE);
    const PreviewRect view{0, 0, 16, 16};
    ASSERT_TRUE(target.Begin(view, true));
    GLint old_framebuffer = 0;
    GLint old_color = 0;
    GLint old_depth = 0;
    glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &old_framebuffer);
    glGetFramebufferAttachmentParameteriv(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME, &old_color);
    glGetFramebufferAttachmentParameteriv(GL_DRAW_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME, &old_depth);
    target.Present(view, view);
    GLint binding = 0;
    glGetIntegerv(GL_PIXEL_PACK_BUFFER_BINDING, &binding);
    EXPECT_EQ(static_cast<GLuint>(binding), buffers[0]);
    glGetIntegerv(GL_PIXEL_UNPACK_BUFFER_BINDING, &binding);
    EXPECT_EQ(static_cast<GLuint>(binding), buffers[1]);
    std::array<GLboolean, 4> mask{};
    glGetBooleanv(GL_COLOR_WRITEMASK, mask.data());
    EXPECT_EQ(mask, (std::array<GLboolean, 4>{GL_FALSE, GL_TRUE, GL_FALSE, GL_TRUE}));
    GLboolean depth_mask = GL_TRUE;
    glGetBooleanv(GL_DEPTH_WRITEMASK, &depth_mask);
    EXPECT_FALSE(depth_mask);
    // Keep the test context live to prove that loss notification never deletes old-context names.
    render::Device_OnContextDestroyed();
    target.Clear();
    EXPECT_TRUE(glIsFramebuffer(old_framebuffer));
    EXPECT_TRUE(glIsRenderbuffer(old_color));
    EXPECT_TRUE(glIsRenderbuffer(old_depth));
    render::Device_OnContextCreated();
    ASSERT_TRUE(target.Begin(view, false));
    glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &binding);
    EXPECT_NE(binding, old_framebuffer);
    target.Present(view, view);
    target.Clear();
    GLuint old_images[] = {static_cast<GLuint>(old_color), static_cast<GLuint>(old_depth)};
    GLuint old_target = old_framebuffer;
    glDeleteFramebuffers(1, &old_target);
    glDeleteRenderbuffers(2, old_images);
    glDeleteBuffers(2, buffers);
    EXPECT_EQ(glGetError(), GLenum(GL_NO_ERROR));
    render::Device_OnContextDestroyed();
    developer = saved_developer;
}

TEST_F(ScenePreviewGlTest, LiveTargetClearRestoresCallerBeforeDeletingItsObjects)
{
    goldsrc::cvar_t quiet{};
    goldsrc::cvar_t* saved_developer = developer;
    developer = &quiet;
    render::Device_OnContextCreated();
    GLint caller_read = 0;
    GLint caller_draw = 0;
    glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &caller_read);
    glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &caller_draw);
    render::preview::PreviewSceneTarget target(render::GetRendererState().resources.device);
    ASSERT_TRUE(target.Begin({0, 0, 16, 16}, true));
    GLint owned = 0;
    glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &owned);
    ASSERT_NE(owned, caller_draw);
    target.Clear();
    GLint binding = 0;
    glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &binding);
    EXPECT_EQ(binding, caller_read);
    glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &binding);
    EXPECT_EQ(binding, caller_draw);
    EXPECT_FALSE(glIsFramebuffer(static_cast<GLuint>(owned)));
    target.Clear();
    EXPECT_EQ(glGetError(), GLenum(GL_NO_ERROR));
    render::Device_OnContextDestroyed();
    developer = saved_developer;
}
