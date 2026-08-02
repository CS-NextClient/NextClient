#pragma once

#include <next_engine_mini/TextRenderInterface.h>

#include <cstdint>
#include <vector>

namespace text_render
{
    struct AtlasCell
    {
        int page;
        int x;
        int y;
    };

    // Shelf packer over a list of GL texture pages of one texel layout. Owns the textures; the
    // caller supplies cell pixels and gets back texture coordinates.
    class GlyphAtlas
    {
    public:
        // Page dimensions and texel layout, exposed so diagnostics that read pages
        // back (text_diag) size their buffers from this single source of truth
        // instead of duplicating the constants.
        static constexpr int kPageSize = 512;
        static constexpr int kBytesPerTexel = 2; // GL_LUMINANCE_ALPHA

        void Configure(TextGlyphFormat format);

        // Allocates a cell and uploads it. Returns false when no page had room and no further page
        // could be created, which the caller must treat as transient.
        bool Add(int wide, int tall, const uint8_t* texels, AtlasCell* out_cell);

        void CellUv(const AtlasCell& cell, int wide, int tall, float* u0, float* v0, float* u1, float* v1) const;
        unsigned int PageTexture(int page) const;
        int PageCount() const { return (int)pages_.size(); }

        // Releases every page. Must run while the GL context is still current.
        void ReleaseGl();

    private:
        struct Page
        {
            unsigned int texture;
            int next_x;
            int next_y;
            int row_tall;
        };

        bool CreatePage();
        bool Allocate(int wide, int tall, AtlasCell* out_cell);

        std::vector<Page> pages_;
        TextGlyphFormat format_ = TextGlyphFormat::Coverage;
    };
}
