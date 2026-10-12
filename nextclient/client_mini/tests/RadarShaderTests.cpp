#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

#include <glad/glad.h>
#include <gtest/gtest.h>

#include "RadarGlContext.h"

namespace
{
    constexpr int kImageSize = 128;
    constexpr int kMapSize = 1024;

    struct EdgeCase
    {
        float shift;
        float coverage;
    };

    constexpr std::array<EdgeCase, 2> kEdges{{{-.3f, .75f}, {-.7f, .25f}}};

    std::string RadarShaderTest_ReadSource(const char* name)
    {
        std::string path = std::string(RADAR_SHADER_DIRECTORY) + "/" + name;
        std::ifstream stream(path);
        if (!stream)
        {
            ADD_FAILURE() << "Cannot read production shader: " << path;
            return {};
        }
        return std::string(std::istreambuf_iterator<char>(stream), {});
    }

    GLuint RadarShaderTest_Compile(GLenum kind, const std::string& source)
    {
        GLuint shader = glCreateShader(kind);
        const char* text = source.c_str();
        glShaderSource(shader, 1, &text, nullptr);
        glCompileShader(shader);
        GLint success = 0;
        glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
        if (!success)
        {
            std::array<char, 4096> log{};
            glGetShaderInfoLog(shader, static_cast<GLsizei>(log.size()), nullptr, log.data());
            ADD_FAILURE() << "Production shader compilation failed: " << log.data();
            glDeleteShader(shader);
            return 0;
        }
        return shader;
    }

    std::array<int, 4> RadarShaderTest_Pixel(const std::vector<unsigned char>& pixels, int x, int y)
    {
        std::array<int, 4> result{};
        for (int channel = 0; channel < 4; ++channel)
        {
            result[channel] = pixels[(y * kImageSize + x) * 4 + channel];
        }
        return result;
    }

    class RadarShaderTest : public testing::Test
    {
    private:
        RadarGlContext context_;
        bool context_ready_{};
        GLuint vertex_shader_{};
        GLuint fragment_shader_{};
        GLuint program_{};
        GLuint texture_{};
        GLuint output_texture_{};
        GLuint framebuffer_{};

    protected:
        void SetUp() override
        {
            std::string error;
            ASSERT_TRUE(context_.Initialize(error)) << error;
            context_ready_ = true;
            RecordProperty("renderer", reinterpret_cast<const char*>(glGetString(GL_RENDERER)));
            RecordProperty("version", reinterpret_cast<const char*>(glGetString(GL_VERSION)));

            std::string vertex_source = RadarShaderTest_ReadSource("radar.vert");
            std::string fragment_source = RadarShaderTest_ReadSource("radar_mask.frag");
            ASSERT_FALSE(vertex_source.empty());
            ASSERT_FALSE(fragment_source.empty());
            vertex_shader_ = RadarShaderTest_Compile(GL_VERTEX_SHADER, vertex_source);
            fragment_shader_ = RadarShaderTest_Compile(GL_FRAGMENT_SHADER, fragment_source);
            ASSERT_NE(vertex_shader_, 0u);
            ASSERT_NE(fragment_shader_, 0u);
            program_ = glCreateProgram();
            glAttachShader(program_, vertex_shader_);
            glAttachShader(program_, fragment_shader_);
            glLinkProgram(program_);
            GLint linked = 0;
            glGetProgramiv(program_, GL_LINK_STATUS, &linked);
            std::array<char, 4096> log{};
            glGetProgramInfoLog(program_, static_cast<GLsizei>(log.size()), nullptr, log.data());
            ASSERT_TRUE(linked) << "Production shader link failed: " << log.data();
            glUseProgram(program_);
            glUniform1i(Uniform("uTex"), 0);
            glUniform1f(Uniform("uMode"), 0);
            glUniform2f(Uniform("uCenter"), 64, 64);
            glUniform1f(Uniform("uRadius"), 100000);
            glUniform1f(Uniform("uFeather"), 1.5f);
            glUniform1f(Uniform("uSquare"), 0);
            glUniform1f(Uniform("uGamma"), 0);
            glUniform1f(Uniform("uPremultiplied"), 1);
            glUniform1f(Uniform("uFadeAmt"), 0);
            glActiveTexture(GL_TEXTURE0);
            for (GLenum capability :
                 {GL_ALPHA_TEST,
                  GL_BLEND,
                  GL_DEPTH_TEST,
                  GL_CULL_FACE,
                  GL_SCISSOR_TEST,
                  GL_LIGHTING,
                  GL_FOG,
                  GL_DITHER,
                  GL_FRAMEBUFFER_SRGB})
            {
                glDisable(capability);
            }
            glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
            glMatrixMode(GL_PROJECTION);
            glLoadIdentity();
            glOrtho(0, kImageSize, 0, kImageSize, -1, 1);
            glMatrixMode(GL_MODELVIEW);
            glLoadIdentity();
            glMatrixMode(GL_TEXTURE);
            glLoadIdentity();
            glGenTextures(1, &texture_);
            glBindTexture(GL_TEXTURE_2D, texture_);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            glGenTextures(1, &output_texture_);
            glBindTexture(GL_TEXTURE_2D, output_texture_);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, kImageSize, kImageSize, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
            glGenFramebuffers(1, &framebuffer_);
            glBindFramebuffer(GL_FRAMEBUFFER, framebuffer_);
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, output_texture_, 0);
            ASSERT_EQ(glCheckFramebufferStatus(GL_FRAMEBUFFER), GL_FRAMEBUFFER_COMPLETE);
            glViewport(0, 0, kImageSize, kImageSize);
            glBindTexture(GL_TEXTURE_2D, texture_);
            ASSERT_EQ(glGetError(), GL_NO_ERROR);
        }

