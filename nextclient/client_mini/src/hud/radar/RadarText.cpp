#include "RadarText.h"
#include "RadarDraw.h"

#include <next_engine_mini/engine_mini.h>
#include <interface.h>
#include <vgui/IScheme.h>

#include <ncl_math/aabb.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstring>

namespace
{
    // The client-side name copy truncates well before this, so one fixed buffer always holds
    // a whole label and the layout never allocates.
    constexpr int kMaxChars = 40;
    constexpr int kMaxSlots = 80;

    constexpr uint32_t kReplacement = 0xFFFD;

    // Radar labels follow the engine's standard text font (the drop-shadowed console/chat face) so
    // they match the rest of the HUD and track the selected colour scheme. This is the scheme entry
    // pfnDrawConsoleString itself binds.
    constexpr const char* kSchemeFontName = "EngineFont";

    // Extra Latin faces appended to a derived (non-scheme-size) label font; broad script coverage
    // then comes from the text service's own lazy system fallback.
    constexpr const char* kFallbackFaces[] = {"Verdana", "Arial"};

    struct LaidGlyph
    {
        ncl_math::Aabb2 rect; // relative to the pen origin
        ncl_math::Aabb2 uv;
        unsigned int texture;
    };

    struct Layout
    {
        std::array<LaidGlyph, kMaxChars> glyphs;
        int count = 0;
        float width = 0.0f;
        int device_px = 0;
        uint32_t generation = 0;
        char text[64] = {};
        bool valid = false;
    };

    std::array<Layout, kMaxSlots> g_Slots;
    Layout g_Scratch;

    float g_TextMs = 0.0f;

    // Accumulates into the frame total rather than overwriting it: a frame shapes and emits many
    // labels, and the overlay wants their sum.
    class ScopedMsCounter
    {
    public:
        ScopedMsCounter() : start_(std::chrono::steady_clock::now()) {}

        ~ScopedMsCounter()
        {
            std::chrono::duration<float, std::milli> elapsed = std::chrono::steady_clock::now() - start_;
            g_TextMs += elapsed.count();
        }

    private:
        std::chrono::steady_clock::time_point start_;
    };

    // One font handle per pixel size. The service never releases fonts, so this must not create
    // one per frame; the caller's size clamp bounds the set.
    struct SizedFont
    {
        int device_px;
        TextFont_t font;
    };

    std::array<SizedFont, 32> g_Fonts;
    int g_FontCount = 0;

    TextRenderInterface* Service()
    {
        static TextRenderInterface* service = nullptr;
        static bool resolved = false;

        if (!resolved)
        {
            resolved = true;
            CreateInterfaceFn factory = Sys_GetFactory("next_engine_mini.dll");

            if (factory)
            {
                EngineMiniInterface* engine = (EngineMiniInterface*)factory(ENGINE_MINI_INTERFACE_VERSION, nullptr);

                if (engine)
                {
                    service = engine->GetTextRender();
                }
            }
        }

        return service;
    }

    // The standard text font read from the current scheme (face/weight/flags/size). Its HFont is the
    // same handle as a TextFont_t in the shared service, so its descriptor can be read back and mirrored
    // at the radar's own sizes.
    struct SchemeFont
    {
        char face[64];
        int weight;
        int flags; // drop shadow / antialias, exactly as the scheme set them
        int tall; // the standard pixel size for this resolution
    };

    // File-scope so RadarText_ResetGl can drop them on a video mode change, when the scheme font is
    // rebuilt at the new resolution's size (mirrors the sibling caches reset from HudRadarAdvanced::VidInit).
    SchemeFont g_SchemeFont;
    bool g_SchemeResolved = false;

