#include <algorithm>
#include <cstdint>
#include <cwchar>
#include <memory>
#include <type_traits>
#include <vector>

#include <Windows.h>
#include <gtest/gtest.h>

#include "graphics/qgl.hpp"
#include "graphics/text/text_render.h"

namespace
{
    constexpr wchar_t kSimplifiedChineseGlyph = L'\u663E';
    constexpr int kChineseFontTall = 19;
    constexpr int kChineseFontWeight = 400;

    std::vector<uint8_t> g_Upload;
    uint32_t g_TextDeletions{};

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
    void APIENTRY DeleteTextures(GLsizei count, const GLuint*)
    {
        g_TextDeletions += count;
    }
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

TEST_P(TextRenderShadowTest, LossForgetsCellsWithoutDeletingAndPreservesFontsForLazyRecreation)
{
    TextRenderInterface* renderer = TextRender_Get();
    TextFontDesc desc{"Arial", 19, 400, GetParam(), TextGlyphFormat::Coverage};
    TextFont_t font = renderer->FindOrCreateFont(desc);
    ASSERT_NE(renderer->GetGlyph(font, 'A'), nullptr);
    TextRenderStats before = TextRender_Stats();
    ASSERT_GT(before.pages, 0);
    uint32_t deletions = g_TextDeletions;
    TextRender_ForgetGl();
    TextRenderStats lost = TextRender_Stats();
    EXPECT_EQ(lost.pages, 0);
    EXPECT_EQ(lost.cells, 0);
    EXPECT_EQ(lost.fonts, before.fonts);
    EXPECT_GT(lost.generation, before.generation);
    EXPECT_EQ(g_TextDeletions, deletions);
    TextRender_ReleaseGl();
    EXPECT_EQ(g_TextDeletions, deletions);
    ASSERT_NE(renderer->GetGlyph(font, 'A'), nullptr);
    EXPECT_GT(TextRender_Stats().pages, 0);
    TextRender_ReleaseGl();
    EXPECT_GT(g_TextDeletions, deletions);
}

TEST_P(TextRenderShadowTest, SimplifiedChineseFallbackMatchesNativeGlyphCoverageAndMetrics)
{
    LOGFONTW native_desc{};
    native_desc.lfHeight = kChineseFontTall;
    native_desc.lfWeight = kChineseFontWeight;
    native_desc.lfCharSet = DEFAULT_CHARSET;
    native_desc.lfOutPrecision = OUT_TT_PRECIS;
    native_desc.lfQuality = GetParam() & kTextFontAntialias ? ANTIALIASED_QUALITY : NONANTIALIASED_QUALITY;
    std::wcsncpy(native_desc.lfFaceName, L"Microsoft YaHei", LF_FACESIZE - 1);

    std::unique_ptr<std::remove_pointer_t<HFONT>, decltype(&DeleteObject)> native_font(CreateFontIndirectW(&native_desc), DeleteObject);
    ASSERT_NE(native_font, nullptr);
    std::unique_ptr<std::remove_pointer_t<HDC>, decltype(&DeleteDC)> dc(CreateCompatibleDC(nullptr), DeleteDC);
    ASSERT_NE(dc, nullptr);
    ASSERT_NE(SelectObject(dc.get(), native_font.get()), nullptr);

    wchar_t actual_face[LF_FACESIZE]{};
    ASSERT_GT(GetTextFaceW(dc.get(), LF_FACESIZE, actual_face), 0);
    if (std::wcscmp(actual_face, L"Microsoft YaHei") != 0)
    {
        GTEST_SKIP() << "Microsoft YaHei is not installed";
    }

    WORD glyph_index = 0;
    ASSERT_NE(GetGlyphIndicesW(dc.get(), &kSimplifiedChineseGlyph, 1, &glyph_index, GGI_MARK_NONEXISTING_GLYPHS), GDI_ERROR);
    ASSERT_NE(glyph_index, 0xFFFF);

    TEXTMETRICW native_metrics{};
    ASSERT_TRUE(GetTextMetricsW(dc.get(), &native_metrics));
    UINT format = GetParam() & kTextFontAntialias ? GGO_GRAY8_BITMAP : GGO_BITMAP;
    MAT2 identity = {{0, 1}, {0, 0}, {0, 0}, {0, 1}};
    GLYPHMETRICS metrics{};
    DWORD size = GetGlyphOutlineW(dc.get(), kSimplifiedChineseGlyph, format, &metrics, 0, nullptr, &identity);
    ASSERT_NE(size, GDI_ERROR);
    ASSERT_GT(size, 0u);
    std::vector<uint8_t> native_bitmap(size);
    ASSERT_EQ(GetGlyphOutlineW(dc.get(), kSimplifiedChineseGlyph, format, &metrics, size, native_bitmap.data(), &identity), size);

    TextRenderInterface* renderer = TextRender_Get();
    TextFontDesc desc{"Tahoma", kChineseFontTall, kChineseFontWeight, GetParam(), TextGlyphFormat::Coverage};
    TextFont_t font = renderer->FindOrCreateFont(desc);
    ASSERT_NE(font, kInvalidTextFont);
    const TextGlyph* glyph = renderer->GetGlyph(font, kSimplifiedChineseGlyph);
    ASSERT_NE(glyph, nullptr);
    ASSERT_TRUE(glyph->renderable);

    EXPECT_EQ(glyph->wide, metrics.gmBlackBoxX);
    EXPECT_EQ(glyph->tall, metrics.gmBlackBoxY);
    EXPECT_EQ(glyph->bearing_x, metrics.gmptGlyphOrigin.x);
    EXPECT_EQ(glyph->bearing_y, renderer->GetAscent(font) - metrics.gmptGlyphOrigin.y);
    EXPECT_EQ(glyph->advance, metrics.gmCellIncX);
    EXPECT_EQ(renderer->GetGlyphAscent(font, kSimplifiedChineseGlyph), native_metrics.tmAscent);
    ASSERT_EQ(g_Upload.size(), metrics.gmBlackBoxX * metrics.gmBlackBoxY * 2u);

    size_t pitch = format == GGO_GRAY8_BITMAP ? (metrics.gmBlackBoxX + 3u) & ~3u : ((metrics.gmBlackBoxX + 31u) / 32u) * 4u;
    for (size_t y = 0; y < metrics.gmBlackBoxY; y++)
    {
        for (size_t x = 0; x < metrics.gmBlackBoxX; x++)
        {
            int coverage = format == GGO_GRAY8_BITMAP ? (std::min<int>(native_bitmap[y * pitch + x], 64) * 255 + 32) / 64
                                                      : (native_bitmap[y * pitch + x / 8] & (0x80 >> (x % 8)) ? 255 : 0);
            size_t pixel = (y * metrics.gmBlackBoxX + x) * 2;
            EXPECT_EQ(g_Upload[pixel], 255);
            EXPECT_EQ(g_Upload[pixel + 1], coverage);
        }
    }
}