        void TearDown() override
        {
            if (!context_ready_)
            {
                return;
            }
            glUseProgram(0);
            glDeleteProgram(program_);
            glDeleteShader(vertex_shader_);
            glDeleteShader(fragment_shader_);
            glDeleteTextures(1, &texture_);
            glDeleteTextures(1, &output_texture_);
            glDeleteFramebuffers(1, &framebuffer_);
        }

        GLint Uniform(const char* name)
        {
            GLint location = glGetUniformLocation(program_, name);
            EXPECT_GE(location, 0) << "Missing production uniform: " << name;
            return location;
        }

        void Upload(const std::vector<unsigned char>& pixels)
        {
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, kMapSize, kMapSize, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
            ASSERT_EQ(glGetError(), GL_NO_ERROR);
        }

        std::vector<unsigned char> Render(bool enabled, float shift, float extent = 128, float angle = 0, float opacity = 1)
        {
            glUniform1f(Uniform("uAlphaAa"), enabled ? 1.0f : 0.0f);
            glColor4f(opacity, opacity, opacity, opacity);
            glClearColor(0, 0, 0, 0);
            glClear(GL_COLOR_BUFFER_BIT);
            const std::array<float, 8> uv{0, 0, 1, 0, 1, 1, 0, 1};
            float radians = angle * 3.14159265358979323846f / 180;
            glBegin(GL_QUADS);
            for (int i = 0; i < 4; ++i)
            {
                float x = (uv[i * 2] - .5f) * extent;
                float y = (uv[i * 2 + 1] - .5f) * extent;
                glTexCoord2f(uv[i * 2], uv[i * 2 + 1]);
                glVertex2f(64 + shift + x * std::cos(radians) - y * std::sin(radians), 64 + x * std::sin(radians) + y * std::cos(radians));
            }
            glEnd();
            std::vector<unsigned char> pixels(kImageSize * kImageSize * 4);
            glReadPixels(0, 0, kImageSize, kImageSize, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
            EXPECT_EQ(glGetError(), GL_NO_ERROR);
            return pixels;
        }

        std::vector<unsigned char> Contour(bool diagonal = false)
        {
            std::vector<unsigned char> pixels(kMapSize * kMapSize * 4);
            for (int y = 0; y < kMapSize; ++y)
            {
                for (int x = 0; x < kMapSize; ++x)
                {
                    if (diagonal ? x + y < kMapSize : x < kMapSize / 2)
                    {
                        int index = (y * kMapSize + x) * 4;
                        pixels[index] = 51;
                        pixels[index + 1] = 102;
                        pixels[index + 2] = 153;
                        pixels[index + 3] = 255;
                    }
                }
            }
            return pixels;
        }
    };
} // namespace

