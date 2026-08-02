#include "RadarHooks.h"
#include "RadarBroadcast.h"
#include "HudTextZone.h"
#include "SmokeRegistry.h"
#include "../../main.h"

#include <next_client_mini/client_mini.h>
#include <parsemsg.h>

#include <cstdint>
#include <unordered_map>
#include <vector>

namespace
{
    pfnEngSrc_pfnDrawConsoleString_t g_OrigDrawConsoleString;
    pfnEngSrc_pfnDrawConsoleStringLen_t g_OrigDrawConsoleStringLen;
    pfnEngSrc_pfnDrawString_t g_OrigDrawString;
    pfnEngSrc_pfnDrawCharacter_t g_OrigDrawCharacter;
    pfnEngSrc_pfnVGUI2DrawCharacter_t g_OrigVGUI2DrawCharacter;
    pfnEngSrc_pfnVGUI2DrawCharacterAdd_t g_OrigVGUI2DrawCharacterAdd;
    pfnEngSrc_pfnDrawSetTextColor_t g_OrigDrawSetTextColor;
    cl_enginefunc_t* g_PatchedEngTable;

    // VGUI2DrawCharacter draws with the engine's ambient text colour (set via
    // DrawSetTextColor), which is not an argument. Track it here so a buffered glyph
    // can be replayed in its own colour instead of whatever colour is current when the
    // deferred line is flushed (the next line's, mid-frame).
    int g_TextR = 255, g_TextG = 255, g_TextB = 255;

    int TextWidth(const char* string)
    {
        int width = 0, height = 0;

        if (g_OrigDrawConsoleStringLen)
        {
            g_OrigDrawConsoleStringLen(string, &width, &height);
        }

        return width;
    }

    // Glyph text (DrawCharacter / VGUI2DrawCharacter) arrives one glyph at a time, so a
    // line's full width is only known after its last glyph. An in-zone line is buffered
    // here and drawn once, shifted as one line, when the next line starts or the frame
    // ends (RadarHooks_FlushText). The advance each hook must return is measured by
    // drawing the glyph off-screen (the engine's returned width is position-independent),
    // so the caller's x-cursor keeps stepping while the visible draw is deferred.
    constexpr int kOffscreen = -10000;

    enum GlyphKind
    {
        kGlyphConsole,
        kGlyphVgui2,
        kGlyphVgui2Add,
    };

    struct BufGlyph
    {
        GlyphKind kind;
        int x, y, ch;
        int r, g, b;
        unsigned int font;
    };

    std::vector<BufGlyph> g_GlyphLine;
    int g_LineY;
    int g_LineFirstX;
    int g_LineLastX;
    int g_LineRight;

    // Glyph advance is a pure function of (kind, char, font) that only changes when the
    // fonts are rebuilt (video mode / UI scale), so cache it instead of drawing the glyph
    // off-screen every frame just to read the returned width. Cleared on VidInit.
    std::unordered_map<std::uint64_t, int> g_GlyphWidth;

    template <class Measure>
    int CachedGlyphWidth(GlyphKind kind, int ch, unsigned int font, Measure measure)
    {
        std::uint64_t key = ((std::uint64_t)font << 32) | ((std::uint64_t)(std::uint32_t)ch << 8) | (std::uint32_t)kind;
        auto it = g_GlyphWidth.find(key);

        if (it != g_GlyphWidth.end())
        {
            return it->second;
        }

        int width = measure();
        g_GlyphWidth.emplace(key, width);
        return width;
    }

    void FlushGlyphLine()
    {
        if (g_GlyphLine.empty())
        {
            return;
        }

        int dx = 0, dy = 0;
        HudTextZone::LineShift(g_LineFirstX, g_LineY, g_LineRight - g_LineFirstX, &dx, &dy);

        for (const BufGlyph& g : g_GlyphLine)
        {
            int gx = g.x + dx, gy = g.y + dy;

            switch (g.kind)
            {
                case kGlyphConsole:
                    g_OrigDrawCharacter(gx, gy, g.ch, g.r, g.g, g.b);
                    break;
                case kGlyphVgui2:
                    // Restore the ambient colour this glyph was drawn under before
                    // replaying it (the plain VGUI2 draw takes no colour argument).
                    if (g_OrigDrawSetTextColor)
                    {
                        g_OrigDrawSetTextColor(g.r / 255.0f, g.g / 255.0f, g.b / 255.0f);
                    }

                    g_OrigVGUI2DrawCharacter(gx, gy, g.ch, g.font);
                    break;
                case kGlyphVgui2Add:
                    g_OrigVGUI2DrawCharacterAdd(gx, gy, g.ch, g.r, g.g, g.b, g.font);
                    break;
            }
        }

        // Leave the ambient colour as the caller last set it, so the line that
        // triggered a mid-frame flush keeps drawing in its own colour.
        if (g_OrigDrawSetTextColor)
        {
            g_OrigDrawSetTextColor(g_TextR / 255.0f, g_TextG / 255.0f, g_TextB / 255.0f);
        }

        g_GlyphLine.clear();
    }

