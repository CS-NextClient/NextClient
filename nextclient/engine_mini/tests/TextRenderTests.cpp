#include <algorithm>
#include <cstdint>
#include <vector>

#include <gtest/gtest.h>

#include "graphics/qgl.hpp"
#include "graphics/text/text_render.h"

namespace
{
    std::vector<uint8_t> g_Upload;

    void APIENTRY GetInteger(GLenum, GLint* value)
    {
        *value = 0;
    }

    void APIENTRY GenTextures(GLsizei count, GLuint* textures)
    {
        std::fill_n(textures, count, 1u);
    }

    void APIENTRY BindTexture(GLenum, GLuint) {}
    void APIENTRY TextureParameter(GLenum, GLenum, GLint) {}
    void APIENTRY PixelStore(GLenum, GLint) {}
    void APIENTRY DeleteTextures(GLsizei, const GLuint*) {}
    void APIENTRY TextureImage(GLenum, GLint, GLint, GLsizei, GLsizei, GLint, GLenum, GLenum, const GLvoid*) {}

    void APIENTRY UploadGlyph(GLenum, GLint, GLint, GLint, GLsizei wide, GLsizei tall, GLenum format, GLenum type, const GLvoid* pixels)
    {
        ASSERT_EQ(format, GL_LUMINANCE_ALPHA);
        ASSERT_EQ(type, GL_UNSIGNED_BYTE);
        const uint8_t* texels = static_cast<const uint8_t*>(pixels);
        g_Upload.assign(texels, texels + wide * tall * 2);
    }
} // namespace

class TextRenderShadowTest : public testing::TestWithParam<int>
{
private:
    decltype(qglGetIntegerv) get_integer_ = qglGetIntegerv;
    decltype(qglGenTextures) gen_textures_ = qglGenTextures;
    decltype(qglBindTexture) bind_texture_ = qglBindTexture;
    decltype(qglTexParameteri) texture_parameter_ = qglTexParameteri;
    decltype(qglTexImage2D) texture_image_ = qglTexImage2D;
    decltype(qglTexSubImage2D) upload_glyph_ = qglTexSubImage2D;
    decltype(qglPixelStorei) pixel_store_ = qglPixelStorei;
    decltype(qglDeleteTextures) delete_textures_ = qglDeleteTextures;

protected:
    void SetUp() override
    {
        qglGetIntegerv = GetInteger;
        qglGenTextures = GenTextures;
        qglBindTexture = BindTexture;
        qglTexParameteri = TextureParameter;
        qglTexImage2D = TextureImage;
        qglTexSubImage2D = UploadGlyph;
        qglPixelStorei = PixelStore;
        qglDeleteTextures = DeleteTextures;
        TextRender_ReleaseGl();
        g_Upload.clear();
    }

    void TearDown() override
    {
        TextRender_ReleaseGl();
        qglGetIntegerv = get_integer_;
        qglGenTextures = gen_textures_;
        qglBindTexture = bind_texture_;
        qglTexParameteri = texture_parameter_;
        qglTexImage2D = texture_image_;
        qglTexSubImage2D = upload_glyph_;
        qglPixelStorei = pixel_store_;
        qglDeleteTextures = delete_textures_;
    }
};

TEST_P(TextRenderShadowTest, ShadowPreservesTheBlendedFillCoverage)
{
    TextRenderInterface* renderer = TextRender_Get();
    TextFontDesc desc{"Arial", 19, 400, GetParam(), TextGlyphFormat::Coverage};
    TextFont_t plain_font = renderer->FindOrCreateFont(desc);
    ASSERT_NE(plain_font, kInvalidTextFont);
    const TextGlyph* glyph = renderer->GetGlyph(plain_font, 'A');
    ASSERT_NE(glyph, nullptr);
    ASSERT_TRUE(glyph->renderable);
    TextGlyph plain = *glyph;
    std::vector<uint8_t> fill = g_Upload;
    ASSERT_EQ(fill.size(), plain.wide * plain.tall * 2u);

    desc.flags |= kTextFontDropShadow;
    TextFont_t shadow_font = renderer->FindOrCreateFont(desc);
    ASSERT_NE(shadow_font, kInvalidTextFont);
    glyph = renderer->GetGlyph(shadow_font, 'A');
    ASSERT_NE(glyph, nullptr);
    ASSERT_TRUE(glyph->renderable);
    ASSERT_EQ(glyph->wide, plain.wide + 1);
    ASSERT_EQ(glyph->tall, plain.tall + 1);
    ASSERT_EQ(g_Upload.size(), glyph->wide * glyph->tall * 2u);

    int partial_pixels = 0;
    int shadow_pixels = 0;
    for (int y = 0; y < glyph->tall; y++)
    {
        for (int x = 0; x < glyph->wide; x++)
        {
            size_t i = (static_cast<size_t>(y) * glyph->wide + x) * 2;
            int coverage = x < plain.wide && y < plain.tall ? fill[(y * plain.wide + x) * 2 + 1] : 0;
            int shadow = x > 0 && y > 0 ? fill[((y - 1) * plain.wide + x - 1) * 2 + 1] : 0;
            int luminance = g_Upload[i];
            int alpha = g_Upload[i + 1];

            EXPECT_EQ(alpha, std::max(coverage, shadow));
            // GL_SRC_ALPHA must apply the fill coverage once, including pixels covered by both masks.
            EXPECT_NEAR(luminance * alpha / 255.0, coverage, 0.5);
            partial_pixels += coverage > 0 && coverage < 255;
            if (coverage == 0 && shadow > 0)
            {
                EXPECT_EQ(luminance, 0);
                shadow_pixels++;
            }
        }
    }
    EXPECT_GT(shadow_pixels, 0);
    if (GetParam() & kTextFontAntialias)
    {
        EXPECT_GT(partial_pixels, 0);
    }
}

INSTANTIATE_TEST_SUITE_P(Coverage, TextRenderShadowTest, testing::Values(kTextFontNone, kTextFontAntialias));
