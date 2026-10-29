#include "text_render.h"
#include "glyph_atlas.h"

#include <Windows.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <cwchar>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace
{
    constexpr int kDropShadowOffset = 1;

    constexpr int kMinTall = 6;
    constexpr int kMaxTall = 128;

    struct RasterGlyph
    {
        std::vector<uint8_t> coverage; // tight ink box, row major
        int wide;
        int tall;
        int bearing_x;
        int bearing_y; // from the top of the line box
        int advance;
        bool has_ink;
    };

    // Owns the DC so the font stays selected for metric queries.
    class FontFace
    {
    public:
        ~FontFace()
        {
            Release();
        }

        bool Create(const char* face, int tall, int weight, bool antialias, bool* out_substituted)
        {
            Release();

            dc_ = CreateCompatibleDC(nullptr);

            if (!dc_)
            {
                return false;
            }

            wchar_t wide_face[LF_FACESIZE] = {};
            MultiByteToWideChar(CP_UTF8, 0, face, -1, wide_face, LF_FACESIZE - 1);

            LOGFONTW lf;
            std::memset(&lf, 0, sizeof(lf));
            lf.lfHeight = tall;
            lf.lfWeight = weight;
            lf.lfCharSet = DEFAULT_CHARSET;
            lf.lfOutPrecision = OUT_TT_PRECIS;
            lf.lfClipPrecision = CLIP_DEFAULT_PRECIS;
            // Not CLEARTYPE: its subpixel coverage is already tinted and cannot take a caller colour.
            lf.lfQuality = antialias ? ANTIALIASED_QUALITY : NONANTIALIASED_QUALITY;
            lf.lfPitchAndFamily = DEFAULT_PITCH | FF_DONTCARE;
            std::wcsncpy(lf.lfFaceName, wide_face, LF_FACESIZE - 1);

            font_ = CreateFontIndirectW(&lf);

            if (!font_)
            {
                DeleteDC(dc_);
                dc_ = nullptr;
                return false;
            }

            SelectObject(dc_, font_);

            wchar_t got[LF_FACESIZE] = {};
            GetTextFaceW(dc_, LF_FACESIZE, got);
            *out_substituted = std::wcscmp(got, wide_face) != 0;

            TEXTMETRICW tm;
            GetTextMetricsW(dc_, &tm);
            ascent_ = tm.tmAscent;
            line_height_ = tm.tmHeight;
            antialias_ = antialias;
            requested_ = face;

            return true;
        }

        bool HasChar(uint32_t codepoint) const
        {
            if (codepoint > 0xFFFF)
            {
                return false;
            }

            wchar_t ch = (wchar_t)codepoint;
            WORD index = 0;

            if (GetGlyphIndicesW(dc_, &ch, 1, &index, GGI_MARK_NONEXISTING_GLYPHS) == GDI_ERROR)
            {
                return false;
            }

            return index != 0xFFFF;
        }

        // Matches the engine's two rasterizers: GGO_GRAY8_BITMAP with 65 coverage levels (the
        // surface's a = grayscale/64), and GGO_BITMAP, GDI's monochrome rasterizer with dropout
        // control. Thresholding the grey bitmap instead gives a different, un-hinted shape.
        bool Rasterize(uint32_t codepoint, RasterGlyph* out) const
        {
            UINT format = antialias_ ? GGO_GRAY8_BITMAP : GGO_BITMAP;

            GLYPHMETRICS gm;
            MAT2 mat = {{0, 1}, {0, 0}, {0, 0}, {0, 1}};
            DWORD size = GetGlyphOutlineW(dc_, (UINT)codepoint, format, &gm, 0, nullptr, &mat);

            if (size == GDI_ERROR)
            {
                return false;
            }

            out->advance = gm.gmCellIncX;
            out->bearing_x = gm.gmptGlyphOrigin.x;
            out->bearing_y = ascent_ - gm.gmptGlyphOrigin.y;
            out->wide = (int)gm.gmBlackBoxX;
            out->tall = (int)gm.gmBlackBoxY;
            out->has_ink = size != 0 && gm.gmBlackBoxX != 0 && gm.gmBlackBoxY != 0;

            if (!out->has_ink)
            {
                out->coverage.clear();
                out->wide = 0;
                out->tall = 0;
                return true;
            }

            std::vector<uint8_t> raw(size);

            if (GetGlyphOutlineW(dc_, (UINT)codepoint, format, &gm, size, raw.data(), &mat) == GDI_ERROR)
            {
                return false;
            }

            out->coverage.assign((size_t)out->wide * out->tall, 0);

            if (antialias_)
            {
                int pitch = (out->wide + 3) & ~3; // GGO_GRAY8 rows padded to four bytes

                for (int y = 0; y < out->tall; y++)
                {
                    for (int x = 0; x < out->wide; x++)
                    {
                        size_t src = (size_t)y * pitch + x;

                        if (src < raw.size())
                        {
                            float coverage = std::min(1.0f, raw[src] / 64.0f);
                            out->coverage[(size_t)y * out->wide + x] = (uint8_t)std::lround(255.0f * coverage);
                        }
                    }
                }
            }
            else
            {
                int pitch = ((out->wide + 31) / 32) * 4; // GGO_BITMAP rows padded to 32-bit words

                for (int y = 0; y < out->tall; y++)
                {
                    for (int x = 0; x < out->wide; x++)
                    {
                        size_t src = (size_t)y * pitch + (x / 8);

                        if (src < raw.size() && (raw[src] & (0x80 >> (x & 7))))
                        {
                            out->coverage[(size_t)y * out->wide + x] = 255;
                        }
                    }
                }
            }

            return true;
        }

        int ascent() const { return ascent_; }
        int line_height() const { return line_height_; }
        const std::string& requested() const { return requested_; }

    private:
        void Release()
        {
            // The DC goes first: a font selected into a DC cannot be deleted.
            if (dc_)
            {
                DeleteDC(dc_);
                dc_ = nullptr;
            }

            if (font_)
            {
                DeleteObject(font_);
                font_ = nullptr;
            }
        }

        HDC dc_ = nullptr;
        HFONT font_ = nullptr;
        int ascent_ = 0;
        int line_height_ = 0;
        bool antialias_ = false;
        std::string requested_; // family as asked for, not the substituted one
    };

    struct Font
    {
        std::vector<std::unique_ptr<FontFace>> faces;
        std::string face_name;
        int tall;
        int weight;
        int flags;
        TextGlyphFormat format;
        bool system_fallbacks_added = false;
        bool used = false; // set once a glyph resolved, which means the chain is complete
    };

    // GetGlyphOutlineW does no system font linking (the engine's ExtTextOut path does), so without
    // these a nickname's decorative glyph renders as .notdef. Order is widest-common first; Gadugi
    // covers Canadian Aboriginal Syllabics (U+1400..167F).
    constexpr const char* kSystemFallbackFaces[] = {
        "Segoe UI",
        "Segoe UI Symbol",
        "Segoe UI Historic",
        "Gadugi",
        "Nirmala UI",
        "Microsoft Sans Serif",
        "Malgun Gothic",
        "MS UI Gothic",
    };

    uint64_t GlyphKey(TextFont_t font, uint32_t codepoint)
    {
        return ((uint64_t)(uint32_t)font << 32) | codepoint;
    }

    class TextRender : public TextRenderInterface
    {
    public:
        TextFont_t FindOrCreateFont(const TextFontDesc& desc) override
        {
            if (!desc.face)
            {
                return kInvalidTextFont;
            }

            int tall = std::clamp(desc.tall, kMinTall, kMaxTall);

            for (size_t i = 1; i < fonts_.size(); i++)
            {
                const Font& font = fonts_[i];

                if (font.tall == tall && font.weight == desc.weight && font.flags == desc.flags &&
                    font.format == desc.format && font.face_name == desc.face)
                {
                    return (TextFont_t)i;
                }
            }

            Font font;
            font.face_name = desc.face;
            font.tall = tall;
            font.weight = desc.weight;
            font.flags = desc.flags;
            font.format = desc.format;

            fonts_.push_back(std::move(font));
            TextFont_t handle = (TextFont_t)fonts_.size() - 1;

            if (!AddFace(handle, desc.face))
            {
                fonts_.pop_back();
                return kInvalidTextFont;
            }

            EnsureAtlas(desc.format);

            return handle;
        }

        TextFont_t CreateEmptyFont() override
        {
            Font font;
            font.tall = 0;
            font.weight = 0;
            font.flags = kTextFontNone;
            font.format = TextGlyphFormat::Coverage;
            fonts_.push_back(std::move(font));

            return (TextFont_t)fonts_.size() - 1;
        }

        bool AddGlyphSet(TextFont_t font, const char* face, int tall, int weight, int flags) override
        {
            if (!Valid(font) || !face)
            {
                return false;
            }

            Font& target = fonts_[font];

            // A scheme reload re-adds the whole chain on the same handle; appending to the complete
            // old chain leaves a stale face ahead of the new size.
            if (target.used)
            {
                target.faces.clear();
                target.system_fallbacks_added = false;
                target.used = false;
                DropCells();
            }

            if (target.faces.empty())
            {
                target.face_name = face;
                target.tall = std::clamp(tall, kMinTall, kMaxTall);
                target.weight = weight;
                target.flags = flags;
                EnsureAtlas(target.format);
            }

            return AddFace(font, face);
        }

        void GetCharABCwide(TextFont_t font, uint32_t codepoint, int* a, int* b, int* c) override
        {
            *a = 0;
            *b = 0;
            *c = 0;

            const TextGlyph* glyph = GetGlyph(font, codepoint);

            if (!glyph)
            {
                return;
            }

            *a = glyph->bearing_x;
            *b = glyph->renderable ? glyph->wide : 0;
            *c = glyph->advance - glyph->bearing_x - *b;
        }

        int GetCharacterWidth(TextFont_t font, uint32_t codepoint) override
        {
            const TextGlyph* glyph = GetGlyph(font, codepoint);

            return glyph ? glyph->advance : 0;
        }

        void GetTextSize(TextFont_t font, const wchar_t* text, int* wide, int* tall) override
        {
            *wide = 0;
            *tall = GetLineHeight(font);

            if (!text)
            {
                return;
            }

            int line_wide = 0;

            for (const wchar_t* cursor = text; *cursor != L'\0'; cursor++)
            {
                if (*cursor == L'\n')
                {
                    *wide = std::max(*wide, line_wide);
                    line_wide = 0;
                    *tall += GetLineHeight(font);
                    continue;
                }

                line_wide += GetCharacterWidth(font, (uint32_t)*cursor);
            }

            *wide = std::max(*wide, line_wide);
        }

        int GetGlyphAscent(TextFont_t font, uint32_t codepoint) override
        {
            if (!Valid(font) || fonts_[font].faces.empty())
            {
                return 0;
            }

            return ResolveFace(fonts_[font], codepoint)->ascent();
        }

        bool AddFallbackFace(TextFont_t font, const char* face) override
        {
            if (!Valid(font) || !face)
            {
                return false;
            }

            return AddFace(font, face);
        }

        const TextGlyph* GetGlyph(TextFont_t font, uint32_t codepoint) override
        {
            if (!Valid(font) || fonts_[font].faces.empty())
            {
                return nullptr;
            }

            uint64_t key = GlyphKey(font, codepoint);
            auto it = cells_.find(key);

            if (it != cells_.end())
            {
                return &it->second;
            }

            TextGlyph glyph{};
            bool cacheable = Bake(font, codepoint, &glyph);

            if (!cacheable)
            {
                return nullptr;
            }

            return &cells_.emplace(key, glyph).first->second;
        }

        int GetLineHeight(TextFont_t font) override
        {
            return (Valid(font) && !fonts_[font].faces.empty()) ? fonts_[font].faces[0]->line_height() : 0;
        }

        int GetAscent(TextFont_t font) override
        {
            return (Valid(font) && !fonts_[font].faces.empty()) ? fonts_[font].faces[0]->ascent() : 0;
        }

        uint32_t Generation() override
        {
            return generation_;
        }

        bool GetFontDesc(TextFont_t font, char* face_out, int face_out_len, int* tall, int* weight, int* flags) override
        {
            if (!Valid(font))
            {
                return false;
            }

            const Font& f = fonts_[font];

            if (face_out && face_out_len > 0)
            {
                std::strncpy(face_out, f.face_name.c_str(), face_out_len - 1);
                face_out[face_out_len - 1] = '\0';
            }

            if (tall)
            {
                *tall = f.tall;
            }

            if (weight)
            {
                *weight = f.weight;
            }

            if (flags)
            {
                *flags = f.flags;
            }

            return true;
        }

        TextRenderStats Stats() const
        {
            TextRenderStats stats{};
            stats.fonts = (int)fonts_.size() - 1;
            stats.cells = (int)cells_.size();
            stats.generation = generation_;

            for (size_t i = 1; i < fonts_.size(); i++)
            {
                stats.faces += (int)fonts_[i].faces.size();
            }

            for (const auto& entry : atlases_)
            {
                stats.pages += entry.second.PageCount();
            }

            return stats;
        }

        TextFontInfo FontInfo(TextFont_t handle) const
        {
            TextFontInfo info{};
            info.face = "";

            if (!Valid(handle))
            {
                return info;
            }

            const Font& font = fonts_[handle];
            info.face = font.face_name.c_str();
            info.tall = font.tall;
            info.weight = font.weight;
            info.flags = font.flags;
            info.faces = (int)font.faces.size();

            for (const auto& cell : cells_)
            {
                if ((TextFont_t)(cell.first >> 32) == handle)
                {
                    info.cells++;
                }
            }

            return info;
        }

        unsigned int PageTexture(int page) const
        {
            for (const auto& entry : atlases_)
            {
                if (page < entry.second.PageCount())
                {
                    return entry.second.PageTexture(page);
                }

                page -= entry.second.PageCount();
            }

            return 0;
        }

        void ForgetGl()
        {
            for (auto& entry : atlases_)
            {
                entry.second.ForgetGl();
            }

            atlases_.clear();
            DropCells();
        }

        void ReleaseGl()
        {
            for (auto& entry : atlases_)
            {
                entry.second.ReleaseGl();
            }

            atlases_.clear();
            DropCells();
        }

    private:
        bool Valid(TextFont_t font) const
        {
            return font > 0 && (size_t)font < fonts_.size();
        }

        void DropCells()
        {
            cells_.clear();
            generation_++;
        }

        void EnsureAtlas(TextGlyphFormat format)
        {
            auto it = atlases_.find((int)format);

            if (it == atlases_.end())
            {
                it = atlases_.emplace((int)format, text_render::GlyphAtlas{}).first;
                it->second.Configure(format);
            }
        }

        bool AddFace(TextFont_t font, const char* face)
        {
            Font& target = fonts_[font];

            // Callers re-run their setup on the same memoized handle after a level change; appending
            // the same face again leaks a DC and an HFONT per repeat and lengthens every chain walk.
            for (const std::unique_ptr<FontFace>& existing : target.faces)
            {
                if (existing->requested() == face)
                {
                    return true;
                }
            }

            std::unique_ptr<FontFace> created = std::make_unique<FontFace>();
            bool substituted = false;

            bool antialias = (target.flags & kTextFontAntialias) != 0;

            if (!created->Create(face, target.tall, target.weight, antialias, &substituted))
            {
                return false;
            }

            // A substituted family is kept, not rejected: refusing it loses text entirely on a system
            // without the named face.
            target.faces.push_back(std::move(created));

            return true;
        }

        // Kept only when the request resolved to that exact family: a substituted family adds no new
        // coverage and would only lengthen the chain.
        void AddSystemFallback(Font& font, const char* face)
        {
            std::unique_ptr<FontFace> created = std::make_unique<FontFace>();
            bool substituted = false;
            bool antialias = (font.flags & kTextFontAntialias) != 0;

            if (created->Create(face, font.tall, font.weight, antialias, &substituted) && !substituted)
            {
                font.faces.push_back(std::move(created));
            }
        }

        const FontFace* ResolveFace(Font& font, uint32_t codepoint)
        {
            font.used = true;

            for (const std::unique_ptr<FontFace>& face : font.faces)
            {
                if (face->HasChar(codepoint))
                {
                    return face.get();
                }
            }

            if (!font.system_fallbacks_added)
            {
                font.system_fallbacks_added = true;

                for (const char* face : kSystemFallbackFaces)
                {
                    AddSystemFallback(font, face);
                }

                for (const std::unique_ptr<FontFace>& face : font.faces)
                {
                    if (face->HasChar(codepoint))
                    {
                        return face.get();
                    }
                }
            }

            return font.faces[0].get();
        }

        // False only for a transient failure, so the caller knows not to cache the miss.
        bool Bake(TextFont_t handle, uint32_t codepoint, TextGlyph* out)
        {
            Font& font = fonts_[handle];
            const FontFace* face = ResolveFace(font, codepoint);

            RasterGlyph raster{};

            if (!face->Rasterize(codepoint, &raster))
            {
                out->renderable = false;
                return true;
            }

            // Rasterize measures from the resolving face's baseline; a fallback with another ascent
            // must be shifted onto the primary's or its glyphs sit off the line.
            raster.bearing_y += font.faces[0]->ascent() - face->ascent();

            out->advance = raster.advance;

            if (!raster.has_ink)
            {
                out->renderable = false;
                return true;
            }

            bool shadow = (font.flags & kTextFontDropShadow) != 0;

            // The shadow extends the cell right and down; the fill sits at its top-left.
            int cell_wide = raster.wide + (shadow ? kDropShadowOffset : 0);
            int cell_tall = raster.tall + (shadow ? kDropShadowOffset : 0);

            std::vector<uint8_t> fill((size_t)cell_wide * cell_tall, 0);

            for (int y = 0; y < raster.tall; y++)
            {
                std::memcpy(
                    &fill[(size_t)y * cell_wide],
                    &raster.coverage[(size_t)y * raster.wide],
                    (size_t)raster.wide
                );
            }

            std::vector<uint8_t> texels(fill.size() * 2);

            if (shadow)
            {
                for (int y = 0; y < cell_tall; y++)
                {
                    for (int x = 0; x < cell_wide; x++)
                    {
                        size_t i = (size_t)y * cell_wide + x;
                        uint8_t f = fill[i];
                        uint8_t s = 0;

                        if (x >= kDropShadowOffset && y >= kDropShadowOffset)
                        {
                            s = fill[(size_t)(y - kDropShadowOffset) * cell_wide + (x - kDropShadowOffset)];
                        }

                        uint8_t alpha = std::max(f, s);
                        // GL_SRC_ALPHA applies coverage, so luminance must undo that factor for the fill.
                        texels[i * 2 + 0] = alpha ? static_cast<uint8_t>((255 * f + alpha / 2) / alpha) : 0;
                        texels[i * 2 + 1] = alpha;
                    }
                }
            }
            else
            {
                for (size_t i = 0; i < fill.size(); i++)
                {
                    texels[i * 2 + 0] = 255;
                    texels[i * 2 + 1] = fill[i];
                }
            }

            EnsureAtlas(font.format);
            text_render::GlyphAtlas& atlas = atlases_[(int)font.format];
            text_render::AtlasCell cell{};

            if (!atlas.Add(cell_wide, cell_tall, texels.data(), &cell))
            {
                return false;
            }

            atlas.CellUv(cell, cell_wide, cell_tall, &out->u0, &out->v0, &out->u1, &out->v1);
            out->texture = atlas.PageTexture(cell.page);
            out->wide = cell_wide;
            out->tall = cell_tall;
            out->bearing_x = raster.bearing_x;
            out->bearing_y = raster.bearing_y;
            out->renderable = true;

            return true;
        }

        // Index 0 is reserved as kInvalidTextFont.
        std::vector<Font> fonts_ = std::vector<Font>(1);
        std::unordered_map<int, text_render::GlyphAtlas> atlases_;
        std::unordered_map<uint64_t, TextGlyph> cells_;
        uint32_t generation_ = 1;
    };

    TextRender g_TextRender;
}

TextRenderInterface* TextRender_Get()
{
    return &g_TextRender;
}

TextRenderStats TextRender_Stats()
{
    return g_TextRender.Stats();
}

TextFontInfo TextRender_FontInfo(TextFont_t font)
{
    return g_TextRender.FontInfo(font);
}

unsigned int TextRender_PageTexture(int page)
{
    return g_TextRender.PageTexture(page);
}

void TextRender_ForgetGl()
{
    g_TextRender.ForgetGl();
}

void TextRender_ReleaseGl()
{
    g_TextRender.ReleaseGl();
}