    void BufferGlyph(GlyphKind kind, int x, int y, int ch, int r, int g, int b, unsigned int font, int width)
    {
        // A different row or an x that steps back starts a new line; flush the one in
        // progress first so lines never merge.
        if (!g_GlyphLine.empty() && (y != g_LineY || x < g_LineLastX))
        {
            FlushGlyphLine();
        }

        if (g_GlyphLine.empty())
        {
            g_LineY = y;
            g_LineFirstX = x;
            g_LineRight = x;
        }

        g_LineLastX = x;
        g_GlyphLine.push_back({kind, x, y, ch, r, g, b, font});

        if (x + width > g_LineRight)
        {
            g_LineRight = x + width;
        }
    }

    // The width measurement is a full pass over the string, so it is skipped for rows the
    // zone can never shift (and entirely while the zone is inactive).
    int Hooked_DrawConsoleString(int x, int y, char* string)
    {
        HudTextZone::TrackText(&x, &y, HudTextZone::RowInZone(y) ? TextWidth(string) : 0);
        return g_OrigDrawConsoleString(x, y, string);
    }

    int Hooked_DrawString(int x, int y, const char* string, int r, int g, int b)
    {
        HudTextZone::TrackText(&x, &y, HudTextZone::RowInZone(y) ? TextWidth(string) : 0);
        return g_OrigDrawString(x, y, string, r, g, b);
    }

    int Hooked_DrawCharacter(int x, int y, int number, int r, int g, int b)
    {
        if (!HudTextZone::RowInZone(y))
        {
            return g_OrigDrawCharacter(x, y, number, r, g, b);
        }

        int width = CachedGlyphWidth(kGlyphConsole, number, 0, [&] {
            return g_OrigDrawCharacter(kOffscreen, kOffscreen, number, r, g, b);
        });
        BufferGlyph(kGlyphConsole, x, y, number, r, g, b, 0, width);
        return width;
    }

    int Hooked_VGUI2DrawCharacter(int x, int y, int ch, unsigned int font)
    {
        if (!HudTextZone::RowInZone(y))
        {
            return g_OrigVGUI2DrawCharacter(x, y, ch, font);
        }

        int width = CachedGlyphWidth(kGlyphVgui2, ch, font, [&] {
            return g_OrigVGUI2DrawCharacter(kOffscreen, kOffscreen, ch, font);
        });
        BufferGlyph(kGlyphVgui2, x, y, ch, g_TextR, g_TextG, g_TextB, font, width);
        return width;
    }

    void Hooked_DrawSetTextColor(float r, float g, float b)
    {
        g_TextR = (int)(r * 255.0f + 0.5f);
        g_TextG = (int)(g * 255.0f + 0.5f);
        g_TextB = (int)(b * 255.0f + 0.5f);
        g_OrigDrawSetTextColor(r, g, b);
    }

    int Hooked_VGUI2DrawCharacterAdd(int x, int y, int ch, int r, int g, int b, unsigned int font)
    {
        if (!HudTextZone::RowInZone(y))
        {
            return g_OrigVGUI2DrawCharacterAdd(x, y, ch, r, g, b, font);
        }

        int width = CachedGlyphWidth(kGlyphVgui2Add, ch, font, [&] {
            return g_OrigVGUI2DrawCharacterAdd(kOffscreen, kOffscreen, ch, r, g, b, font);
        });
        BufferGlyph(kGlyphVgui2Add, x, y, ch, r, g, b, font, width);
        return width;
    }

    void CL_HookEventHandler(char* name, void (*cb)(event_args_t*), CL_HookEventNext next)
    {
        // Route the createsmoke event through the radar (exact smoke centre), chaining
        // to the original handler so the smoke particles still render.
        next->Invoke(name, RadarSmoke::WrapEventHook(name, cb));
    }

    int UserMsg_BombDropHandler(const char* name, int size, void* data, UserMsg_RadarMsgNext next)
    {
        BEGIN_READ(data, size);
        float x = READ_COORD();
        float y = READ_COORD();
        float z = READ_COORD();
        int planted = READ_BYTE();

        ncl_math::Vector3 pos{x, y, z};
        float time = gEngfuncs.GetClientTime();

        if (pos == ncl_math::Vector3{})
        {
            RadarBroadcast::ClearBomb(time); // engine "hide" sentinel
        }
        else
        {
            RadarBroadcast::SetBomb(pos, planted != 0, time);
        }

        return next->Invoke(name, size, data);
    }

    int UserMsg_BombPickupHandler(const char* name, int size, void* data, UserMsg_RadarMsgNext next)
    {
        RadarBroadcast::ClearBomb(gEngfuncs.GetClientTime());
        return next->Invoke(name, size, data);
    }