TEST_F(RadarShaderTest, OpaqueDetailsStayIdenticalAtDifferentScalesAndRotations)
{
    std::vector<unsigned char> input(kMapSize * kMapSize * 4);
    for (int y = 0; y < kMapSize; ++y)
    {
        for (int x = 0; x < kMapSize; ++x)
        {
            int index = (y * kMapSize + x) * 4;
            bool stripe = x % 19 < 3;
            input[index] = stripe ? 153 : 51;
            input[index + 1] = stripe ? 26 : 102;
            input[index + 2] = stripe ? 51 : 153;
            input[index + 3] = 255;
        }
    }
    Upload(input);
    for (float extent : {96.0f, 128.0f, 224.0f})
    {
        for (float angle : {0.0f, 17.0f, 47.0f, 90.0f})
        {
            SCOPED_TRACE(testing::Message() << "extent=" << extent << " angle=" << angle);
            std::vector<unsigned char> original = Render(false, 0, extent, angle);
            EXPECT_EQ(Render(true, 0, extent, angle), original);
            EXPECT_EQ(RadarShaderTest_Pixel(original, 64, 64)[3], 255);
        }
    }
}

TEST_F(RadarShaderTest, SubpixelEdgesRetainCoverageAndPremultipliedColor)
{
    Upload(Contour());
    for (const EdgeCase& edge : kEdges)
    {
        for (float opacity : {1.0f, .6f})
        {
            SCOPED_TRACE(testing::Message() << "shift=" << edge.shift << " opacity=" << opacity);
            std::vector<unsigned char> pixels = Render(true, edge.shift, 128, 0, opacity);
            std::array<int, 4> pixel = RadarShaderTest_Pixel(pixels, 63, 64);
            float alpha = edge.coverage * opacity;
            EXPECT_NEAR(pixel[3], 255 * alpha, 1);
            EXPECT_NEAR(pixel[0], 51 * 1.15f * alpha, 2);
            EXPECT_NEAR(pixel[1], 102 * 1.15f * alpha, 2);
            EXPECT_NEAR(pixel[2], 153 * 1.15f * alpha, 2);
            EXPECT_EQ(RadarShaderTest_Pixel(pixels, 100, 64), (std::array<int, 4>{0, 0, 0, 0}));
            std::array<int, 4> unsmoothed = RadarShaderTest_Pixel(Render(false, edge.shift, 128, 0, opacity), 63, 64);
            EXPECT_GT(std::abs(unsmoothed[3] - pixel[3]), 20);
        }
    }
}

TEST_F(RadarShaderTest, FlashFadePreservesEdgeCoverage)
{
    Upload(Contour());
    glUniform3f(Uniform("uFadeColor"), 1, 0, 0);
    glUniform1f(Uniform("uFadeAmt"), 1);
    std::array<int, 4> pixel = RadarShaderTest_Pixel(Render(true, -.3f), 63, 64);
    EXPECT_NEAR(pixel[3], 191, 1);
    EXPECT_EQ(pixel[0], pixel[3]);
    EXPECT_EQ(pixel[1], 0);
    EXPECT_EQ(pixel[2], 0);
}

TEST_F(RadarShaderTest, DiagonalContourCoversHalfAPixel)
{
    Upload(Contour(true));
    std::array<int, 4> pixel = RadarShaderTest_Pixel(Render(true, 0), 63, 64);
    EXPECT_NEAR(pixel[3], 128, 8);
}

TEST_F(RadarShaderTest, RoundAndSquareMasksClipDifferentCorners)
{
    std::vector<unsigned char> input(kMapSize * kMapSize * 4, 255);
    Upload(input);
    glUniform1f(Uniform("uRadius"), 40);
    std::vector<unsigned char> round = Render(true, 0);
    EXPECT_EQ(RadarShaderTest_Pixel(round, 100, 100), (std::array<int, 4>{0, 0, 0, 0}));
    EXPECT_EQ(RadarShaderTest_Pixel(round, 64, 64)[3], 255);
    glUniform1f(Uniform("uSquare"), 1);
    std::vector<unsigned char> square = Render(true, 0);
    EXPECT_EQ(RadarShaderTest_Pixel(square, 100, 100)[3], 255);
    EXPECT_EQ(RadarShaderTest_Pixel(square, 110, 110), (std::array<int, 4>{0, 0, 0, 0}));
}