    // Lazily resolved: the scheme manager lives in vgui2.dll and may not be up on the first radar
    // frame. Returns nullptr until it resolves; the caller then stays on the engine-font fallback.
    const SchemeFont* ResolveSchemeFont()
    {
        if (g_SchemeResolved)
        {
            return &g_SchemeFont;
        }

        TextRenderInterface* service = Service();

        if (!service)
        {
            return nullptr;
        }

        CreateInterfaceFn factory = Sys_GetFactory("vgui2.dll");

        if (!factory)
        {
            return nullptr;
        }

        vgui2::ISchemeManager* schemes = (vgui2::ISchemeManager*)factory(VGUI_SCHEME_INTERFACE_VERSION_GS, nullptr);

        if (!schemes)
        {
            return nullptr;
        }

        vgui2::IScheme* scheme = schemes->GetIScheme(schemes->GetDefaultScheme());

        if (!scheme)
        {
            return nullptr;
        }

        vgui2::HFont font = scheme->GetFont(kSchemeFontName, false);

        if (font == 0)
        {
            return nullptr;
        }

        int tall = 0, weight = 0, flags = 0;

        if (!service->GetFontDesc((TextFont_t)font, g_SchemeFont.face, sizeof(g_SchemeFont.face), &tall, &weight, &flags) ||
            tall <= 0 || g_SchemeFont.face[0] == '\0')
        {
            return nullptr;
        }

        g_SchemeFont.tall = tall;
        g_SchemeFont.weight = weight;
        g_SchemeFont.flags = flags;
        g_SchemeResolved = true;

        return &g_SchemeFont;
    }

    TextFont_t FontFor(int device_px)
    {
        for (int i = 0; i < g_FontCount; i++)
        {
            if (g_Fonts[i].device_px == device_px)
            {
                return g_Fonts[i].font;
            }
        }

        TextRenderInterface* service = Service();
        const SchemeFont* scheme = ResolveSchemeFont();

        if (!service || !scheme || g_FontCount == (int)g_Fonts.size())
        {
            return kInvalidTextFont;
        }

        TextFontDesc desc{};
        desc.face = scheme->face;
        desc.tall = device_px;
        desc.weight = scheme->weight;
        desc.flags = scheme->flags; // carries the scheme's drop shadow and antialias choice
        desc.format = TextGlyphFormat::Coverage;

        TextFont_t font = service->FindOrCreateFont(desc);

        if (font == kInvalidTextFont)
        {
            return kInvalidTextFont;
        }

        // At the scheme's own size FindOrCreateFont returns the scheme font itself, which already
        // carries its fallback chain; leave it untouched. A derived size is a fresh font, so give it
        // the Latin fallbacks (broad script coverage still comes from the service's lazy fallback).
        if (device_px != scheme->tall)
        {
            for (const char* face : kFallbackFaces)
            {
                service->AddFallbackFace(font, face);
            }
        }

        g_Fonts[g_FontCount++] = {device_px, font};

        return font;
    }

    // Decodes one UTF-8 sequence, always consuming at least one byte so malformed input
    // cannot spin, and rejecting overlong forms and invalid scalars.
    uint32_t DecodeUtf8(const char** cursor)
    {
        const uint8_t* s = reinterpret_cast<const uint8_t*>(*cursor);
        uint32_t b0 = s[0];
        *cursor += 1;

        if (b0 < 0x80)
        {
            return b0;
        }

        int extra = 0;
        uint32_t cp = 0;
        uint32_t lowest = 0;

        if ((b0 & 0xE0) == 0xC0)
        {
            extra = 1;
            cp = b0 & 0x1F;
            lowest = 0x80;
        }
        else if ((b0 & 0xF0) == 0xE0)
        {
            extra = 2;
            cp = b0 & 0x0F;
            lowest = 0x800;
        }
        else if ((b0 & 0xF8) == 0xF0)
        {
            extra = 3;
            cp = b0 & 0x07;
            lowest = 0x10000;
        }
        else
        {
            return kReplacement;
        }

        for (int i = 0; i < extra; i++)
        {
            uint32_t b = reinterpret_cast<const uint8_t*>(*cursor)[0];

            if ((b & 0xC0) != 0x80)
            {
                return kReplacement;
            }

            cp = (cp << 6) | (b & 0x3F);
            *cursor += 1;
        }

        if (cp < lowest || cp > 0x10FFFF || (cp >= 0xD800 && cp <= 0xDFFF))
        {
            return kReplacement;
        }

        return cp;
    }

    // Reproduces the engine's iswprint filter, which drew a space rather than a missing-glyph
    // box for control characters.
    uint32_t FilterCodepoint(uint32_t cp)
    {
        if (cp < 0x20 || cp == 0x7F || (cp >= 0x80 && cp <= 0x9F))
        {
            return 0x20;
        }

        return cp;
    }