    int UserMsg_HostagePosHandler(const char* name, int size, void* data, UserMsg_RadarMsgNext next)
    {
        BEGIN_READ(data, size);
        READ_BYTE(); // leading flag byte (always 0)
        int index = READ_BYTE();
        float x = READ_COORD();
        float y = READ_COORD();
        READ_COORD(); // z, unused on the 2D radar

        RadarBroadcast::SetHostage(index, {x, y}, gEngfuncs.GetClientTime());
        return next->Invoke(name, size, data);
    }

    int UserMsg_HostageKHandler(const char* name, int size, void* data, UserMsg_RadarMsgNext next)
    {
        BEGIN_READ(data, size);
        int index = READ_BYTE();

        RadarBroadcast::KillHostage(index);
        return next->Invoke(name, size, data);
    }
} // namespace

// Route text drawing through the radar so HUD text (the stock client.dll HUD and
// AMXX hudmessages) is pushed out from under the advanced radar. This patches
// client.dll's own copy of the engine table: the stock client copies the engine
// functions into that copy in Initialize and calls them through it, so patching
// the engine's table would not reach it. NextClient draws its own HUD text through
// the engine table instead, so it is naturally left untouched here.
void RadarHooks_PatchHudText()
{
    cl_enginefunc_t* ef = g_NitroApi->GetClientData()->gEngfuncs;

    if (!ef || g_OrigDrawConsoleString)
    {
        return;
    }

    g_OrigDrawConsoleStringLen = ef->pfnDrawConsoleStringLen; // measure only, not hooked
    g_OrigDrawConsoleString = ef->pfnDrawConsoleString;
    g_OrigDrawString = ef->pfnDrawString;
    g_OrigDrawCharacter = ef->pfnDrawCharacter;
    g_OrigVGUI2DrawCharacter = ef->pfnVGUI2DrawCharacter;
    g_OrigVGUI2DrawCharacterAdd = ef->pfnVGUI2DrawCharacterAdd;
    g_OrigDrawSetTextColor = ef->pfnDrawSetTextColor;
    ef->pfnDrawConsoleString = Hooked_DrawConsoleString;
    ef->pfnDrawString = Hooked_DrawString;
    ef->pfnDrawCharacter = Hooked_DrawCharacter;
    ef->pfnVGUI2DrawCharacter = Hooked_VGUI2DrawCharacter;
    ef->pfnVGUI2DrawCharacterAdd = Hooked_VGUI2DrawCharacterAdd;
    ef->pfnDrawSetTextColor = Hooked_DrawSetTextColor;
    g_PatchedEngTable = ef;
}

void RadarHooks_UnpatchHudText()
{
    if (g_PatchedEngTable && g_OrigDrawConsoleString)
    {
        g_PatchedEngTable->pfnDrawConsoleString = g_OrigDrawConsoleString;
        g_PatchedEngTable->pfnDrawString = g_OrigDrawString;
        g_PatchedEngTable->pfnDrawCharacter = g_OrigDrawCharacter;
        g_PatchedEngTable->pfnVGUI2DrawCharacter = g_OrigVGUI2DrawCharacter;
        g_PatchedEngTable->pfnVGUI2DrawCharacterAdd = g_OrigVGUI2DrawCharacterAdd;
        g_PatchedEngTable->pfnDrawSetTextColor = g_OrigDrawSetTextColor;
    }

    g_GlyphLine.clear(); // drop any buffered glyphs; the orig funcs are being restored
    g_GlyphWidth.clear(); // the measure functions are about to change
    g_PatchedEngTable = nullptr;
    g_OrigDrawConsoleString = nullptr;
    g_OrigDrawConsoleStringLen = nullptr;
    g_OrigDrawString = nullptr;
    g_OrigDrawCharacter = nullptr;
    g_OrigVGUI2DrawCharacter = nullptr;
    g_OrigVGUI2DrawCharacterAdd = nullptr;
    g_OrigDrawSetTextColor = nullptr;
}

void RadarHooks_FlushText()
{
    FlushGlyphLine();
}

void RadarHooks_InvalidateGlyphMetrics()
{
    g_GlyphWidth.clear();
}

void RadarHooks_Subscribe(nitroapi::NitroApiInterface* nitro_api, std::vector<std::shared_ptr<nitroapi::Unsubscriber>>& unsub)
{
    nitroapi::ClientData* client_data = nitro_api->GetClientData();
    unsub.emplace_back(nitro_api->GetEngineData()->CL_HookEvent |= CL_HookEventHandler);
    unsub.emplace_back(client_data->UserMsg_BombDrop |= UserMsg_BombDropHandler);
    unsub.emplace_back(client_data->UserMsg_BombPickup |= UserMsg_BombPickupHandler);
    unsub.emplace_back(client_data->UserMsg_HostagePos |= UserMsg_HostagePosHandler);
    unsub.emplace_back(client_data->UserMsg_HostageK |= UserMsg_HostageKHandler);
}
