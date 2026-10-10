#include "RadarText.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>

#include <next_engine_mini/engine_mini.h>
#include <interface.h>
#include <vgui/IScheme.h>
#include <strtools.h>
#include <ncl_math/aabb.h>
#include <ncl_utils/scoped_ms_counter.h>

#include "hud/radar/render/RadarDraw.h"

namespace
{
    constexpr uint32_t kReplacement = 0xFFFD;

    // The scheme entry pfnDrawConsoleString itself binds, so labels track the HUD's font.
    constexpr const char* kSchemeFontName = "EngineFont";

    constexpr const char* kFallbackFaces[] = {"Verdana", "Arial"};

    TextRenderInterface* ResolveTextService()
    {
        static TextRenderInterface* service = nullptr;
        static bool resolved = false;

        if (!resolved)
        {
            resolved = true;
            CreateInterfaceFn factory = Sys_GetFactory("next_engine_mini.dll");

            if (factory)
            {
                EngineMiniInterface* engine = static_cast<EngineMiniInterface*>(factory(ENGINE_MINI_INTERFACE_VERSION, nullptr));

                if (engine)
                {
                    service = engine->GetTextRender();
                }
            }
        }

        return service;
    }

    // A vgui2 HFont is the same handle as a TextFont_t in the shared service, so the scheme font's
    // descriptor can be read back and mirrored at the radar's own sizes.
    struct SchemeFont
    {
        char face[64];
        int weight;
        int flags;
        int tall;
    };

    SchemeFont g_SchemeFont;
    bool g_SchemeResolved = false;

    // The scheme manager lives in vgui2.dll and may not be up on the first radar frame, hence the
    // lazy resolve; null until it is.
    const SchemeFont* ResolveSchemeFont()
    {
        if (g_SchemeResolved)
        {
            return &g_SchemeFont;
        }

        TextRenderInterface* service = ResolveTextService();

        if (!service)
        {
            return nullptr;
        }

        CreateInterfaceFn factory = Sys_GetFactory("vgui2.dll");

        if (!factory)
        {
            return nullptr;
        }

        vgui2::ISchemeManager* schemes = static_cast<vgui2::ISchemeManager*>(factory(VGUI_SCHEME_INTERFACE_VERSION_GS, nullptr));

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

    // Q_UTF8ToUChar32 reports malformation through the flag and always consumes at least one
    // byte, so the shaping loop cannot spin.
    uint32_t DecodeUtf8(const char*& cursor)
    {
        uchar32 cp = 0;
        bool error = false;
        cursor += Q_UTF8ToUChar32(cursor, cp, error);

        return error ? kReplacement : (uint32_t)cp;
    }

    uint32_t FilterCodepoint(uint32_t cp)
    {
        if (cp < 0x20 || cp == 0x7F || (cp >= 0x80 && cp <= 0x9F))
        {
            return 0x20;
        }

        return cp;
    }

} // namespace

TextFont_t RadarText::ResolveFont(int device_px)
{
    for (int i = 0; i < font_count_; i++)
    {
        if (fonts_[i].device_px == device_px)
        {
            return fonts_[i].font;
        }
    }

    TextRenderInterface* service = ResolveTextService();
    const SchemeFont* scheme = ResolveSchemeFont();

    if (!service || !scheme || font_count_ == (int)fonts_.size())
    {
        return kInvalidTextFont;
    }

    TextFontDesc desc{};
    desc.face = scheme->face;
    desc.tall = device_px;
    desc.weight = scheme->weight;
    desc.flags = scheme->flags;
    desc.format = TextGlyphFormat::Coverage;

    TextFont_t font = service->FindOrCreateFont(desc);

    if (font == kInvalidTextFont)
    {
        return kInvalidTextFont;
    }

    // At the scheme's own size FindOrCreateFont returns the scheme font itself, which already
    // carries its fallback chain; only a derived size is a fresh font needing the Latin faces.
    if (device_px != scheme->tall)
    {
        for (const char* face : kFallbackFaces)
        {
            service->AddFallbackFace(font, face);
        }
    }

    fonts_[font_count_++] = {device_px, font};

    return font;
}

void RadarText::BuildLayout(Layout& out, const char* utf8, int device_px)
{
    out.count = 0;
    out.width = 0.0f;
    out.device_px = device_px;
    out.valid = false;

    TextRenderInterface* service = ResolveTextService();
    TextFont_t font = ResolveFont(device_px);

    if (!service || font == kInvalidTextFont)
    {
        return;
    }

    float pen = 0.0f;
    const char* cursor = utf8;
    bool complete = true;

    while (*cursor != '\0' && out.count < kMaxChars)
    {
        uint32_t cp = FilterCodepoint(DecodeUtf8(cursor));
        const TextGlyph* glyph = service->GetGlyph(font, cp);

        if (!glyph)
        {
            // Atlas had no room: not cached as final, or the slot would keep serving this
            // truncated shape once the cell becomes bakeable.
            complete = false;
            continue;
        }

        if (glyph->renderable)
        {
            LaidGlyph& laid = out.glyphs[out.count];
            float x = pen + glyph->bearing_x;
            float y = (float)glyph->bearing_y;
            laid.rect = {{x, y}, {x + glyph->wide, y + glyph->tall}};
            laid.uv = {{glyph->u0, glyph->v0}, {glyph->u1, glyph->v1}};
            laid.texture = glyph->texture;
            out.count++;
        }

        pen += glyph->advance;
    }

    out.width = pen;
    // Read after baking: a repack triggered while shaping must not be recorded as this layout's generation.
    out.generation = service->Generation();
    out.valid = complete;
}

RadarText::Layout& RadarText::ResolveLayout(int slot, const char* utf8, int device_px)
{
    TextRenderInterface* service = ResolveTextService();
    uint32_t generation = service ? service->Generation() : 0;

    if (slot < 0 || slot >= kMaxSlots)
    {
        BuildLayout(scratch_, utf8, device_px);
        return scratch_;
    }

    Layout& cached = slots_[slot];

    if (cached.valid && cached.device_px == device_px && cached.generation == generation &&
        std::strcmp(cached.text, utf8) == 0)
    {
        return cached;
    }

    std::strncpy(cached.text, utf8, sizeof(cached.text) - 1);
    cached.text[sizeof(cached.text) - 1] = '\0';
    BuildLayout(cached, utf8, device_px);

    return cached;
}

bool RadarText::CheckAvailable(int device_px)
{
    return RadarDraw_CheckPrimsAvailable() && ResolveFont(device_px) != kInvalidTextFont;
}

int RadarText::ResolveStandardPx() const
{
    const SchemeFont* scheme = ResolveSchemeFont();

    return scheme ? scheme->tall : 0;
}

void RadarText::DrawCentered(int slot, const char* utf8, ncl_math::Vector2 pos, int device_px, ncl_math::Color color)
{
    ncl_utils::ScopedMsCounter counter{ms_};
    const Layout& layout = ResolveLayout(slot, utf8, device_px);

    if (layout.count == 0)
    {
        return;
    }

    // Whole pixels: the cells are rasterized hinted at this size, a fractional origin resamples that away.
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

void RadarText::ResetGl()
{
    // A video mode change rebuilds the scheme font at the new resolution's size, so re-resolve it and
    // drop the fonts derived from it, or the label sizes freeze at the old resolution. The engine owns
    // the atlas, so only the caches held here need dropping.
    g_SchemeResolved = false;
    font_count_ = 0;

    for (Layout& layout : slots_)
    {
        layout.valid = false;
    }

    scratch_.valid = false;
}
