#include "HudTextBuffer.h"

#include <algorithm>
#include <utility>

namespace
{
    constexpr int kOffscreen = -10000;
    constexpr int kFragmentGap = 16;
} // namespace

void HudTextBuffer::Initialize(const goldsrc::cl_enginefunc_t& engine, void (*set_color)(int, int, int))
{
    engine_ = engine;
    set_color_ = set_color;
}

void HudTextBuffer::Reset()
{
    lines_.clear();
    widths_.clear();
    engine_ = goldsrc::cl_enginefunc_t{};
    set_color_ = nullptr;
}

void HudTextBuffer::SetColor(int r, int g, int b)
{
    r_ = r;
    g_ = g;
    b_ = b;
}

void HudTextBuffer::InvalidateGlyphMetrics()
{
    widths_.clear();
}

void HudTextBuffer::Buffer(HudTextGlyph glyph, int left, int width)
{
    int right = left + width;

    if (!lines_.empty())
    {
        HudTextBufferedLine& line = lines_.back();
        int line_right = line.layout.left + line.layout.width;

        // Consecutive fragments share a baseline, including right-to-left draw calls.
        if (glyph.y == line.layout.y && left <= line_right + kFragmentGap && right >= line.layout.left - kFragmentGap)
        {
            line.layout.left = std::min(line.layout.left, left);
            line.layout.width = std::max(line_right, right) - line.layout.left;
            line.glyphs.push_back(std::move(glyph));
            return;
        }
    }

    int y = glyph.y;
    lines_.push_back({{left, y, width}, {std::move(glyph)}});
}

int HudTextBuffer::DrawConsoleString(int x, int y, char* text)
{
    if (!HudTextZone_IsActive())
    {
        return engine_.pfnDrawConsoleString(x, y, text);
    }

    HudTextGlyph glyph{HudTextGlyphKind::kConsoleString, x, y, 0, r_, g_, b_, 0, text};
    // The original call also resets the ambient color to con_color.
    int result = engine_.pfnDrawConsoleString(x, kOffscreen, text);
    Buffer(std::move(glyph), x, result - x);
    return result;
}

int HudTextBuffer::DrawString(int x, int y, const char* text, int r, int g, int b)
{
    if (!HudTextZone_IsActive())
    {
        return engine_.pfnDrawString(x, y, text, r, g, b);
    }

    int width = engine_.pfnDrawString(x, kOffscreen, text, r, g, b);
    Buffer({HudTextGlyphKind::kString, x, y, 0, r, g, b, 0, text}, x, width);
    return width;
}

int HudTextBuffer::DrawStringReverse(int x, int y, const char* text, int r, int g, int b)
{
    if (!HudTextZone_IsActive())
    {
        return engine_.pfnDrawStringReverse(x, y, text, r, g, b);
    }

    // Both string functions use CreditsFont; the reverse call has a different return contract.
    int width = engine_.pfnDrawString(x, kOffscreen, text, r, g, b);
    int result = engine_.pfnDrawStringReverse(x, kOffscreen, text, r, g, b);
    Buffer({HudTextGlyphKind::kStringReverse, x, y, 0, r, g, b, 0, text}, x - width, width);
    return result;
}

int HudTextBuffer::MeasureGlyph(const HudTextGlyph& glyph)
{
    std::uint64_t key = ((std::uint64_t)glyph.font << 32) | ((std::uint64_t)(std::uint32_t)glyph.ch << 8) | (std::uint32_t)glyph.kind;
    auto it = widths_.find(key);

    if (it != widths_.end())
    {
        return it->second;
    }

    int width = 0;

    switch (glyph.kind)
    {
        case HudTextGlyphKind::kConsole:
            width = engine_.pfnDrawCharacter(kOffscreen, kOffscreen, glyph.ch, glyph.r, glyph.g, glyph.b);
            break;
        case HudTextGlyphKind::kVgui2:
            width = engine_.pfnVGUI2DrawCharacter(kOffscreen, kOffscreen, glyph.ch, glyph.font);
            break;
        case HudTextGlyphKind::kVgui2Add:
            width = engine_.pfnVGUI2DrawCharacterAdd(kOffscreen, kOffscreen, glyph.ch, glyph.r, glyph.g, glyph.b, glyph.font);
            break;
    }

    widths_.emplace(key, width);
    return width;
}

