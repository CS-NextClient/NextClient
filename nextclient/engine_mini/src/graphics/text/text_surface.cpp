#include "text_surface.h"

#include <Windows.h>
#include <glad/glad.h>
#include <interface.h>
#include <optick.h>
#include <vgui/IPanel.h>

#include "text_render.h"
#include "graphics/qgl.hpp"

namespace
{
    constexpr int kMaxBatchQuads = 1024;

    // vgui2 ISurface.h font flags; these bits do not match the service's own flags.
    constexpr int kFontFlagAntialias = 0x010;
    constexpr int kFontFlagDropShadow = 0x080;

    struct BatchVertex
    {
        float x;
        float y;
        float u;
        float v;
        unsigned char rgba[4];
    };

    TextFont_t g_CurrentFont;
    int g_TextX;
    int g_TextY;
    int g_TextColor[4] = {255, 255, 255, 255};
    bool g_AlphaTestWasEnabled;
    bool g_BlendWasEnabled;
    GLint g_PrevBlendSrc = GL_ONE;
    GLint g_PrevBlendDst = GL_ZERO;

    BatchVertex g_Batch[kMaxBatchQuads * 4];
    int g_BatchVerts;
    unsigned int g_BatchTexture;

    // Panel-local space, the same the draw coordinates use. The engine keeps no stack: a pop drops
    // clipping entirely rather than restoring an outer rectangle, and that is reproduced here.
    bool g_ClipActive;
    int g_ClipLeft;
    int g_ClipTop;
    int g_ClipRight;
    int g_ClipBottom;

    // The qgl table carries only the SGIS selector; FlushBatch needs the ARB entry point instead.
    void (APIENTRY *g_SelectTextureUnit)(GLenum);

    TextSurfaceStats g_Stats;

    bool GlReady()
    {
        // The qgl table is populated by QGL_Init, and text can be asked for before that.
        if (!qglDrawArrays)
        {
            return false;
        }

        if (!g_SelectTextureUnit && qwglGetProcAddress)
        {
            g_SelectTextureUnit = reinterpret_cast<void (APIENTRY *)(GLenum)>(qwglGetProcAddress("glActiveTexture"));
        }

        return true;
    }

    void FlushBatch()
    {
        if (g_BatchVerts == 0)
        {
            return;
        }

        g_Stats.flushes++;

        if (!GlReady())
        {
            g_Stats.flushes_without_gl++;
            g_BatchVerts = 0;
            g_BatchTexture = 0;
            return;
        }

        // The engine's HUD/sprite pass can leave a non-zero unit active. Bind on unit 0, where the
        // fixed-function text draw samples and where hw.dll's GL_Bind cache tracks the binding, and
        // restore both, or the next sprite samples the atlas and draws garbled.
        GLint prev_unit = GL_TEXTURE0;

        if (g_SelectTextureUnit)
        {
            qglGetIntegerv(GL_ACTIVE_TEXTURE, &prev_unit);
            g_SelectTextureUnit(GL_TEXTURE0);
        }

        GLint prev_tex = 0;
        qglGetIntegerv(GL_TEXTURE_BINDING_2D, &prev_tex);
        qglBindTexture(GL_TEXTURE_2D, g_BatchTexture);

        // A colour-array draw leaves the current colour undefined, and the engine's number sprites
        // are tinted by glColor alone: leaving white here paints the round-timer digits white.
        GLfloat prev_color[4];
        qglGetFloatv(GL_CURRENT_COLOR, prev_color);

        qglEnableClientState(GL_VERTEX_ARRAY);
        qglEnableClientState(GL_TEXTURE_COORD_ARRAY);
        qglEnableClientState(GL_COLOR_ARRAY);
        qglVertexPointer(2, GL_FLOAT, sizeof(BatchVertex), &g_Batch[0].x);
        qglTexCoordPointer(2, GL_FLOAT, sizeof(BatchVertex), &g_Batch[0].u);
        qglColorPointer(4, GL_UNSIGNED_BYTE, sizeof(BatchVertex), &g_Batch[0].rgba);
        qglDrawArrays(GL_QUADS, 0, g_BatchVerts);
        qglDisableClientState(GL_COLOR_ARRAY);
        qglDisableClientState(GL_TEXTURE_COORD_ARRAY);
        qglDisableClientState(GL_VERTEX_ARRAY);
        qglColor4fv(prev_color);
        qglBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(prev_tex));

