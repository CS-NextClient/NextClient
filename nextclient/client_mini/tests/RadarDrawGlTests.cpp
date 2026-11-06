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
#include "RadarGlAllocations.h"
#include "RadarRecordingMonitor.h"
#include "hud/radar/integration/Performance.h"
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
    protected:
        RadarGlContext context_;
        bool context_ready_{};
        GLuint texture_{};
        GLuint framebuffer_{};

    protected:
        RadarRecordingMonitor monitor;
        radar::integration::Performance performance_{&monitor};
        RadarDrawContext draw_{performance_};

        void SetUp() override
        {
            std::string error;
            ASSERT_TRUE(context_.Initialize(error)) << error;
            context_ready_ = true;
            RecordProperty("renderer", reinterpret_cast<const char*>(glGetString(GL_RENDERER)));
            gEngfuncs.Con_DPrintf = ShaderFailure;
            draw_.ResetGl(true);
            draw_.Prepare(RadarGlContext::Current(), nullptr);
            ASSERT_TRUE(draw_.CheckPrimsAvailable());
            draw_.ClearFade();
            draw_.ClearPrimClip();

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
            draw_.ResetGl(true);
            glDeleteTextures(1, &texture_);
            glDeleteFramebuffers(1, &framebuffer_);
        }

        std::vector<unsigned char> RenderMarkers(bool bomb)
        {
            glClearColor(0, 0, 0, 0);
            glClear(GL_COLOR_BUFFER_BIT);
            draw_.BeginPrims();
            if (bomb)
            {
                draw_.DrawBomb({64, 64}, 8, {255, 215, 95, 102});
            }
            else
            {
                draw_.DrawDisc({44, 64}, 5, {255, 0, 0, 128});
                draw_.DrawDisc({84, 64}, 5, {255, 0, 0, 128});
            }
            draw_.EndPrims();
            std::vector<unsigned char> pixels(kImageSize * kImageSize * 4);
            glReadPixels(0, 0, kImageSize, kImageSize, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
            EXPECT_EQ(glGetError(), GL_NO_ERROR);
            return pixels;
        }

        std::vector<unsigned char> RenderFullPlayerSet(bool batched, int alpha)
        {
            glClearColor(0, 0, 0, 0);
            glClear(GL_COLOR_BUFFER_BIT);
            draw_.BeginPrims();
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
                draw_.DrawPolyline(trail, trail_alpha, color, 1.5f);
                if (!batched)
                {
                    draw_.FlushPrims();
                }
            }
            for (int player = 0; player < 32; player++)
            {
                ncl_math::Vector2 center{8.0f + (player % 8) * 16.0f, 16.0f + (player / 8) * 28.0f};
                ncl_math::Color color{player % 2 ? 255 : 80, 160, 220, alpha};
                for (int copy = 0; copy < 2; copy++)
                {
                    ncl_math::Vector2 pos = center + ncl_math::Vector2{(float)copy * 3.0f, 0.0f};
                    draw_.DrawDisc(pos, 3, color);
                    draw_.DrawTriangle(pos + ncl_math::Vector2{0, 6}, pos + ncl_math::Vector2{-2, 2}, pos + ncl_math::Vector2{2, 2}, color);
                    draw_.DrawRing(pos, 3, 2, color);
                    draw_.DrawRect(pos + ncl_math::Vector2{-3, -9}, pos + ncl_math::Vector2{3, -8}, color);
                    if (!batched)
                    {
                        draw_.FlushPrims();
                    }
                }
            }
            draw_.EndPrims();
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
            draw_.BeginPrims();
            const ncl_math::Color color{80, 160, 220};
            const float radius = 5.0f;
            if (pair)
            {
                draw_.DrawPlayerMarkerFade(current, previous, radius, color);
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
                    draw_.DrawTriangle(current.pos + direction * (radius * 1.7f), base + width, base - width, single);
                }
                else
                {
                    if (directed)
                    {
                        draw_.DrawTriangle(
                            current.pos + direction * (radius * 2.0f),
                            current.pos + direction.Rotated(0.62f) * radius,
                            current.pos + direction.Rotated(-0.62f) * radius,
                            {240, 240, 240, single.a}
                        );
                    }
                    draw_.DrawDisc(current.pos, radius, single);
                }
            }
            draw_.EndPrims();
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

