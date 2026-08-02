#pragma once

// Must run before the first font is created.
void TextSurface_Install(void* surface);

// Must run before this module unloads: vgui2 outlives it, and a vtable entry pointing into the
// unmapped image faults on the next dispatch.
void TextSurface_Uninstall();

struct TextSurfaceStats
{
    int print_calls;
    int char_calls;
    int glyphs_asked;
    int glyphs_missing;
    int glyphs_clipped;
    int quads;
    int flushes;
    int flushes_without_gl;
    unsigned long last_font;
    bool clip_active;
    int clip[4];
};

TextSurfaceStats TextSurface_Stats();
void TextSurface_ResetStats();
