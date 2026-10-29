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

// Requires the context owning the atlas pages to be current.
void TextRender_ReleaseGl();
// Invalidates cached cells and page names without GL calls, preserving CPU fonts.
void TextRender_ForgetGl();
