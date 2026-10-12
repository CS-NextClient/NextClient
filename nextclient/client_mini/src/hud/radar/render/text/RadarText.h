#pragma once

#include <array>
#include <cstdint>

#include <ncl_math/vec3.h>
#include <ncl_math/color.h>
#include <ncl_math/aabb.h>

typedef int TextFont_t;

class RadarText
{
private:
    struct LaidGlyph
    {
        ncl_math::Aabb2 rect; // relative to the pen origin
        ncl_math::Aabb2 uv;
        unsigned int texture;
    };

    // Must precede Layout, whose type uses this array bound.
    // The client-side name copy truncates well before this, so a label always fits.
    static constexpr int kMaxChars = 40;

    struct Layout
    {
        std::array<LaidGlyph, kMaxChars> glyphs;
        int count{};
        float width{};
        int device_px{};
        std::uint32_t generation{};
        char text[64]{};
        bool valid{};
    };

    // The service never releases fonts, so this table must not grow per frame; a full table
    // falls back to the engine-text renderer rather than failing.
    struct SizedFont
    {
        int device_px;
        TextFont_t font;
    };

    // Must precede slots_, whose type uses this array bound.
    static constexpr int kMaxSlots = 80;

    std::array<Layout, kMaxSlots> slots_{};
    Layout scratch_{};

    // Must precede fonts_, whose type uses this array bound.
    static constexpr int kMaxFonts = 48;

    std::array<SizedFont, kMaxFonts> fonts_{};
    int font_count_{};

    float ms_{};

public:
    bool CheckAvailable(int device_px);

    // Zero until the scheme font resolves.
    int ResolveStandardPx() const;

    // pos is the horizontal center and the top of the text.
    void DrawCentered(int slot, const char* utf8, ncl_math::Vector2 pos, int device_px, ncl_math::Color color);

    void ResetGl();

    void ResetStats()
    {
        ms_ = 0.0f;
    }

    float get_ms() const
    {
        return ms_;
    }

private:
    TextFont_t ResolveFont(int device_px);
    void BuildLayout(Layout& out, const char* utf8, int device_px);
    Layout& ResolveLayout(int slot, const char* utf8, int device_px);
};
