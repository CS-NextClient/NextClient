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

    class GlyphAtlas
    {
    public:
        // Public so text_diag sizes its page-readback buffers from one source of truth.
        static constexpr int kPageSize = 512;
        static constexpr int kBytesPerTexel = 2; // GL_LUMINANCE_ALPHA

        void Configure(TextGlyphFormat format);

        // false means no page had room right now; the caller must treat it as transient.
        bool Add(int wide, int tall, const uint8_t* texels, AtlasCell* out_cell);

        void CellUv(const AtlasCell& cell, int wide, int tall, float* u0, float* v0, float* u1, float* v1) const;
        unsigned int PageTexture(int page) const;
        int PageCount() const { return (int)pages_.size(); }

        // Must run while the GL context is still current.
        void ReleaseGl();
        void ForgetGl();

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
