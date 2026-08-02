#pragma once

#include <next_engine_mini/TextRenderInterface.h>

TextRenderInterface* TextRender_Get();

struct TextRenderStats
{
    int fonts;
    int faces;
    int cells;
    int pages;
    unsigned int generation;
};

struct TextFontInfo
{
    const char* face;
    int tall;
    int weight;
    int flags;
    int faces;
    int cells;
};

TextRenderStats TextRender_Stats();
TextFontInfo TextRender_FontInfo(TextFont_t font);
unsigned int TextRender_PageTexture(int page);

// Registers text_atlas_dump and text_service_info. Called from Draw_Init, where the console is up.
void TextRender_RegisterDiagnostics();

// Releases every atlas page. Must run while the GL context is still current, so it is driven from
// the Draw_Shutdown hook rather than from module teardown.
void TextRender_ReleaseGl();
