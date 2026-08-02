#include <cstdint>
#include <cstdio>
#include <vector>

#include "engine.h"

#include <glad/glad.h>

#include "text_render.h"
#include "text_surface.h"
#include "glyph_atlas.h"
#include "graphics/qgl.hpp"
#include "common/cmd.h"
#include "common/filesystem.h"
#include "console/console.h"

namespace
{
    constexpr int kAtlasSize = text_render::GlyphAtlas::kPageSize;

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

namespace
{
    // Counters are deltas since the previous read.
    void TextSurfaceInfo_f()
    {
        TextSurfaceStats stats = TextSurface_Stats();

        Con_Printf("text surface: %d print calls, %d char calls, last font %lu\n",
            stats.print_calls, stats.char_calls, stats.last_font);
        Con_Printf("  glyphs: %d asked, %d without a cell, %d clipped, %d quads\n",
            stats.glyphs_asked, stats.glyphs_missing, stats.glyphs_clipped, stats.quads);
        Con_Printf("  flushes: %d, of them %d with no gl table\n",
            stats.flushes, stats.flushes_without_gl);
        Con_Printf("  clip: %s [%d %d %d %d]\n",
            stats.clip_active ? "on" : "off", stats.clip[0], stats.clip[1], stats.clip[2], stats.clip[3]);

        TextSurface_ResetStats();
    }
}

void TextRender_RegisterDiagnostics()
{
    Cmd_AddCommand("text_atlas_dump", TextAtlasDump_f);
    Cmd_AddCommand("text_service_info", TextServiceInfo_f);
    Cmd_AddCommand("text_surface_info", TextSurfaceInfo_f);
}