TEST_F(RadarDrawGlTest, QueuedMarkersStartGpuOnlyAtActualFlush)
{
    monitor.gpu_begins.clear();
    monitor.gpu_ends = 0;
    monitor.counters.fill(0);
    draw_.ResetStats();
    draw_.BeginPrims();
    draw_.DrawRect({2, 2}, {8, 8}, {255, 255, 255, 255});
    draw_.DrawRect({12, 12}, {18, 18}, {255, 255, 255, 255});
    EXPECT_TRUE(monitor.gpu_begins.empty());
    draw_.FlushPrims();
    EXPECT_EQ(monitor.gpu_begins, (std::vector<ncl_perf::PerfScopeId_t>{performance::PerfStage::RadarBatch}));
    EXPECT_EQ(monitor.gpu_ends, 1u);
    EXPECT_EQ(monitor.counters[uint32_t(performance::PerfCounterId::RadarBatches)], 1u);
    EXPECT_EQ(monitor.counters[uint32_t(performance::PerfCounterId::RadarQuads)], 2u);
    draw_.EndPrims();
    EXPECT_EQ(monitor.gpu_begins.size(), 1u);
    EXPECT_EQ(glGetError(), GL_NO_ERROR);
}

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
                draw_.BeginPrims();
                draw_.DrawLayerIndicator(placement.indicator, vertical, 5, {255, 255, 255});
                draw_.EndPrims();
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
            draw_.ResetStats();
            EXPECT_EQ(RenderFullPlayerSet(true, alpha), expected);
            EXPECT_EQ(draw_.GetStats().batches, 1);
            EXPECT_EQ(draw_.GetStats().quads, 4320);
        }
    }
}

