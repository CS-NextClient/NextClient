#include "../../engine.h"

#include "text_render.h"
#include "glyph_atlas.h"

#include <glad/glad.h>

#include "../qgl.hpp"
#include "../../common/cmd.h"
#include "../../common/filesystem.h"
#include "../../console/console.h"

#include <cstdint>
#include <cstdio>
#include <vector>

// Diagnostics for the glyph service. The atlas is the one place where a wrong glyph, a wrong cell
// or an overflowing page all look the same from the outside, so being able to look at it directly
// is what separates a measurement from a guess.

namespace
{
    constexpr int kAtlasSize = text_render::GlyphAtlas::kPageSize;

    // The pages are two channel (luminance, alpha). Both are written out: alpha as the visible
    // coverage, luminance beside it, so a drop-shadow cell can be told from a plain one at a glance.
    void WritePageTga(const char* path, const std::vector<uint8_t>& texels)
    {
        FILE* file = nullptr;

        if (fopen_s(&file, path, "wb") != 0 || !file)
        {
            Con_Printf("text_atlas_dump: cannot open %s\n", path);
            return;
        }

        int wide = kAtlasSize * 2;
        int tall = kAtlasSize;

        uint8_t header[18] = {};
        header[2] = 2; // uncompressed true colour
        header[12] = (uint8_t)(wide & 0xFF);
        header[13] = (uint8_t)((wide >> 8) & 0xFF);
        header[14] = (uint8_t)(tall & 0xFF);
        header[15] = (uint8_t)((tall >> 8) & 0xFF);
        header[16] = 24;
        header[17] = 0x20; // top-down
        fwrite(header, sizeof(header), 1, file);

        std::vector<uint8_t> row((size_t)wide * 3);

        for (int y = 0; y < tall; y++)
        {
            for (int x = 0; x < kAtlasSize; x++)
            {
                size_t src = ((size_t)y * kAtlasSize + x) * 2;
                uint8_t luminance = texels[src + 0];
                uint8_t alpha = texels[src + 1];

                // left half: coverage (alpha), right half: luminance
                row[(size_t)x * 3 + 0] = alpha;
                row[(size_t)x * 3 + 1] = alpha;
                row[(size_t)x * 3 + 2] = alpha;

                size_t right = (size_t)(kAtlasSize + x) * 3;
                row[right + 0] = luminance;
                row[right + 1] = luminance;
                row[right + 2] = luminance;
            }

            fwrite(row.data(), row.size(), 1, file);
        }

        fclose(file);
    }

    void TextAtlasDump_f()
    {
        if (!qglGetTexImage)
        {
            Con_Printf("text_atlas_dump: no gl context\n");
            return;
        }

        TextRenderStats stats = TextRender_Stats();

        if (stats.pages == 0)
        {
            Con_Printf("text_atlas_dump: no pages baked yet; draw some text first\n");
            return;
        }

        GLint prev_tex = 0;
        qglGetIntegerv(GL_TEXTURE_BINDING_2D, &prev_tex);

        std::vector<uint8_t> texels((size_t)kAtlasSize * kAtlasSize * text_render::GlyphAtlas::kBytesPerTexel);

        for (int i = 0; i < stats.pages; i++)
        {
            unsigned int texture = TextRender_PageTexture(i);

            if (texture == 0)
            {
                continue;
            }

            qglBindTexture(GL_TEXTURE_2D, texture);
            qglGetTexImage(GL_TEXTURE_2D, 0, GL_LUMINANCE_ALPHA, GL_UNSIGNED_BYTE, texels.data());

            char path[MAX_PATH];
            std::snprintf(path, sizeof(path), "text_atlas_%d.tga", i);
            WritePageTga(path, texels);
            Con_Printf("text_atlas_dump: wrote %s (left half coverage, right half luminance)\n", path);
        }

        qglBindTexture(GL_TEXTURE_2D, (GLuint)prev_tex);
    }

    void TextServiceInfo_f()
    {
        TextRenderStats stats = TextRender_Stats();

        Con_Printf("text service: %d fonts, %d faces, %d cells, %d pages, generation %u\n",
            stats.fonts, stats.faces, stats.cells, stats.pages, stats.generation);

        for (int i = 0; i < stats.fonts; i++)
        {
            TextFontInfo info = TextRender_FontInfo(i + 1);

            Con_Printf("  font %d: %s tall %d weight %d flags %d, %d faces, %d cells\n",
                i + 1, info.face, info.tall, info.weight, info.flags, info.faces, info.cells);
        }
    }
}

void TextRender_RegisterDiagnostics()
{
    Cmd_AddCommand("text_atlas_dump", TextAtlasDump_f);
    Cmd_AddCommand("text_service_info", TextServiceInfo_f);
}
