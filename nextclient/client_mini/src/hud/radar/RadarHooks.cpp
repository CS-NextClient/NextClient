#include "RadarHooks.h"

#include <cmath>
#include <cstdint>
#include <cstring>
#include <unordered_map>
#include <vector>

#include "main.h"

#include <next_client_mini/client_mini.h>
#include <data_types/MessageReader.h>
#include <goldsrc/client_api.h>
#include <goldsrc/event_api.h>

#include "RadarBroadcast.h"
#include "HudTextZone.h"
#include "SmokeRegistry.h"

namespace
{
    goldsrc::pfnEngSrc_pfnDrawConsoleString_t g_OrigDrawConsoleString;
    goldsrc::pfnEngSrc_pfnDrawConsoleStringLen_t g_OrigDrawConsoleStringLen;
    goldsrc::pfnEngSrc_pfnDrawString_t g_OrigDrawString;
    goldsrc::pfnEngSrc_pfnDrawCharacter_t g_OrigDrawCharacter;
    goldsrc::pfnEngSrc_pfnVGUI2DrawCharacter_t g_OrigVGUI2DrawCharacter;
    goldsrc::pfnEngSrc_pfnVGUI2DrawCharacterAdd_t g_OrigVGUI2DrawCharacterAdd;
    goldsrc::pfnEngSrc_pfnDrawSetTextColor_t g_OrigDrawSetTextColor;
    goldsrc::cl_enginefunc_t* g_PatchedEngTable;

    // VGUI2DrawCharacter takes no colour argument: it draws in the engine's ambient colour,
    // set by DrawSetTextColor. Track it so a buffered glyph replays in its own colour.
    int g_TextR = 255, g_TextG = 255, g_TextB = 255;

    int MeasureTextWidth(const char* string)
    {
        int width = 0, height = 0;

        if (g_OrigDrawConsoleStringLen)
        {
            g_OrigDrawConsoleStringLen(string, &width, &height);
        }

        return width;
    }

    // Glyph text arrives one glyph at a time, so a line's width is known only after its
    // last glyph: an in-zone line is buffered and drawn as one shifted line later. The
    // advance a hook returns is measured off-screen; the engine's width is position-independent.
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

    // Glyph advance depends only on (kind, char, font) and changes only when fonts are rebuilt.
    std::unordered_map<std::uint64_t, int> g_GlyphWidth;

    template <class Measure>
    int MeasureGlyphWidth(GlyphKind kind, int ch, unsigned int font, Measure measure)
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
        HudTextZone_ComputeLineShift(g_LineFirstX, g_LineY, g_LineRight - g_LineFirstX, dx, dy);