namespace
{
    class RadarPrimShaderTest : public testing::Test
    {
    private:
        RadarGlContext context_;
        bool context_ready_{};
        GLuint vertex_shader_{};
        GLuint fragment_shader_{};
        GLuint program_{};
        GLuint output_texture_{};
        GLuint framebuffer_{};

    protected:
        void SetUp() override
        {
            std::string error;
            ASSERT_TRUE(context_.Initialize(error)) << error;
            context_ready_ = true;
            RecordProperty("renderer", reinterpret_cast<const char*>(glGetString(GL_RENDERER)));
            vertex_shader_ = RadarShaderTest_Compile(GL_VERTEX_SHADER, RadarShaderTest_ReadSource("radar_prim.vert"));
            fragment_shader_ = RadarShaderTest_Compile(GL_FRAGMENT_SHADER, RadarShaderTest_ReadSource("radar_prim.frag"));
            ASSERT_NE(vertex_shader_, 0u);
            ASSERT_NE(fragment_shader_, 0u);
            program_ = glCreateProgram();
            glAttachShader(program_, vertex_shader_);
            glAttachShader(program_, fragment_shader_);
            glBindAttribLocation(program_, 0, "aPos");
            glLinkProgram(program_);
            GLint linked = 0;
            glGetProgramiv(program_, GL_LINK_STATUS, &linked);
            std::array<char, 4096> log{};
            glGetProgramInfoLog(program_, static_cast<GLsizei>(log.size()), nullptr, log.data());
            ASSERT_TRUE(linked) << log.data();
            glUseProgram(program_);
            glUniform2f(Uniform("uClipCenter"), 64, 64);
            glUniform1f(Uniform("uClipRadius"), 100000);
            glUniform1f(Uniform("uClipSquare"), 0);
            glUniform1f(Uniform("uFadeAmt"), 0);
            for (GLenum capability :
                 {GL_ALPHA_TEST,
                  GL_BLEND,
                  GL_DEPTH_TEST,
                  GL_CULL_FACE,
                  GL_SCISSOR_TEST,
                  GL_LIGHTING,
                  GL_FOG,
                  GL_DITHER,
                  GL_FRAMEBUFFER_SRGB})
            {
                glDisable(capability);
            }
            glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
            glMatrixMode(GL_PROJECTION);
            glLoadIdentity();
            glOrtho(0, kImageSize, kImageSize, 0, -1, 1);
            glMatrixMode(GL_MODELVIEW);
            glLoadIdentity();
            glGenTextures(1, &output_texture_);
            glBindTexture(GL_TEXTURE_2D, output_texture_);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, kImageSize, kImageSize, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
            glGenFramebuffers(1, &framebuffer_);
            glBindFramebuffer(GL_FRAMEBUFFER, framebuffer_);
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, output_texture_, 0);
            ASSERT_EQ(glCheckFramebufferStatus(GL_FRAMEBUFFER), GL_FRAMEBUFFER_COMPLETE);
            glViewport(0, 0, kImageSize, kImageSize);
            ASSERT_EQ(glGetError(), GL_NO_ERROR);
        }

        void TearDown() override
        {
            if (!context_ready_)
            {
                return;
            }
            glUseProgram(0);
            glDeleteProgram(program_);
            glDeleteShader(vertex_shader_);
            glDeleteShader(fragment_shader_);
            glDeleteTextures(1, &output_texture_);
            glDeleteFramebuffers(1, &framebuffer_);
        }

        GLint Uniform(const char* name)
        {
            GLint location = glGetUniformLocation(program_, name);
            EXPECT_GE(location, 0) << name;
            return location;
        }

        void Attribute(const char* name, float x, float y, float z, float w)
        {
            GLint location = glGetAttribLocation(program_, name);
            ASSERT_GE(location, 0) << name;
            glVertexAttrib4f(location, x, y, z, w);
        }

