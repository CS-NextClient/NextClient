#include <algorithm>
#include <array>
#include <cmath>
#include <string>
#include <vector>

#include <glad/glad.h>
#include <goldsrc/client_api.h>
#include <gtest/gtest.h>
#include <ncl_gl/loader.h>

#include "RadarGlContext.h"
#include "hud/radar/markers/RadarLayerMarker.h"
#include "hud/radar/render/RadarDraw.h"

goldsrc::cl_enginefunc_t gEngfuncs{};

// RadarGlContext already resolves the entry points for its native or EGL context.
bool ncl_gl::EnsureLoaded()
{
    return GLAD_GL_VERSION_2_0 != 0;
}

bool ncl_gl::IsLoaded()
{
    return GLAD_GL_VERSION_2_0 != 0;
}

namespace
{
    constexpr int kImageSize = 128;

    void ShaderFailure(const char* format, ...)
    {
        ADD_FAILURE() << format;
    }

    class RadarDrawGlTest : public testing::Test
    {
    private:
        RadarGlContext context_;
        bool context_ready_{};
        GLuint texture_{};
        GLuint framebuffer_{};

    protected:
        void SetUp() override
        {
            std::string error;
            ASSERT_TRUE(context_.Initialize(error)) << error;
            context_ready_ = true;
            RecordProperty("renderer", reinterpret_cast<const char*>(glGetString(GL_RENDERER)));
            gEngfuncs.Con_DPrintf = ShaderFailure;
            RadarDraw_ResetGl();
            ASSERT_TRUE(RadarDraw_CheckPrimsAvailable());
            RadarDraw_ClearFade();
            RadarDraw_ClearPrimClip();

            glGenTextures(1, &texture_);
            glBindTexture(GL_TEXTURE_2D, texture_);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, kImageSize, kImageSize, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
            glGenFramebuffers(1, &framebuffer_);
            glBindFramebuffer(GL_FRAMEBUFFER, framebuffer_);
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture_, 0);
            ASSERT_EQ(glCheckFramebufferStatus(GL_FRAMEBUFFER), GL_FRAMEBUFFER_COMPLETE);
            glViewport(0, 0, kImageSize, kImageSize);
            glMatrixMode(GL_PROJECTION);
            glLoadIdentity();
            glOrtho(0, kImageSize, 0, kImageSize, -1, 1);
            glMatrixMode(GL_MODELVIEW);
            glLoadIdentity();
            for (GLenum capability : {GL_ALPHA_TEST, GL_DEPTH_TEST, GL_CULL_FACE, GL_SCISSOR_TEST, GL_LIGHTING, GL_FOG, GL_DITHER})
            {
                glDisable(capability);
            }
            glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
            EXPECT_EQ(glGetError(), GL_NO_ERROR);
        }

        void TearDown() override
        {
            if (!context_ready_)
            {
                return;
            }
            RadarDraw_ResetGl();
            glDeleteTextures(1, &texture_);
            glDeleteFramebuffers(1, &framebuffer_);
        }