int HudTextBuffer::DrawCharacter(int x, int y, int ch, int r, int g, int b)
{
    if (!HudTextZone_IsActive())
    {
        return engine_.pfnDrawCharacter(x, y, ch, r, g, b);
    }

    HudTextGlyph glyph{HudTextGlyphKind::kConsole, x, y, ch, r, g, b, 0, {}};
    int width = MeasureGlyph(glyph);
    Buffer(std::move(glyph), x, width);
    return width;
}

int HudTextBuffer::VGUI2DrawCharacter(int x, int y, int ch, unsigned int font)
{
    if (!HudTextZone_IsActive())
    {
        return engine_.pfnVGUI2DrawCharacter(x, y, ch, font);
    }

    HudTextGlyph glyph{HudTextGlyphKind::kVgui2, x, y, ch, r_, g_, b_, font, {}};
    int width = MeasureGlyph(glyph);
    Buffer(std::move(glyph), x, width);
    return width;
}

int HudTextBuffer::VGUI2DrawCharacterAdd(int x, int y, int ch, int r, int g, int b, unsigned int font)
{
    if (!HudTextZone_IsActive())
    {
        return engine_.pfnVGUI2DrawCharacterAdd(x, y, ch, r, g, b, font);
    }

    HudTextGlyph glyph{HudTextGlyphKind::kVgui2Add, x, y, ch, r, g, b, font, {}};
    int width = MeasureGlyph(glyph);
    Buffer(std::move(glyph), x, width);
    return width;
}

void HudTextBuffer::Flush()
{
    if (lines_.empty())
    {
        return;
    }

    int saved_r = r_, saved_g = g_, saved_b = b_;
    std::vector<HudTextLine> layouts;
    layouts.reserve(lines_.size());

    for (const HudTextBufferedLine& line : lines_)
    {
        layouts.push_back(line.layout);
    }

    HudTextZone_ComputeBlockShifts(layouts);

    for (size_t i = 0; i < lines_.size(); i++)
    {
        for (const HudTextGlyph& glyph : lines_[i].glyphs)
        {
            int x = glyph.x + layouts[i].dx, y = glyph.y + layouts[i].dy;

            switch (glyph.kind)
            {
                case HudTextGlyphKind::kConsole:
                    engine_.pfnDrawCharacter(x, y, glyph.ch, glyph.r, glyph.g, glyph.b);
                    break;
                case HudTextGlyphKind::kVgui2:
                    set_color_(glyph.r, glyph.g, glyph.b);
                    engine_.pfnVGUI2DrawCharacter(x, y, glyph.ch, glyph.font);
                    break;
                case HudTextGlyphKind::kVgui2Add:
                    engine_.pfnVGUI2DrawCharacterAdd(x, y, glyph.ch, glyph.r, glyph.g, glyph.b, glyph.font);
                    break;
                case HudTextGlyphKind::kConsoleString:
                    set_color_(glyph.r, glyph.g, glyph.b);
                    engine_.pfnDrawConsoleString(x, y, const_cast<char*>(glyph.text.c_str()));
                    break;
                case HudTextGlyphKind::kString:
                    engine_.pfnDrawString(x, y, glyph.text.c_str(), glyph.r, glyph.g, glyph.b);
                    break;
                case HudTextGlyphKind::kStringReverse:
                    engine_.pfnDrawStringReverse(x, y, glyph.text.c_str(), glyph.r, glyph.g, glyph.b);
                    break;
            }
        }
    }

    set_color_(saved_r, saved_g, saved_b);
    lines_.clear();
}