    void BuildLayout(Layout* out, const char* utf8, int device_px)
    {
        out->count = 0;
        out->width = 0.0f;
        out->device_px = device_px;
        out->valid = false;

        TextRenderInterface* service = Service();
        TextFont_t font = FontFor(device_px);

        if (!service || font == kInvalidTextFont)
        {
            return;
        }

        float pen = 0.0f;
        const char* cursor = utf8;

        while (*cursor != '\0' && out->count < kMaxChars)
        {
            uint32_t cp = FilterCodepoint(DecodeUtf8(&cursor));
            const TextGlyph* glyph = service->GetGlyph(font, cp);

            if (!glyph)
            {
                continue; // atlas had no room; the cell resolves on a later frame
            }

            if (glyph->renderable)
            {
                LaidGlyph& laid = out->glyphs[out->count];
                float x = pen + glyph->bearing_x;
                float y = (float)glyph->bearing_y;
                laid.rect = {{x, y}, {x + glyph->wide, y + glyph->tall}};
                laid.uv = {{glyph->u0, glyph->v0}, {glyph->u1, glyph->v1}};
                laid.texture = glyph->texture;
                out->count++;
            }

            pen += glyph->advance;
        }

        out->width = pen;
        // Read after baking, so a repack triggered while shaping cannot be recorded as the
        // generation these cells belong to.
        out->generation = service->Generation();
        out->valid = true;
    }

    Layout& ResolveLayout(int slot, const char* utf8, int device_px)
    {
        TextRenderInterface* service = Service();
        uint32_t generation = service ? service->Generation() : 0;

        if (slot < 0 || slot >= kMaxSlots)
        {
            BuildLayout(&g_Scratch, utf8, device_px);
            return g_Scratch;
        }

        Layout& cached = g_Slots[slot];

        if (cached.valid && cached.device_px == device_px && cached.generation == generation &&
            std::strcmp(cached.text, utf8) == 0)
        {
            return cached;
        }

        std::strncpy(cached.text, utf8, sizeof(cached.text) - 1);
        cached.text[sizeof(cached.text) - 1] = '\0';
        BuildLayout(&cached, utf8, device_px);

        return cached;
    }
} // namespace

bool RadarText_Available(int device_px)
{
    return RadarDraw_PrimsAvailable() && FontFor(device_px) != kInvalidTextFont;
}

int RadarText_StandardPx()
{
    const SchemeFont* scheme = ResolveSchemeFont();

    return scheme ? scheme->tall : 0;
}

float RadarText_Width(int slot, const char* utf8, int device_px)
{
    ScopedMsCounter counter;

    return ResolveLayout(slot, utf8, device_px).width;
}

void RadarText_DrawCentered(int slot, const char* utf8, ncl_math::Vector2 pos, int device_px, ncl_math::Color color)
{
    ScopedMsCounter counter;
    const Layout& layout = ResolveLayout(slot, utf8, device_px);

    if (layout.count == 0)
    {
        return;
    }

    // Snap the origin to whole pixels: the cells are rasterized hinted at this size, and a
    // fractional origin would resample that away.
    float origin_x = std::round(pos.x - layout.width * 0.5f);
    float origin_y = std::round(pos.y);

    for (int i = 0; i < layout.count; i++)
    {
        const LaidGlyph& laid = layout.glyphs[i];
        ncl_math::Aabb2 rect = {
            {origin_x + laid.rect.min.x, origin_y + laid.rect.min.y},
            {origin_x + laid.rect.max.x, origin_y + laid.rect.max.y}
        };
        RadarDraw_DrawGlyph(laid.texture, rect, laid.uv, color);
    }
}

void RadarText_ResetStats()
{
    g_TextMs = 0.0f;
}

float RadarText_Ms()
{
    return g_TextMs;
}

void RadarText_ResetGl()
{
    // A video mode change (this runs from HudRadarAdvanced::VidInit) rebuilds the scheme font at the
    // new resolution's size, so re-resolve it and drop the fonts derived from it; otherwise the label
    // sizes would freeze at the old resolution. The engine owns the atlas and releases it with the GL
    // context, so only the layouts cached here need dropping.
    g_SchemeResolved = false;
    g_FontCount = 0;

    for (Layout& layout : g_Slots)
    {
        layout.valid = false;
    }

    g_Scratch.valid = false;
}