        std::vector<unsigned char> RenderSignal(float extent, float width, float shift = 0.0f, float opacity = 1.0f)
        {
            glClearColor(0, 0, 0, 0);
            glClear(GL_COLOR_BUFFER_BIT);
            Attribute("aShape", 9, .75f, extent, width);
            Attribute("aCenter", 64.5f + shift, 64.5f + shift, 0, 0);
            Attribute("aColor", 1, .8f, .4f, opacity);
            Attribute("aTri", 0, 0, 0, 0);
            Attribute("aTri2", 0, 0, 0, 0);
            glBegin(GL_QUADS);
            glVertex2f(32, 32);
            glVertex2f(96, 32);
            glVertex2f(96, 96);
            glVertex2f(32, 96);
            glEnd();
            std::vector<unsigned char> pixels(kImageSize * kImageSize * 4);
            glReadPixels(0, 0, kImageSize, kImageSize, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
            EXPECT_EQ(glGetError(), GL_NO_ERROR);
            const char* export_dir = std::getenv("NEXTCLIENT_RADAR_PRIM_EXPORT_DIR");
            if (export_dir && *export_dir)
            {
                std::filesystem::path path =
                    std::filesystem::path(export_dir) /
                    ("signal-" + std::to_string(extent) + "-" + std::to_string(shift) + "-" + std::to_string(opacity) + ".rgba");
                std::ofstream stream(path, std::ios::binary);
                EXPECT_TRUE(stream) << path;
                stream.write(reinterpret_cast<const char*>(pixels.data()), static_cast<std::streamsize>(pixels.size()));
                EXPECT_TRUE(stream);
            }
            return pixels;
        }

        int Alpha(const std::vector<unsigned char>& pixels, int x, int y)
        {
            return RadarShaderTest_Pixel(pixels, x, kImageSize - 1 - y)[3];
        }

        int PeakAlpha(const std::vector<unsigned char>& pixels, float x, float y)
        {
            int peak = 0;
            for (int dx = -1; dx <= 1; dx++)
            {
                for (int dy = -1; dy <= 1; dy++)
                {
                    peak = std::max(peak, Alpha(pixels, (int)x + dx, (int)y + dy));
                }
            }
            return peak;
        }
    };
} // namespace

TEST_F(RadarPrimShaderTest, DisconnectedSignalKeepsVisibleArcsDotAndSlashAtSmallSizes)
{
    for (float extent : {8.0f, 10.0f, 14.0f})
    {
        for (float shift : {0.0f, .3f, .7f})
        {
            SCOPED_TRACE(testing::Message() << "extent=" << extent << " shift=" << shift);
            std::vector<unsigned char> pixels = RenderSignal(extent, std::max(1.05f, extent * .095f), shift);
            float center = 64.5f + shift;
            for (const std::array<float, 2>& point : {std::array<float, 2>{-.7f, -.28f}, {.4f, .05f}, {0, .5f}, {.7f, -.75f}})
            {
                EXPECT_GT(PeakAlpha(pixels, center + point[0] * extent, center + point[1] * extent), 225);
            }
            EXPECT_EQ(Alpha(pixels, (int)(center + extent * .7f), (int)(center + extent * .6f)), 0);
            EXPECT_EQ(Alpha(pixels, (int)(center - extent * 1.4f), (int)center), 0);
        }
    }
}

TEST_F(RadarPrimShaderTest, DisconnectedSignalHasPartialCoverageAtNonAlignedEdges)
{
    for (float shift : {0.0f, .3f, .7f})
    {
        std::vector<unsigned char> pixels = RenderSignal(8, 1.05f, shift);
        int partial = 0;
        int solid = 0;
        for (int y = 48; y < 81; y++)
        {
            for (int x = 48; x < 81; x++)
            {
                int alpha = Alpha(pixels, x, y);
                partial += alpha > 20 && alpha < 220;
                solid += alpha > 225;
            }
        }
        EXPECT_GT(partial, 25);
        EXPECT_GT(solid, 20);
    }
}

TEST_F(RadarPrimShaderTest, DisconnectedSignalPreservesCoverageThroughOpacityAndFlash)
{
    std::vector<unsigned char> full = RenderSignal(8, 1.05f);
    glUniform3f(Uniform("uFadeColor"), 1, 0, 0);
    glUniform1f(Uniform("uFadeAmt"), 1);
    std::vector<unsigned char> faded = RenderSignal(8, 1.05f, 0, .5f);
    for (int y = 48; y < 81; y++)
    {
        for (int x = 48; x < 81; x++)
        {
            EXPECT_NEAR(Alpha(faded, x, y), Alpha(full, x, y) * .5f, 1);
            std::array<int, 4> pixel = RadarShaderTest_Pixel(faded, x, kImageSize - 1 - y);
            EXPECT_EQ(pixel[0], 255);
            EXPECT_EQ(pixel[1], 0);
            EXPECT_EQ(pixel[2], 0);
        }
    }
}