        std::vector<unsigned char> RenderMarkers(bool bomb)
        {
            glClearColor(0, 0, 0, 0);
            glClear(GL_COLOR_BUFFER_BIT);
            RadarDraw_BeginPrims();
            if (bomb)
            {
                RadarDraw_DrawBomb({64, 64}, 8, {255, 215, 95, 102});
            }
            else
            {
                RadarDraw_DrawDisc({44, 64}, 5, {255, 0, 0, 128});
                RadarDraw_DrawDisc({84, 64}, 5, {255, 0, 0, 128});
            }
            RadarDraw_EndPrims();
            std::vector<unsigned char> pixels(kImageSize * kImageSize * 4);
            glReadPixels(0, 0, kImageSize, kImageSize, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
            EXPECT_EQ(glGetError(), GL_NO_ERROR);
            return pixels;
        }

        std::vector<unsigned char> RenderFullPlayerSet(bool batched, int alpha)
        {
            glClearColor(0, 0, 0, 0);
            glClear(GL_COLOR_BUFFER_BIT);
            RadarDraw_BeginPrims();
            for (int player = 0; player < 32; player++)
            {
                ncl_math::Vector2 center{8.0f + (player % 8) * 16.0f, 16.0f + (player / 8) * 28.0f};
                ncl_math::Color color{player % 2 ? 255 : 80, 160, 220, alpha};
                ncl_math::Vector2 trail[128];
                uint8_t trail_alpha[128];
                for (int point = 0; point < 128; point++)
                {
                    trail[point] = {center.x + point * 0.025f, center.y - 6.0f};
                    trail_alpha[point] = (uint8_t)point;
                }
                RadarDraw_DrawPolyline(trail, trail_alpha, color, 1.5f);
                if (!batched)
                {
                    RadarDraw_FlushPrims();
                }
            }
            for (int player = 0; player < 32; player++)
            {
                ncl_math::Vector2 center{8.0f + (player % 8) * 16.0f, 16.0f + (player / 8) * 28.0f};
                ncl_math::Color color{player % 2 ? 255 : 80, 160, 220, alpha};
                for (int copy = 0; copy < 2; copy++)
                {
                    ncl_math::Vector2 pos = center + ncl_math::Vector2{(float)copy * 3.0f, 0.0f};
                    RadarDraw_DrawDisc(pos, 3, color);
                    RadarDraw_DrawTriangle(
                        pos + ncl_math::Vector2{0, 6}, pos + ncl_math::Vector2{-2, 2}, pos + ncl_math::Vector2{2, 2}, color
                    );
                    RadarDraw_DrawRing(pos, 3, 2, color);
                    RadarDraw_DrawRect(pos + ncl_math::Vector2{-3, -9}, pos + ncl_math::Vector2{3, -8}, color);
                    if (!batched)
                    {
                        RadarDraw_FlushPrims();
                    }
                }
            }
            RadarDraw_EndPrims();
            std::vector<unsigned char> pixels(kImageSize * kImageSize * 4);
            glReadPixels(0, 0, kImageSize, kImageSize, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
            EXPECT_EQ(glGetError(), GL_NO_ERROR);
            return pixels;
        }

        std::vector<unsigned char> RenderPlayerMarker(
            const RadarDrawPlayerMarker& current,
            const RadarDrawPlayerMarker& previous,
            bool pair
        )
        {
            glClearColor(0, 0, 0, 0);
            glClear(GL_COLOR_BUFFER_BIT);
            RadarDraw_BeginPrims();
            const ncl_math::Color color{80, 160, 220};
            const float radius = 5.0f;
            if (pair)
            {
                RadarDraw_DrawPlayerMarkerFade(current, previous, radius, color);
            }
            else
            {
                ncl_math::Color single = color.WithAlpha((int)(255 * current.opacity));
                ncl_math::Vector2 direction{};
                bool directed = ncl_math::TryNormalize(current.direction, &direction);
                if (current.offscreen && directed)
                {
                    ncl_math::Vector2 base = current.pos - direction * (radius * 0.8f);
                    ncl_math::Vector2 width = direction.Perp() * (radius * 1.2f);
                    RadarDraw_DrawTriangle(current.pos + direction * (radius * 1.7f), base + width, base - width, single);
                }
                else
                {
                    if (directed)
                    {
                        RadarDraw_DrawTriangle(
                            current.pos + direction * (radius * 2.0f),
                            current.pos + direction.Rotated(0.62f) * radius,
                            current.pos + direction.Rotated(-0.62f) * radius,
                            {240, 240, 240, single.a}
                        );
                    }
                    RadarDraw_DrawDisc(current.pos, radius, single);
                }
            }
            RadarDraw_EndPrims();
            std::vector<unsigned char> pixels(kImageSize * kImageSize * 4);
            glReadPixels(0, 0, kImageSize, kImageSize, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
            EXPECT_EQ(glGetError(), GL_NO_ERROR);
            return pixels;
        }

        int VisiblePixels(const std::vector<unsigned char>& pixels)
        {
            int count = 0;
            for (size_t index = 3; index < pixels.size(); index += 4)
            {
                count += pixels[index] > 10;
            }
            return count;
        }
    };
} // namespace

TEST_F(RadarDrawGlTest, TranslucentBombKeepsItsPixelsWithInheritedAlphaTest)
{
    std::vector<unsigned char> expected = RenderMarkers(true);
    ASSERT_GT(VisiblePixels(expected), 80);
    for (float cutoff : {0.25f, 0.5f, 0.9f})
    {
        SCOPED_TRACE(cutoff);
        glAlphaFunc(GL_GREATER, cutoff);
        glEnable(GL_ALPHA_TEST);
        std::vector<unsigned char> actual = RenderMarkers(true);
        EXPECT_EQ(actual, expected);
        EXPECT_EQ(glIsEnabled(GL_ALPHA_TEST), GL_TRUE);
        GLint function = 0;
        GLfloat reference = 0;
        glGetIntegerv(GL_ALPHA_TEST_FUNC, &function);
        glGetFloatv(GL_ALPHA_TEST_REF, &reference);
        EXPECT_EQ(function, GL_GREATER);
        EXPECT_FLOAT_EQ(reference, cutoff);
    }
}

TEST_F(RadarDrawGlTest, HeightIndicatorStaysCloseToRenderedPlayerAndEdgeArrows)
{
    auto boundary = [](const std::vector<unsigned char>& pixels, bool maximum) {
        int result = maximum ? -1 : kImageSize;
        for (int y = 0; y < kImageSize; ++y)
        {
            for (int x = 0; x < kImageSize; ++x)
            {
                if (pixels[((size_t)y * kImageSize + x) * 4 + 3] > 10)
                {
                    result = maximum ? std::max(result, y) : std::min(result, y);
                }
            }
        }
        return result;
    };
    RadarShape shape{{64, 64}, 60, true};
    for (bool offscreen : {false, true})
    {
        for (int vertical : {-1, 1})
        {
            for (int step = 0; step < 24; ++step)
            {
                SCOPED_TRACE(testing::Message() << "offscreen=" << offscreen << " vertical=" << vertical << " step=" << step);
                ncl_math::Vector2 direction = ncl_math::Vector2{1, 0}.Rotated(step * ncl_math::kPi / 12.0f) * (step % 2 ? 8.0f : 0.125f);
                float height = RadarLayerMarker_PlayerHalfHeight(direction, 5, vertical, offscreen);
                RadarLayerPlacement placement = RadarLayerMarker_Place(shape, shape.center, 12, {1, vertical}, 5, height);
                RadarDrawPlayerMarker marker{.pos = placement.marker, .direction = direction, .offscreen = offscreen};
                std::vector<unsigned char> marker_pixels = RenderPlayerMarker(marker, {}, false);
                glClear(GL_COLOR_BUFFER_BIT);
                RadarDraw_BeginPrims();
                RadarDraw_DrawLayerIndicator(placement.indicator, vertical, 5, {255, 255, 255});
                RadarDraw_EndPrims();
                std::vector<unsigned char> arrow_pixels(kImageSize * kImageSize * 4);
                glReadPixels(0, 0, kImageSize, kImageSize, GL_RGBA, GL_UNSIGNED_BYTE, arrow_pixels.data());
                EXPECT_EQ(glGetError(), GL_NO_ERROR);
                int marker_edge = boundary(marker_pixels, vertical < 0);
                int arrow_edge = boundary(arrow_pixels, vertical > 0);
                int gap = vertical * (marker_edge - arrow_edge) - 1;
                EXPECT_GE(gap, 0);
                EXPECT_LE(gap, 3);
            }
        }
    }
}

TEST_F(RadarDrawGlTest, PlayerCrossfadeKeepsItsPixelsWithInheritedAlphaTest)
{
    std::vector<unsigned char> expected = RenderMarkers(false);
    ASSERT_GT(VisiblePixels(expected), 80);
    for (float cutoff : {0.25f, 0.5f, 0.9f})
    {
        SCOPED_TRACE(cutoff);
        glAlphaFunc(GL_GREATER, cutoff);
        glEnable(GL_ALPHA_TEST);
        EXPECT_EQ(RenderMarkers(false), expected);
        EXPECT_EQ(glIsEnabled(GL_ALPHA_TEST), GL_TRUE);
        glDisable(GL_ALPHA_TEST);
        EXPECT_EQ(RenderMarkers(false), expected);
        EXPECT_EQ(glIsEnabled(GL_ALPHA_TEST), GL_FALSE);
    }
}

TEST_F(RadarDrawGlTest, FullPlayerSetAndTrailsMatchSeparateDrawsAcrossRepeatedFrames)
{
    for (int alpha : {255, 128, 64})
    {
        SCOPED_TRACE(alpha);
        std::vector<unsigned char> expected = RenderFullPlayerSet(false, alpha);
        ASSERT_GT(VisiblePixels(expected), 800);
        for (int frame = 0; frame < 16; frame++)
        {
            SCOPED_TRACE(frame);
            RadarDraw_ResetStats();
            EXPECT_EQ(RenderFullPlayerSet(true, alpha), expected);
            EXPECT_EQ(RadarDraw_GetStats().batches, 1);
            EXPECT_EQ(RadarDraw_GetStats().quads, 4320);
        }
    }
}

TEST_F(RadarDrawGlTest, CoincidentPlayerCrossfadeKeepsOpaqueMarkerBrightness)
{
    auto render = [](float weight) {
        glClearColor(0, 0, 0, 0);
        glClear(GL_COLOR_BUFFER_BIT);
        RadarDraw_BeginPrims();
        RadarDraw_DrawPlayerMarkerFade({{64, 64}, {}, 1.0f, weight, false}, {{64, 64}, {}, 1.0f, 1.0f - weight, false}, 5, {255, 0, 0});
        RadarDraw_EndPrims();
        std::array<unsigned char, 4> pixel{};
        glReadPixels(64, 64, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixel.data());
        return pixel;
    };
    for (float weight : {0.0f, 0.25f, 0.5f, 0.75f, 1.0f})
    {
        SCOPED_TRACE(weight);
        std::array<unsigned char, 4> pixel = render(weight);
        EXPECT_EQ(pixel[0], 255);
        EXPECT_EQ(pixel[3], 255);
    }
}

TEST_F(RadarDrawGlTest, PlayerMarkerFadePreservesCoincidentGlyphsAndLayerOpacity)
{
    for (bool offscreen : {false, true})
    {
        for (float opacity : {1.0f, 102.0f / 255.0f})
        {
            RadarDrawPlayerMarker marker{{64.25f, 63.75f}, {0.6f, -0.8f}, opacity, 1.0f, offscreen};
            std::vector<unsigned char> expected = RenderPlayerMarker(marker, {}, false);
            for (float weight : {0.0f, 0.25f, 0.5f, 0.75f, 1.0f})
            {
                SCOPED_TRACE(offscreen);
                SCOPED_TRACE(opacity);
                SCOPED_TRACE(weight);
                RadarDrawPlayerMarker current = marker;
                RadarDrawPlayerMarker previous = marker;
                current.weight = weight;
                previous.weight = 1.0f - weight;
                std::vector<unsigned char> actual = RenderPlayerMarker(current, previous, true);
                int max_error = 0;
                for (size_t index = 0; index < actual.size(); index++)
                {
                    if (index % 4 != 3)
                    {
                        max_error = std::max(max_error, std::abs((int)actual[index] - (int)expected[index]));
                    }
                }
                EXPECT_LE(max_error, 2);
                size_t center = (64 * kImageSize + 64) * 4;
                EXPECT_NEAR(actual[center], 80 * opacity, 1);
                EXPECT_NEAR(actual[center + 1], 160 * opacity, 1);
                EXPECT_NEAR(actual[center + 2], 220 * opacity, 1);
            }
        }
    }
}

TEST_F(RadarDrawGlTest, PlayerMarkerFadeInterpolatesOverlappingCoverage)
{
    RadarDrawPlayerMarker current{{65.25f, 64}, {}, 1.0f, 1.0f, false};
    RadarDrawPlayerMarker previous{{62.75f, 64}, {}, 1.0f, 1.0f, false};
    std::vector<unsigned char> new_pixels = RenderPlayerMarker(current, {}, false);
    std::vector<unsigned char> old_pixels = RenderPlayerMarker(previous, {}, false);
    for (float weight : {0.25f, 0.5f, 0.75f})
    {
        SCOPED_TRACE(weight);
        current.weight = weight;
        previous.weight = 1.0f - weight;
        std::vector<unsigned char> actual = RenderPlayerMarker(current, previous, true);
        int max_error = 0;
        for (size_t index = 0; index < actual.size(); index++)
        {
            if (index % 4 == 3)
            {
                continue;
            }
            int expected = (int)(new_pixels[index] * weight + old_pixels[index] * (1.0f - weight));
            max_error = std::max(max_error, std::abs((int)actual[index] - expected));
        }
        EXPECT_LE(max_error, 1);
    }
}

TEST_F(RadarDrawGlTest, PlayerMarkerFadeKeepsDisjointConfirmedPositions)
{
    RadarDrawPlayerMarker current{{84, 64}, {}, 1.0f, 0.5f, false};
    RadarDrawPlayerMarker previous{{44, 64}, {}, 1.0f, 0.5f, false};
    glAlphaFunc(GL_GREATER, 0.9f);
    glEnable(GL_ALPHA_TEST);
    std::vector<unsigned char> pixels = RenderPlayerMarker(current, previous, true);
    EXPECT_EQ(glIsEnabled(GL_ALPHA_TEST), GL_TRUE);
    for (int x : {44, 84})
    {
        size_t offset = (64 * kImageSize + x) * 4;
        EXPECT_NEAR(pixels[offset], 40, 1);
        EXPECT_NEAR(pixels[offset + 1], 80, 1);
        EXPECT_NEAR(pixels[offset + 2], 110, 1);
    }
    size_t middle = (64 * kImageSize + 64) * 4;
    EXPECT_EQ(pixels[middle], 0);
    EXPECT_EQ(pixels[middle + 3], 0);
}

TEST_F(RadarDrawGlTest, FullPlayerMarkerFadesKeepBrightnessAcrossRepeatedFrames)
{
    auto render = [](bool pair, float weight) {
        glClearColor(0, 0, 0, 0);
        glClear(GL_COLOR_BUFFER_BIT);
        RadarDraw_ResetStats();
        RadarDraw_BeginPrims();
        for (int player = 0; player < 32; player++)
        {
            ncl_math::Vector2 center{8.0f + (player % 8) * 16.0f, 16.0f + (player / 8) * 28.0f};
            ncl_math::Color color{player % 2 ? 255 : 80, 160, 220};
            if (pair)
            {
                RadarDraw_DrawPlayerMarkerFade({center, {}, 1.0f, weight, false}, {center, {}, 1.0f, 1.0f - weight, false}, 3, color);
            }
            else
            {
                RadarDraw_DrawDisc(center, 3, color);
            }
        }
        RadarDraw_EndPrims();
        std::vector<unsigned char> pixels(kImageSize * kImageSize * 4);
        glReadPixels(0, 0, kImageSize, kImageSize, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
        return pixels;
    };
    std::vector<unsigned char> expected = render(false, 1.0f);
    ASSERT_GT(VisiblePixels(expected), 400);
    for (int frame = 0; frame < 32; frame++)
    {
        SCOPED_TRACE(frame);
        std::vector<unsigned char> actual = render(true, (float)frame / 31.0f);
        int max_error = 0;
        for (size_t index = 0; index < actual.size(); index++)
        {
            max_error = std::max(max_error, std::abs((int)actual[index] - (int)expected[index]));
        }
        EXPECT_LE(max_error, 1);
        EXPECT_EQ(RadarDraw_GetStats().batches, 1);
        EXPECT_EQ(RadarDraw_GetStats().quads, 32);
    }
    EXPECT_EQ(glGetError(), GL_NO_ERROR);
}

TEST_F(RadarDrawGlTest, PlayerMarkerFadeKeepsSeparateGlyphKindsAndLayerOpacities)
{
    for (bool offscreen : {false, true})
    {
        RadarDrawPlayerMarker current{{65.25f, 63.75f}, {0.6f, -0.8f}, 102.0f / 255.0f, 1.0f, offscreen};
        RadarDrawPlayerMarker previous{{62.75f, 64.25f}, {-1, 0}, 1.0f, 1.0f, !offscreen};
        std::vector<unsigned char> new_pixels = RenderPlayerMarker(current, {}, false);
        std::vector<unsigned char> old_pixels = RenderPlayerMarker(previous, {}, false);
        for (float weight : {0.0f, 0.25f, 0.5f, 0.75f, 1.0f})
        {
            SCOPED_TRACE(offscreen);
            SCOPED_TRACE(weight);
            current.weight = weight;
            previous.weight = 1.0f - weight;
            std::vector<unsigned char> actual = RenderPlayerMarker(current, previous, true);
            int max_error = 0;
            for (size_t index = 0; index < actual.size(); index++)
            {
                if (index % 4 == 3)
                {
                    continue;
                }
                int expected = (int)(new_pixels[index] * weight + old_pixels[index] * (1.0f - weight));
                max_error = std::max(max_error, std::abs((int)actual[index] - expected));
            }
            EXPECT_LE(max_error, 2);
        }
    }
}

TEST_F(RadarDrawGlTest, BlurCaptureIgnoresIncomingScissor)
{
    glClearColor(0, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT);
    ASSERT_TRUE(RadarDraw_DrawBlurBackground(
        64,
        64,
        32,
        true,
        0.5f,
        1.0f,
        128,
        128
    ));

    glClearColor(1, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT);
    glScissor(64, 64, 32, 32);
    glEnable(GL_SCISSOR_TEST);
    ASSERT_TRUE(RadarDraw_DrawBlurBackground(
        64,
        64,
        32,
        true,
        0.5f,
        1.0f,
        128,
        128
    ));

    std::array<unsigned char, 4> clipped{};
    glReadPixels(80, 80, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, clipped.data());

    EXPECT_EQ(glIsEnabled(GL_SCISSOR_TEST), GL_TRUE);
    std::array<GLint, 4> scissor{};
    glGetIntegerv(GL_SCISSOR_BOX, scissor.data());
    EXPECT_EQ(scissor, (std::array<GLint, 4>{64, 64, 32, 32}));

    glDisable(GL_SCISSOR_TEST);
    glClear(GL_COLOR_BUFFER_BIT);
    ASSERT_TRUE(RadarDraw_DrawBlurBackground(
        64,
        64,
        32,
        true,
        0.5f,
        1.0f,
        128,
        128
    ));

    std::array<unsigned char, 4> reference{};
    glReadPixels(80, 80, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, reference.data());
    EXPECT_NEAR(reference[0], 127, 1);
    EXPECT_EQ(clipped, reference);
    EXPECT_EQ(glGetError(), GL_NO_ERROR);
}

TEST_F(RadarDrawGlTest, BlurCaptureRestoresStateAcrossAllocationAndResize)
{
    GLint draw_fb = 0;
    glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &draw_fb);

    GLuint read_texture = 0;
    GLuint read_fb = 0;
    glGenTextures(1, &read_texture);
    glBindTexture(GL_TEXTURE_2D, read_texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, kImageSize, kImageSize, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);

    glGenFramebuffers(1, &read_fb);
    glBindFramebuffer(GL_FRAMEBUFFER, read_fb);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, read_texture, 0);
    ASSERT_EQ(glCheckFramebufferStatus(GL_FRAMEBUFFER), GL_FRAMEBUFFER_COMPLETE);

    glClearColor(0, 1, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT);
    glReadBuffer(GL_NONE);
    glBindFramebuffer(GL_FRAMEBUFFER, draw_fb);

    for (bool split_fbos : {false, true})
    {
        std::array<std::array<unsigned char, 4>, 3> reference{};

        for (bool clipped : {false, true})
        {
            RadarDraw_ResetGl();
            int step = 0;

            for (int size : {32, 32, 48})
            {
                SCOPED_TRACE(testing::Message() << "split=" << split_fbos << "clipped=" << clipped << "step=" << step);
                glBindFramebuffer(GL_DRAW_FRAMEBUFFER, draw_fb);
                glBindFramebuffer(GL_READ_FRAMEBUFFER, split_fbos ? read_fb : draw_fb);
                glDisable(GL_SCISSOR_TEST);
                glClearColor(1, 0, 0, 1);
                glClear(GL_COLOR_BUFFER_BIT);

                glActiveTexture(GL_TEXTURE0);
                glBindTexture(GL_TEXTURE_2D, read_texture);
                glActiveTexture(GL_TEXTURE1);
                glBindTexture(GL_TEXTURE_2D, read_texture);

                glScissor(64, 64, 16, 16);
                if (clipped)
                {
                    glEnable(GL_SCISSOR_TEST);
                }

                ASSERT_TRUE(RadarDraw_DrawBlurBackground(
                    64,
                    64,
                    (float)size,
                    true,
                    0.5f,
                    1.0f,
                    128,
                    128
                ));

                GLint state = 0;
                glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &state);
                EXPECT_EQ(state, split_fbos ? (GLint)read_fb : draw_fb);
                glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &state);
                EXPECT_EQ(state, draw_fb);
                glGetIntegerv(GL_READ_BUFFER, &state);
                EXPECT_EQ(state, split_fbos ? GL_NONE : GL_COLOR_ATTACHMENT0);
                glGetIntegerv(GL_DRAW_BUFFER, &state);
                EXPECT_EQ(state, GL_COLOR_ATTACHMENT0);

                glGetIntegerv(GL_ACTIVE_TEXTURE, &state);
                EXPECT_EQ(state, GL_TEXTURE1);
                glGetIntegerv(GL_TEXTURE_BINDING_2D, &state);
                EXPECT_EQ(state, (GLint)read_texture);
                glActiveTexture(GL_TEXTURE0);
                glGetIntegerv(GL_TEXTURE_BINDING_2D, &state);
                EXPECT_EQ(state, (GLint)read_texture);

                EXPECT_EQ(glIsEnabled(GL_SCISSOR_TEST), clipped ? GL_TRUE : GL_FALSE);
                std::array<GLint, 4> box{};
                glGetIntegerv(GL_SCISSOR_BOX, box.data());
                EXPECT_EQ(box, (std::array<GLint, 4>{64, 64, 16, 16}));

                glBindFramebuffer(GL_READ_FRAMEBUFFER, draw_fb);
                std::array<unsigned char, 4> pixel{};
                glReadPixels(72, 72, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixel.data());
                EXPECT_NEAR(pixel[0], 127, 1);

                if (clipped)
                {
                    EXPECT_EQ(pixel, reference[step]);
                }
                else
                {
                    reference[step] = pixel;
                }

                glReadPixels(88, 72, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixel.data());
                EXPECT_NEAR(pixel[0], clipped ? 255 : 127, 1);
                EXPECT_EQ(glGetError(), GL_NO_ERROR);
                step++;
            }
        }
    }

    glDisable(GL_SCISSOR_TEST);
    glBindFramebuffer(GL_FRAMEBUFFER, draw_fb);
    glDeleteFramebuffers(1, &read_fb);
    glDeleteTextures(1, &read_texture);
}