TEST_F(RadarDrawGlTest, CoincidentPlayerCrossfadeKeepsOpaqueMarkerBrightness)
{
    auto render = [this](float weight) {
        glClearColor(0, 0, 0, 0);
        glClear(GL_COLOR_BUFFER_BIT);
        draw_.BeginPrims();
        draw_.DrawPlayerMarkerFade({{64, 64}, {}, 1.0f, weight, false}, {{64, 64}, {}, 1.0f, 1.0f - weight, false}, 5, {255, 0, 0});
        draw_.EndPrims();
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
    auto render = [this](bool pair, float weight) {
        glClearColor(0, 0, 0, 0);
        glClear(GL_COLOR_BUFFER_BIT);
        draw_.ResetStats();
        draw_.BeginPrims();
        for (int player = 0; player < 32; player++)
        {
            ncl_math::Vector2 center{8.0f + (player % 8) * 16.0f, 16.0f + (player / 8) * 28.0f};
            ncl_math::Color color{player % 2 ? 255 : 80, 160, 220};
            if (pair)
            {
                draw_.DrawPlayerMarkerFade({center, {}, 1.0f, weight, false}, {center, {}, 1.0f, 1.0f - weight, false}, 3, color);
            }
            else
            {
                draw_.DrawDisc(center, 3, color);
            }
        }
        draw_.EndPrims();
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
        EXPECT_EQ(draw_.GetStats().batches, 1);
        EXPECT_EQ(draw_.GetStats().quads, 32);
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
    ASSERT_TRUE(draw_.DrawBlurBackground(64, 64, 32, true, 0.5f, 1.0f, 128, 128));

    glClearColor(1, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT);
    glScissor(64, 64, 32, 32);
    glEnable(GL_SCISSOR_TEST);
    ASSERT_TRUE(draw_.DrawBlurBackground(64, 64, 32, true, 0.5f, 1.0f, 128, 128));

    std::array<unsigned char, 4> clipped{};
    glReadPixels(80, 80, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, clipped.data());

    EXPECT_EQ(glIsEnabled(GL_SCISSOR_TEST), GL_TRUE);
    std::array<GLint, 4> scissor{};
    glGetIntegerv(GL_SCISSOR_BOX, scissor.data());
    EXPECT_EQ(scissor, (std::array<GLint, 4>{64, 64, 32, 32}));

    glDisable(GL_SCISSOR_TEST);
    glClear(GL_COLOR_BUFFER_BIT);
    ASSERT_TRUE(draw_.DrawBlurBackground(64, 64, 32, true, 0.5f, 1.0f, 128, 128));

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
            draw_.ResetGl(true);
            draw_.Prepare(RadarGlContext::Current(), nullptr);
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

                ASSERT_TRUE(draw_.DrawBlurBackground(64, 64, (float)size, true, 0.5f, 1.0f, 128, 128));

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

TEST_F(RadarDrawGlTest, IndependentOwnersFlushOnlyTheirOwnMonitorAndKeepShaderCache)
{
    RadarRecordingMonitor second_monitor;
    radar::integration::Performance second_performance{&second_monitor};
    RadarDrawContext second{second_performance};
    second.Prepare(RadarGlContext::Current(), nullptr);
    ASSERT_TRUE(second.CheckPrimsAvailable());
    monitor.gpu_begins.clear();
    monitor.counters.fill(0);
    RadarGlAllocations observe;

    draw_.BeginPrims();
    draw_.DrawDisc({32, 32}, 4, {255, 0, 0});
    second.BeginPrims();
    second.DrawDisc({64, 64}, 4, {0, 255, 0});
    EXPECT_TRUE(monitor.gpu_begins.empty());
    EXPECT_TRUE(second_monitor.gpu_begins.empty());
    second.EndFrame();
    EXPECT_TRUE(monitor.gpu_begins.empty());
    EXPECT_EQ(second_monitor.counters[uint32_t(performance::PerfCounterId::RadarBatches)], 1u);
    draw_.EndFrame();
    EXPECT_EQ(monitor.counters[uint32_t(performance::PerfCounterId::RadarBatches)], 1u);
    EXPECT_EQ(draw_.GetStats().quads, 1);
    EXPECT_EQ(second.GetStats().quads, 1);
    EXPECT_TRUE(observe.objects.programs.empty());
    EXPECT_EQ(observe.shader_creates, 0u);
    second.ResetGl(true);
}

TEST_F(RadarDrawGlTest, EndFrameClearsClipFadeRegionAndQueuedVertices)
{
    draw_.BeginPrims();
    draw_.SetPrimClip({0, 0}, 1, false);
    draw_.SetFade({0, 255, 0, 255});
    draw_.DrawDisc({64, 64}, 5, {255, 0, 0});
    draw_.EndFrame();
    int batches = draw_.GetStats().batches;
    draw_.EndFrame();
    EXPECT_EQ(draw_.GetStats().batches, batches);
    glClearColor(0, 0, 0, 0);
    glClear(GL_COLOR_BUFFER_BIT);
    draw_.BeginPrims();
    draw_.DrawDisc({64, 64}, 5, {255, 0, 0});
    draw_.EndPrims();
    std::array<unsigned char, 4> pixel{};
    glReadPixels(64, 64, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixel.data());
    EXPECT_EQ(pixel, (std::array<unsigned char, 4>{255, 0, 0, 255}));
}

TEST_F(RadarDrawGlTest, SameNativeLifetimeReleasesActualProgramsAndEffects)
{
    RadarDrawContext owner{performance_};
    owner.Prepare(RadarGlContext::Current(), nullptr);
    RadarGlObjects names;
    {
        RadarGlAllocations observe;
        ASSERT_TRUE(owner.CheckPrimsAvailable());
        ASSERT_TRUE(owner.DrawBlurBackground(0, 0, 32, true, 0, 1, 128, 128));
        names = observe.objects;
    }
    ASSERT_EQ(names.programs.size(), 3u);
    ASSERT_EQ(names.textures.size(), 3u);
    ASSERT_EQ(names.framebuffers.size(), 3u);
    for (GLuint program : names.programs)
    {
        EXPECT_EQ(glIsProgram(program), GL_TRUE);
    }
    for (GLuint texture : names.textures)
    {
        EXPECT_EQ(glIsTexture(texture), GL_TRUE);
    }
    owner.ResetGl(owner.is_context(RadarGlContext::Current(), nullptr));
    glUseProgram(0);
    for (GLuint program : names.programs)
    {
        EXPECT_EQ(glIsProgram(program), GL_FALSE);
    }
    for (GLuint texture : names.textures)
    {
        EXPECT_EQ(glIsTexture(texture), GL_FALSE);
    }
    for (GLuint framebuffer : names.framebuffers)
    {
        EXPECT_EQ(glIsFramebuffer(framebuffer), GL_FALSE);
    }
    EXPECT_EQ(glIsTexture(texture_), GL_TRUE);
    EXPECT_EQ(glIsFramebuffer(framebuffer_), GL_TRUE);
    EXPECT_FALSE(owner.CheckPrimsAvailable());
    owner.ResetGl(false);
    owner.Prepare(RadarGlContext::Current(), nullptr);
    ASSERT_TRUE(owner.CheckPrimsAvailable());
    owner.ResetGl(true);
}

TEST_F(RadarDrawGlTest, ForeignCurrentPreservesOverlappingLinkedProgramsAndImages)
{
    RadarDrawContext owner{performance_};
    owner.Prepare(RadarGlContext::Current(), nullptr);
    RadarGlObjects names;
    {
        RadarGlAllocations observe;
        ASSERT_TRUE(owner.CheckPrimsAvailable());
        ASSERT_TRUE(owner.DrawBlurBackground(0, 0, 32, true, 0, 1, 128, 128));
        names = observe.objects;
    }
    RadarGlContext foreign;
    std::string error;
    ASSERT_TRUE(foreign.Initialize(error)) << error;
    EXPECT_NE(RadarGlContext::Current(), context_.native_context());
    std::vector<GLuint> sentinels;
    GLuint maximum = *std::max_element(names.programs.begin(), names.programs.end());
    ASSERT_LT(maximum, 128u);
    while (sentinels.empty() || sentinels.back() < maximum)
    {
        sentinels.push_back(glCreateProgram());
        ASSERT_LT(sentinels.size(), 128u);
    }
    GLuint vertex = ncl_gl::CompileShader(GL_VERTEX_SHADER, "#version 120\nvoid main(){gl_Position=gl_Vertex;}");
    GLuint fragment = ncl_gl::CompileShader(GL_FRAGMENT_SHADER, "#version 120\nvoid main(){gl_FragColor=vec4(1);}");
    for (GLuint program : names.programs)
    {
        ASSERT_NE(std::find(sentinels.begin(), sentinels.end(), program), sentinels.end());
        glAttachShader(program, vertex);
        glAttachShader(program, fragment);
        glLinkProgram(program);
        GLint linked = 0;
        glGetProgramiv(program, GL_LINK_STATUS, &linked);
        ASSERT_EQ(linked, GL_TRUE);
    }
    std::vector<GLuint> framebuffer_sentinels;
    GLuint last_framebuffer = *std::max_element(names.framebuffers.begin(), names.framebuffers.end());
    ASSERT_LT(last_framebuffer, 128u);
    while (framebuffer_sentinels.empty() || framebuffer_sentinels.back() < last_framebuffer)
    {
        GLuint name = 0;
        glGenFramebuffers(1, &name);
        framebuffer_sentinels.push_back(name);
        ASSERT_LT(framebuffer_sentinels.size(), 128u);
    }
    const std::array<unsigned char, 4> color{19, 71, 143, 255};
    for (size_t i = 0; i < names.textures.size(); ++i)
    {
        glBindTexture(GL_TEXTURE_2D, names.textures[i]);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, color.data());
        ASSERT_NE(
            std::find(framebuffer_sentinels.begin(), framebuffer_sentinels.end(), names.framebuffers[i]), framebuffer_sentinels.end()
        );
        glBindFramebuffer(GL_FRAMEBUFFER, names.framebuffers[i]);
        ASSERT_EQ(glIsFramebuffer(names.framebuffers[i]), GL_TRUE);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, names.textures[i], 0);
        ASSERT_EQ(glCheckFramebufferStatus(GL_FRAMEBUFFER), GL_FRAMEBUFFER_COMPLETE);
    }
    ASSERT_EQ(glGetError(), GL_NO_ERROR);
    {
        RadarGlAllocations observe;
        owner.BeginPrims();
        owner.DrawDisc({10, 10}, 4, {255, 0, 0});
        owner.ResetGl(owner.is_context(RadarGlContext::Current(), nullptr));
        EXPECT_EQ(observe.delete_calls, 0u);
        EXPECT_EQ(observe.draws, 0u);
    }
    for (GLuint program : names.programs)
    {
        GLint linked = 0;
        EXPECT_EQ(glIsProgram(program), GL_TRUE);
        glGetProgramiv(program, GL_LINK_STATUS, &linked);
        EXPECT_EQ(linked, GL_TRUE);
    }
    for (size_t i = 0; i < names.textures.size(); ++i)
    {
        EXPECT_EQ(glIsTexture(names.textures[i]), GL_TRUE);
        EXPECT_EQ(glIsFramebuffer(names.framebuffers[i]), GL_TRUE);
        glBindFramebuffer(GL_FRAMEBUFFER, names.framebuffers[i]);
        EXPECT_EQ(glCheckFramebufferStatus(GL_FRAMEBUFFER), GL_FRAMEBUFFER_COMPLETE);
        std::array<unsigned char, 4> pixel{};
        glReadPixels(0, 0, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixel.data());
        EXPECT_EQ(pixel, color);
    }
    for (GLuint program : sentinels)
    {
        glDeleteProgram(program);
    }
    glDeleteShader(vertex);
    glDeleteShader(fragment);
    glDeleteTextures(static_cast<GLsizei>(names.textures.size()), names.textures.data());
    glDeleteFramebuffers(static_cast<GLsizei>(framebuffer_sentinels.size()), framebuffer_sentinels.data());
    ASSERT_TRUE(context_.MakeCurrent());
    names.Release();
}

TEST_F(RadarDrawGlTest, NoCurrentAndPassiveDestructionNeverDeleteOrFlush)
{
    RadarGlObjects names;
    auto owner = std::make_unique<RadarDrawContext>(performance_);
    owner->Prepare(RadarGlContext::Current(), nullptr);
    {
        RadarGlAllocations observe;
        ASSERT_TRUE(owner->CheckPrimsAvailable());
        names = observe.objects;
    }
    owner->BeginPrims();
    owner->DrawDisc({16, 16}, 4, {255, 0, 0});
    ASSERT_TRUE(context_.ClearCurrent());
    {
        RadarGlAllocations observe;
        owner->ResetGl(owner->is_context(RadarGlContext::Current(), nullptr));
        owner.reset();
        RadarDrawContext unprepared{performance_};
        EXPECT_EQ(observe.delete_calls, 0u);
        EXPECT_EQ(observe.draws, 0u);
    }
    ASSERT_TRUE(context_.MakeCurrent());
    for (GLuint program : names.programs)
    {
        EXPECT_EQ(glIsProgram(program), GL_TRUE);
    }
    names.Release();

    owner = std::make_unique<RadarDrawContext>(performance_);
    owner->Prepare(RadarGlContext::Current(), nullptr);
    {
        RadarGlAllocations observe;
        ASSERT_TRUE(owner->CheckPrimsAvailable());
        names = observe.objects;
        unsigned int deletes = observe.delete_calls;
        owner.reset();
        EXPECT_EQ(observe.delete_calls, deletes);
    }
    for (GLuint program : names.programs)
    {
        EXPECT_EQ(glIsProgram(program), GL_TRUE);
    }
    names.Release();
}
