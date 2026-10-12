#include "glyph_atlas.h"

#include <algorithm>

#include <glad/glad.h>

#include "graphics/qgl.hpp"

namespace
{
    constexpr int kPageSize = text_render::GlyphAtlas::kPageSize;
    constexpr int kGutter = 1;
    constexpr int kMaxPages = 8;

    GLenum GlFormat(TextGlyphFormat format)
    {
        // Both layouts are two channel: plain coverage still needs a white luminance channel, or
        // GL_MODULATE would multiply the caller's colour by a zero RGB and draw black.
        (void)format;
        return GL_LUMINANCE_ALPHA;
    }

    int BytesPerTexel(TextGlyphFormat format)
    {
        (void)format;
        return text_render::GlyphAtlas::kBytesPerTexel;
    }
}

namespace text_render
{
    void GlyphAtlas::Configure(TextGlyphFormat format)
    {
        format_ = format;
    }

    bool GlyphAtlas::CreatePage()
    {
        if ((int)pages_.size() >= kMaxPages)
        {
            return false;
        }

        // The qgl table is only populated by QGL_Init, and the service is reachable from other
        // modules before that. Report a transient failure rather than calling through a null.
        if (!qglGenTextures)
        {
            return false;
        }

        GLenum format = GlFormat(format_);
        std::vector<uint8_t> zeroes((size_t)kPageSize * kPageSize * BytesPerTexel(format_), 0);

        GLint prev_tex = 0;
        qglGetIntegerv(GL_TEXTURE_BINDING_2D, &prev_tex);

        GLuint texture = 0;
        qglGenTextures(1, &texture);

        if (texture == 0)
        {
            return false;
        }

        // Point sampling matches the engine's font textures (created with hardwareFilter off), so a
        // crisp glyph stays crisp instead of being softened at the sampling stage.
        GLint filter = GL_NEAREST;

        qglBindTexture(GL_TEXTURE_2D, texture);
        qglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filter);
        qglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filter);
        qglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        qglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        qglTexImage2D(GL_TEXTURE_2D, 0, format, kPageSize, kPageSize, 0, format, GL_UNSIGNED_BYTE, zeroes.data());
        qglBindTexture(GL_TEXTURE_2D, (GLuint)prev_tex);

        Page page;
        page.texture = texture;
        page.next_x = 0;
        page.next_y = 0;
        page.row_tall = 0;
        pages_.push_back(page);

        return true;
    }

    // Cells fill a row left to right; a full row starts a new one below it.
    bool GlyphAtlas::Allocate(int wide, int tall, AtlasCell* out_cell)
    {
        int need_wide = wide + kGutter;
        int need_tall = tall + kGutter;

        if (need_wide > kPageSize || need_tall > kPageSize)
        {
            return false;
        }

        for (size_t i = 0; i < pages_.size(); i++)
        {
            Page& page = pages_[i];

            if (page.next_x + need_wide > kPageSize)
            {
                page.next_y += page.row_tall;
                page.next_x = 0;
                page.row_tall = 0;
            }

            if (page.next_y + need_tall > kPageSize)
            {
                continue;
            }

            out_cell->page = (int)i;
            out_cell->x = page.next_x;
            out_cell->y = page.next_y;
            page.next_x += need_wide;
            page.row_tall = std::max(page.row_tall, need_tall);

            return true;
        }

        if (!CreatePage())
        {
            return false;
        }

        Page& fresh = pages_.back();
        out_cell->page = (int)pages_.size() - 1;
        out_cell->x = 0;
        out_cell->y = 0;
        fresh.next_x = need_wide;
        fresh.row_tall = need_tall;

        return true;
    }

    bool GlyphAtlas::Add(int wide, int tall, const uint8_t* texels, AtlasCell* out_cell)
    {
        if (!Allocate(wide, tall, out_cell))
        {
            return false;
        }

        GLint prev_tex = 0;
        qglGetIntegerv(GL_TEXTURE_BINDING_2D, &prev_tex);
        qglBindTexture(GL_TEXTURE_2D, pages_[out_cell->page].texture);
        qglPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        qglTexSubImage2D(
            GL_TEXTURE_2D, 0, out_cell->x, out_cell->y, wide, tall,
            GlFormat(format_), GL_UNSIGNED_BYTE, texels
        );
        qglPixelStorei(GL_UNPACK_ALIGNMENT, 4);
        qglBindTexture(GL_TEXTURE_2D, (GLuint)prev_tex);

        return true;
    }

    void GlyphAtlas::CellUv(const AtlasCell& cell, int wide, int tall, float* u0, float* v0, float* u1, float* v1) const
    {
        *u0 = (float)cell.x / kPageSize;
        *v0 = (float)cell.y / kPageSize;
        *u1 = (float)(cell.x + wide) / kPageSize;
        *v1 = (float)(cell.y + tall) / kPageSize;
    }

    unsigned int GlyphAtlas::PageTexture(int page) const
    {
        return pages_[page].texture;
    }

    void GlyphAtlas::ReleaseGl()
    {
        for (const Page& page : pages_)
        {
            GLuint texture = page.texture;
            qglDeleteTextures(1, &texture);
        }

        pages_.clear();
    }
}