        if (g_SelectTextureUnit)
        {
            g_SelectTextureUnit(static_cast<GLenum>(prev_unit));
        }

        g_BatchVerts = 0;
        g_BatchTexture = 0;
    }

    bool ClipCell(int* x0, int* y0, int* x1, int* y1, float* s0, float* t0, float* s1, float* t1)
    {
        if (!g_ClipActive)
        {
            return true;
        }

        int cx0 = (*x0 > g_ClipLeft) ? *x0 : g_ClipLeft;
        int cy0 = (*y0 > g_ClipTop) ? *y0 : g_ClipTop;
        int cx1 = (*x1 < g_ClipRight) ? *x1 : g_ClipRight;
        int cy1 = (*y1 < g_ClipBottom) ? *y1 : g_ClipBottom;

        if (cx1 <= cx0 || cy1 <= cy0)
        {
            return false;
        }

        if (*x1 > *x0)
        {
            float u = (*s1 - *s0) / static_cast<float>(*x1 - *x0);
            float ns0 = *s0 + u * static_cast<float>(cx0 - *x0);
            float ns1 = *s0 + u * static_cast<float>(cx1 - *x0);
            *s0 = ns0;
            *s1 = ns1;
        }

        if (*y1 > *y0)
        {
            float v = (*t1 - *t0) / static_cast<float>(*y1 - *y0);
            float nt0 = *t0 + v * static_cast<float>(cy0 - *y0);
            float nt1 = *t0 + v * static_cast<float>(cy1 - *y0);
            *t0 = nt0;
            *t1 = nt1;
        }

        *x0 = cx0;
        *y0 = cy0;
        *x1 = cx1;
        *y1 = cy1;

        return true;
    }

    void AppendCell(const TextGlyph& e, int x, int y)
    {
        int x0 = x;
        int y0 = y;
        int x1 = x + e.wide;
        int y1 = y + e.tall;
        float s0 = e.u0;
        float t0 = e.v0;
        float s1 = e.u1;
        float t1 = e.v1;

        if (!ClipCell(&x0, &y0, &x1, &y1, &s0, &t0, &s1, &t1))
        {
            g_Stats.glyphs_clipped++;
            return;
        }

        g_Stats.quads++;

        if (g_BatchTexture != e.texture || g_BatchVerts + 4 > kMaxBatchQuads * 4)
        {
            FlushBatch();
            g_BatchTexture = e.texture;
        }

        BatchVertex* v = &g_Batch[g_BatchVerts];
        g_BatchVerts += 4;

        v[0].x = static_cast<float>(x0); v[0].y = static_cast<float>(y0); v[0].u = s0; v[0].v = t0;
        v[1].x = static_cast<float>(x1); v[1].y = static_cast<float>(y0); v[1].u = s1; v[1].v = t0;
        v[2].x = static_cast<float>(x1); v[2].y = static_cast<float>(y1); v[2].u = s1; v[2].v = t1;
        v[3].x = static_cast<float>(x0); v[3].y = static_cast<float>(y1); v[3].u = s0; v[3].v = t1;

        for (int i = 0; i < 4; i++)
        {
            v[i].rgba[0] = static_cast<unsigned char>(g_TextColor[0]);
            v[i].rgba[1] = static_cast<unsigned char>(g_TextColor[1]);
            v[i].rgba[2] = static_cast<unsigned char>(g_TextColor[2]);
            v[i].rgba[3] = static_cast<unsigned char>(g_TextColor[3]);
        }
    }

    void DrawGlyph(wchar_t ch)
    {
        TextRenderInterface* service = TextRender_Get();
        const TextGlyph* e = service ? service->GetGlyph(g_CurrentFont, static_cast<uint32_t>(ch)) : nullptr;

        g_Stats.glyphs_asked++;

        if (!e)
        {
            g_Stats.glyphs_missing++;
            return; // no cell yet: it resolves on a later draw
        }

        if (e->renderable && g_TextColor[3] != 0)
        {
            AppendCell(*e, g_TextX + e->bearing_x, g_TextY + e->bearing_y);
        }

        g_TextX += e->advance;
    }

    // The engine's HUD/2D pass leaves GL_ALPHA_TEST on, which clips the glyph's antialiased
    // coverage. glPushAttrib is not used to save the state: it is slow on modern drivers and would
    // run on every text draw.
    void BeginTextRun(bool additive)
    {
        if (!GlReady())
        {
            g_AlphaTestWasEnabled = false;
            return;
        }

        // The engine's HUD sprite path restores none of these, so any bit left perturbed here leaks
        // onto the HUD (a white flashing timer digit).
        g_AlphaTestWasEnabled = qglIsEnabled(GL_ALPHA_TEST) != GL_FALSE;
        g_BlendWasEnabled = qglIsEnabled(GL_BLEND) != GL_FALSE;
        qglGetIntegerv(GL_BLEND_SRC, &g_PrevBlendSrc);
        qglGetIntegerv(GL_BLEND_DST, &g_PrevBlendDst);

        qglEnable(GL_TEXTURE_2D);
        qglDisable(GL_ALPHA_TEST);
        qglEnable(GL_BLEND);
        qglBlendFunc(GL_SRC_ALPHA, additive ? GL_ONE : GL_ONE_MINUS_SRC_ALPHA);
        qglTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
    }

    // No batch may outlive the call that filled it, or it composites over a later draw.
    void EndTextRun()
    {
        FlushBatch();

        if (!qglBlendFunc)
        {
            return;
        }

        qglBlendFunc(g_PrevBlendSrc, g_PrevBlendDst);

        if (!g_BlendWasEnabled)
        {
            qglDisable(GL_BLEND);
        }

        if (g_AlphaTestWasEnabled)
        {
            qglEnable(GL_ALPHA_TEST);
        }
    }

    class TextSurface
    {
    public:

        unsigned long CreateEmptyFont()
        {
            TextRenderInterface* service = TextRender_Get();
            return service ? static_cast<unsigned long>(service->CreateEmptyFont()) : 0;
        }

        bool AddGlyphSetToFont(unsigned long font, const char* face, int tall, int weight, int flags)
        {
            TextRenderInterface* service = TextRender_Get();

            if (!service)
            {
                return false;
            }

            int text_flags = kTextFontNone;

            if (flags & kFontFlagDropShadow)
            {
                text_flags |= kTextFontDropShadow;
            }

            if (flags & kFontFlagAntialias)
            {
                text_flags |= kTextFontAntialias;
            }

            return service->AddGlyphSet(static_cast<TextFont_t>(font), face, tall, weight, text_flags);
        }

        int GetFontTall(unsigned long font)
        {
            TextRenderInterface* service = TextRender_Get();
            return service ? service->GetLineHeight(static_cast<TextFont_t>(font)) : 0;
        }

        void GetCharABCwide(unsigned long font, int ch, int* a, int* b, int* c)
        {
            TextRenderInterface* service = TextRender_Get();

            if (service)
            {
                service->GetCharABCwide(static_cast<TextFont_t>(font), static_cast<uint32_t>(ch), a, b, c);
            }
        }

        int GetCharacterWidth(unsigned long font, int ch)
        {
            TextRenderInterface* service = TextRender_Get();
            return service ? service->GetCharacterWidth(static_cast<TextFont_t>(font), static_cast<uint32_t>(ch)) : 0;
        }

        void GetTextSize(unsigned long font, const wchar_t* text, int* wide, int* tall)
        {
            TextRenderInterface* service = TextRender_Get();

            if (service)
            {
                service->GetTextSize(static_cast<TextFont_t>(font), text, wide, tall);
            }
        }

        int GetFontAscent(unsigned long font, wchar_t ch)
        {
            TextRenderInterface* service = TextRender_Get();
            return service ? service->GetGlyphAscent(static_cast<TextFont_t>(font), static_cast<uint32_t>(ch)) : 0;
        }

        void DrawSetTextFont(unsigned long font)
        {
            g_CurrentFont = static_cast<TextFont_t>(font);
            g_Stats.last_font = font;
        }

        void DrawSetTextColor(int r, int g, int b, int a)
        {
            g_TextColor[0] = r;
            g_TextColor[1] = g;
            g_TextColor[2] = b;
            g_TextColor[3] = a;
        }

        void DrawSetTextPos(int x, int y)
        {
            g_TextX = x;
            g_TextY = y;
        }

        void DrawGetTextPos(int* x, int* y)
        {
            *x = g_TextX;
            *y = g_TextY;
        }

        void DrawPrintText(const wchar_t* text, int text_len)
        {
            OPTICK_EVENT();
            g_Stats.print_calls++;

            if (!text)
            {
                return;
            }
            BeginTextRun(false);
            for (int i = 0; i < text_len; i++)
            {
                DrawGlyph(text[i]);
            }
            EndTextRun();
        }

        void DrawUnicodeChar(wchar_t ch)
        {
            g_Stats.char_calls++;
            BeginTextRun(false);
            DrawGlyph(ch);
            EndTextRun();
        }

        void DrawUnicodeCharAdd(wchar_t ch)
        {
            g_Stats.char_calls++;
            BeginTextRun(true);
            DrawGlyph(ch);
            EndTextRun();
        }

        void DrawFlushText()
        {
            // Not redundant: callers use it to separate two overlapping passes of the same text.
            FlushBatch();
        }

        void PushClip(int left, int top, int right, int bottom)
        {
            FlushBatch();

            g_ClipLeft = left;
            g_ClipTop = top;
            g_ClipRight = right;
            g_ClipBottom = bottom;
            g_ClipActive = true;
        }

        void PopClip()
        {
            FlushBatch();
            g_ClipActive = false;
        }
    };

    TextSurface g_TextSurface;

    // vgui2's Color as the slot takes it: four bytes by value, in r, g, b, a order.
    struct VguiColor
    {
        unsigned char rgba[4];
    };

    // Stock client.dll resolves VGUI_Surface026 itself and the engine draws through a pointer of its
    // own, so only replacing the shared object's slots reaches them. The slots are __thiscall;
    // __fastcall absorbs ECX=this, EDX=unused.
    unsigned long __fastcall Hook_CreateFont(void*, void*)
    {
        return g_TextSurface.CreateEmptyFont();
    }

    bool __fastcall Hook_AddGlyphSetToFont(void*, void*, unsigned long font, const char* name, int tall, int weight, int /*blur*/, int /*scanlines*/, int flags, int /*low*/, int /*high*/)
    {
        return g_TextSurface.AddGlyphSetToFont(font, name, tall, weight, flags);
    }

    int __fastcall Hook_GetFontTall(void*, void*, unsigned long font)
    {
        return g_TextSurface.GetFontTall(font);
    }

    void __fastcall Hook_GetCharABCwide(void*, void*, unsigned long font, int ch, int* a, int* b, int* c)
    {
        g_TextSurface.GetCharABCwide(font, ch, a, b, c);
    }

    int __fastcall Hook_GetCharacterWidth(void*, void*, unsigned long font, int ch)
    {
        return g_TextSurface.GetCharacterWidth(font, ch);
    }

    void __fastcall Hook_GetTextSize(void*, void*, unsigned long font, const wchar_t* text, int* wide, int* tall)
    {
        g_TextSurface.GetTextSize(font, text, wide, tall);
    }

    int __fastcall Hook_GetFontAscent(void*, void*, unsigned long font, wchar_t ch)
    {
        return g_TextSurface.GetFontAscent(font, ch);
    }

    void __fastcall Hook_DrawSetTextFont(void*, void*, unsigned long font)
    {
        g_TextSurface.DrawSetTextFont(font);
    }

    void __fastcall Hook_DrawSetTextColor_i(void*, void*, int r, int g, int b, int a)
    {
        g_TextSurface.DrawSetTextColor(r, g, b, a);
    }

    void __fastcall Hook_DrawSetTextColor_c(void*, void*, VguiColor col)
    {
        g_TextSurface.DrawSetTextColor(col.rgba[0], col.rgba[1], col.rgba[2], col.rgba[3]);
    }

    void __fastcall Hook_DrawSetTextPos(void*, void*, int x, int y)
    {
        g_TextSurface.DrawSetTextPos(x, y);
    }

    void __fastcall Hook_DrawGetTextPos(void*, void*, int* x, int* y)
    {
        g_TextSurface.DrawGetTextPos(x, y);
    }

    void __fastcall Hook_DrawPrintText(void*, void*, const wchar_t* text, int text_len)
    {
        g_TextSurface.DrawPrintText(text, text_len);
    }

    void __fastcall Hook_DrawUnicodeChar(void*, void*, wchar_t ch)
    {
        g_TextSurface.DrawUnicodeChar(ch);
    }

    void __fastcall Hook_DrawUnicodeCharAdd(void*, void*, wchar_t ch)
    {
        g_TextSurface.DrawUnicodeCharAdd(ch);
    }

    void __fastcall Hook_DrawFlushText(void*, void*)
    {
        g_TextSurface.DrawFlushText();
    }

    // The paint bracket must keep the engine's implementation: it sets the modelview and the scissor
    // everything else draws under.
    void (__fastcall *g_OrigPushMakeCurrent)(void*, void*, vgui2::VPANEL, bool);
    void (__fastcall *g_OrigPopMakeCurrent)(void*, void*, vgui2::VPANEL);

    vgui2::IPanel* Panels()
    {
        static vgui2::IPanel* panels = nullptr;

        if (!panels)
        {
            CreateInterfaceFn factory = Sys_GetFactory("vgui2.dll");

            if (factory)
            {
                panels = (vgui2::IPanel*)factory(VGUI_PANEL_INTERFACE_VERSION_GS, nullptr);
            }
        }

        return panels;
    }

    void __fastcall Hook_PushMakeCurrent(void* self, void* edx, vgui2::VPANEL panel, bool use_insets)
    {
        vgui2::IPanel* panels = Panels();

        if (panels)
        {
            int inset_left = 0;
            int inset_top = 0;
            int inset_right = 0;
            int inset_bottom = 0;

            if (use_insets)
            {
                panels->GetInset(panel, inset_left, inset_top, inset_right, inset_bottom);
            }

            int abs_x = 0;
            int abs_y = 0;
            panels->GetAbsPos(panel, abs_x, abs_y);

            int clip_x0 = 0;
            int clip_y0 = 0;
            int clip_x1 = 0;
            int clip_y1 = 0;
            panels->GetClipRect(panel, clip_x0, clip_y0, clip_x1, clip_y1);

            // The surface translates the modelview by (inset + absolute position); subtracting it
            // puts the clip rectangle in the panel-local space the glyph coordinates use.
            int origin_x = inset_left + abs_x;
            int origin_y = inset_top + abs_y;

            g_TextSurface.PushClip(clip_x0 - origin_x, clip_y0 - origin_y, clip_x1 - origin_x, clip_y1 - origin_y);
        }

        g_OrigPushMakeCurrent(self, edx, panel, use_insets);
    }

    void __fastcall Hook_PopMakeCurrent(void* self, void* edx, vgui2::VPANEL panel)
    {
        g_TextSurface.PopClip();
        g_OrigPopMakeCurrent(self, edx, panel);
    }

    void* PatchVtableEntry(void** vtable, int index, void* hook)
    {
        DWORD old_protect;
        void* previous = vtable[index];
        VirtualProtect(&vtable[index], sizeof(void*), PAGE_READWRITE, &old_protect);
        vtable[index] = hook;
        VirtualProtect(&vtable[index], sizeof(void*), old_protect, &old_protect);

        return previous;
    }

    bool g_Installed;

    struct PatchedSlot
    {
        int index;
        void* original;
    };

    void** g_SurfaceVtable;
    PatchedSlot g_Patched[32];
    int g_PatchedCount;

    void* Patch(void** vt, int index, void* hook)
    {
        void* previous = PatchVtableEntry(vt, index, hook);

        if (g_PatchedCount < (int)(sizeof(g_Patched) / sizeof(g_Patched[0])))
        {
            g_Patched[g_PatchedCount].index = index;
            g_Patched[g_PatchedCount].original = previous;
            g_PatchedCount++;
        }

        return previous;
    }

    void InstallOverSurface(void* surface)
    {
        // Patching twice would take the first trampoline for the original and chain into itself.
        if (!surface || g_Installed)
        {
            return;
        }

        g_Installed = true;

        void** vt = *reinterpret_cast<void***>(surface);
        g_SurfaceVtable = vt;

        // vtable indices from ISurface.h (VGUI_Surface026); slot 0 is the virtual destructor.
        g_OrigPushMakeCurrent = reinterpret_cast<void (__fastcall*)(void*, void*, vgui2::VPANEL, bool)>(
            Patch(vt, 5, reinterpret_cast<void*>(&Hook_PushMakeCurrent)));
        g_OrigPopMakeCurrent = reinterpret_cast<void (__fastcall*)(void*, void*, vgui2::VPANEL)>(
            Patch(vt, 6, reinterpret_cast<void*>(&Hook_PopMakeCurrent)));

        Patch(vt, 13, reinterpret_cast<void*>(&Hook_DrawSetTextFont));
        // MSVC lays overloaded virtuals out in reverse order: DrawSetTextColor(Color) at 14, (int...) at 15
        Patch(vt, 14, reinterpret_cast<void*>(&Hook_DrawSetTextColor_c));
        Patch(vt, 15, reinterpret_cast<void*>(&Hook_DrawSetTextColor_i));
        Patch(vt, 16, reinterpret_cast<void*>(&Hook_DrawSetTextPos));
        Patch(vt, 17, reinterpret_cast<void*>(&Hook_DrawGetTextPos));
        Patch(vt, 18, reinterpret_cast<void*>(&Hook_DrawPrintText));
        Patch(vt, 19, reinterpret_cast<void*>(&Hook_DrawUnicodeChar));
        Patch(vt, 20, reinterpret_cast<void*>(&Hook_DrawUnicodeCharAdd));
        Patch(vt, 21, reinterpret_cast<void*>(&Hook_DrawFlushText));
        Patch(vt, 59, reinterpret_cast<void*>(&Hook_CreateFont));
        Patch(vt, 60, reinterpret_cast<void*>(&Hook_AddGlyphSetToFont));
        Patch(vt, 62, reinterpret_cast<void*>(&Hook_GetFontTall));
        Patch(vt, 63, reinterpret_cast<void*>(&Hook_GetCharABCwide));
        Patch(vt, 64, reinterpret_cast<void*>(&Hook_GetCharacterWidth));
        Patch(vt, 65, reinterpret_cast<void*>(&Hook_GetTextSize));
        Patch(vt, 89, reinterpret_cast<void*>(&Hook_GetFontAscent));
    }
}

void TextSurface_Install(void* surface)
{
    InstallOverSurface(surface);
}

void TextSurface_Uninstall()
{
    if (!g_Installed || !g_SurfaceVtable)
    {
        return;
    }

    for (int i = g_PatchedCount - 1; i >= 0; i--)
    {
        PatchVtableEntry(g_SurfaceVtable, g_Patched[i].index, g_Patched[i].original);
    }

    g_PatchedCount = 0;
    g_SurfaceVtable = nullptr;
    g_OrigPushMakeCurrent = nullptr;
    g_OrigPopMakeCurrent = nullptr;
    g_Installed = false;
}

TextSurfaceStats TextSurface_Stats()
{
    TextSurfaceStats stats = g_Stats;
    stats.clip_active = g_ClipActive;
    stats.clip[0] = g_ClipLeft;
    stats.clip[1] = g_ClipTop;
    stats.clip[2] = g_ClipRight;
    stats.clip[3] = g_ClipBottom;

    return stats;
}

void TextSurface_ResetStats()
{
    g_Stats = TextSurfaceStats();
}
