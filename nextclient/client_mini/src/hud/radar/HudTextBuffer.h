#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

#include <goldsrc/client_api.h>

#include "HudTextZone.h"

enum class HudTextGlyphKind
{
    kConsole,
    kVgui2,
    kVgui2Add,
    kConsoleString,
    kString,
    kStringReverse,
};

struct HudTextGlyph
{
    HudTextGlyphKind kind;
    int x, y, ch;
    int r, g, b;
    unsigned int font;
    std::string text;
};

struct HudTextBufferedLine
{
    HudTextLine layout;
    std::vector<HudTextGlyph> glyphs;
};

class HudTextBuffer
{
public:
    void Initialize(const goldsrc::cl_enginefunc_t& engine, void (*set_color)(int, int, int));
    void Reset();
    void SetColor(int r, int g, int b);
    void Flush();
    void InvalidateGlyphMetrics();

    int DrawConsoleString(int x, int y, char* text);
    int DrawString(int x, int y, const char* text, int r, int g, int b);
    int DrawStringReverse(int x, int y, const char* text, int r, int g, int b);
    int DrawCharacter(int x, int y, int ch, int r, int g, int b);
    int VGUI2DrawCharacter(int x, int y, int ch, unsigned int font);
    int VGUI2DrawCharacterAdd(int x, int y, int ch, int r, int g, int b, unsigned int font);

private:
    void Buffer(HudTextGlyph glyph, int left, int width);
    int MeasureGlyph(const HudTextGlyph& glyph);

    goldsrc::cl_enginefunc_t engine_{};
    void (*set_color_)(int, int, int){};
    int r_ = 255;
    int g_ = 255;
    int b_ = 255;
    std::vector<HudTextBufferedLine> lines_;
    std::unordered_map<std::uint64_t, int> widths_;
};