        for (const BufGlyph& g : g_GlyphLine)
        {
            int gx = g.x + dx, gy = g.y + dy;

            switch (g.kind)
            {
                case kGlyphConsole:
                    g_OrigDrawCharacter(gx, gy, g.ch, g.r, g.g, g.b);
                    break;
                case kGlyphVgui2:
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

        // Leave the ambient colour as the caller last set it, so the line that triggered
        // a mid-frame flush keeps its own colour.
        if (g_OrigDrawSetTextColor)
        {
            g_OrigDrawSetTextColor(g_TextR / 255.0f, g_TextG / 255.0f, g_TextB / 255.0f);
        }

        g_GlyphLine.clear();
    }

    void BufferGlyph(GlyphKind kind, int x, int y, int ch, int r, int g, int b, unsigned int font, int width)
    {
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

    int Hooked_DrawConsoleString(int x, int y, char* string)
    {
        HudTextZone_ShiftString(x, y, HudTextZone_IsRowInZone(y) ? MeasureTextWidth(string) : 0);
        return g_OrigDrawConsoleString(x, y, string);
    }

    int Hooked_DrawString(int x, int y, const char* string, int r, int g, int b)
    {
        HudTextZone_ShiftString(x, y, HudTextZone_IsRowInZone(y) ? MeasureTextWidth(string) : 0);
        return g_OrigDrawString(x, y, string, r, g, b);
    }

    int Hooked_DrawCharacter(int x, int y, int number, int r, int g, int b)
    {
        if (!HudTextZone_IsRowInZone(y))
        {
            return g_OrigDrawCharacter(x, y, number, r, g, b);
        }

        int width = MeasureGlyphWidth(kGlyphConsole, number, 0, [&] {
            return g_OrigDrawCharacter(kOffscreen, kOffscreen, number, r, g, b);
        });
        BufferGlyph(kGlyphConsole, x, y, number, r, g, b, 0, width);
        return width;
    }

    int Hooked_VGUI2DrawCharacter(int x, int y, int ch, unsigned int font)
    {
        if (!HudTextZone_IsRowInZone(y))
        {
            return g_OrigVGUI2DrawCharacter(x, y, ch, font);
        }

        int width = MeasureGlyphWidth(kGlyphVgui2, ch, font, [&] {
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
        if (!HudTextZone_IsRowInZone(y))
        {
            return g_OrigVGUI2DrawCharacterAdd(x, y, ch, r, g, b, font);
        }

        int width = MeasureGlyphWidth(kGlyphVgui2Add, ch, font, [&] {
            return g_OrigVGUI2DrawCharacterAdd(kOffscreen, kOffscreen, ch, r, g, b, font);
        });
        BufferGlyph(kGlyphVgui2Add, x, y, ch, r, g, b, font, width);
        return width;
    }

    RadarSinks g_Sinks;
    void (*g_OrigCreatesmoke)(goldsrc::event_args_t*);

    bool IsNonZero(const ncl_math::Vector3& v)
    {
        return std::fabs(v.x) > 1.0f || std::fabs(v.y) > 1.0f || std::fabs(v.z) > 1.0f;
    }

    void CreateSmoke_Hook(goldsrc::event_args_t* args)
    {
        // The exact centre rides in angles for the per-second puffs and in origin for the
        // detonation, which is the event with iparam2 == 1.
        if (g_Sinks.smoke)
        {
            bool has_angles = IsNonZero(ncl_math::Vector3::Ref(args->angles));
            const float* exact = has_angles ? args->angles : args->origin;
            bool detonation = (args->iparam2 == 1);

            g_Sinks.smoke->OnSmoke(ncl_math::Vector3::Ref(exact), has_angles || detonation, gEngfuncs.GetClientTime());
        }

        if (g_OrigCreatesmoke)
        {
            g_OrigCreatesmoke(args);
        }
    }

    void CL_HookEventHandler(char* name, void (*cb)(goldsrc::event_args_t*), CL_HookEventNext next)
    {
        if (name && std::strstr(name, "createsmoke"))
        {
            g_OrigCreatesmoke = cb;
            next->Invoke(name, CreateSmoke_Hook);
            return;
        }

        next->Invoke(name, cb);
    }

    int UserMsg_BombDropHandler(const char* name, int size, void* data, UserMsg_RadarMsgNext next)
    {
        MessageReader reader(data, size);
        float x = reader.ReadCoord();
        float y = reader.ReadCoord();
        float z = reader.ReadCoord();
        int planted = reader.ReadByte();

        ncl_math::Vector3 pos{x, y, z};
        float time = gEngfuncs.GetClientTime();

        if (g_Sinks.broadcast)
        {
            if (pos == ncl_math::Vector3{})
            {
                g_Sinks.broadcast->ClearBomb(time); // engine "hide" sentinel
            }
            else
            {
                g_Sinks.broadcast->SetBomb(pos, planted != 0, time);
            }
        }

        return next->Invoke(name, size, data);
    }

    int UserMsg_BombPickupHandler(const char* name, int size, void* data, UserMsg_RadarMsgNext next)
    {
        if (g_Sinks.broadcast)
        {
            g_Sinks.broadcast->ClearBomb(gEngfuncs.GetClientTime());
        }

        return next->Invoke(name, size, data);
    }

    int UserMsg_HostagePosHandler(const char* name, int size, void* data, UserMsg_RadarMsgNext next)
    {
        MessageReader reader(data, size);
        reader.ReadByte(); // 1 on the batch a spawning player gets, 0 on the periodic updates
        int index = reader.ReadByte();
        float x = reader.ReadCoord();
        float y = reader.ReadCoord();
        reader.ReadCoord(); // z, unused on the 2D radar

        if (g_Sinks.broadcast)
        {
            g_Sinks.broadcast->SetHostage(index, {x, y}, gEngfuncs.GetClientTime());
        }

        return next->Invoke(name, size, data);
    }

    int UserMsg_HostageKHandler(const char* name, int size, void* data, UserMsg_RadarMsgNext next)
    {
        MessageReader reader(data, size);
        int index = reader.ReadByte();

        if (g_Sinks.broadcast)
        {
            g_Sinks.broadcast->KillHostage(index);
        }

        return next->Invoke(name, size, data);
    }
} // namespace

// Patches client.dll's own copy of the engine table: the stock client copies the engine
// functions into it in Initialize and calls them through the copy, so patching the engine's
// table would not reach it. NextClient's HUD text uses the engine table and stays unshifted.
void RadarHooks_PatchHudText()
{
    goldsrc::cl_enginefunc_t* ef = g_NitroApi->GetClientData()->gEngfuncs;

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

    g_GlyphLine.clear();
    g_GlyphWidth.clear();
    g_PatchedEngTable = nullptr;
    g_OrigDrawConsoleString = nullptr;
    g_OrigDrawConsoleStringLen = nullptr;
    g_OrigDrawString = nullptr;
    g_OrigDrawCharacter = nullptr;
    g_OrigVGUI2DrawCharacter = nullptr;
    g_OrigVGUI2DrawCharacterAdd = nullptr;
    g_OrigDrawSetTextColor = nullptr;
}

void RadarHooks_SetSinks(const RadarSinks& sinks)
{
    g_Sinks = sinks;
}

void RadarHooks_ClearSinks()
{
    g_Sinks = RadarSinks{};
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
