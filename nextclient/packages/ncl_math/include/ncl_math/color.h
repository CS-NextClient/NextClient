#pragma once

#include <cstdint>
#include <type_traits>

#include "scalar.h"

// RGBA color value types. Two representations coexist: Color (8-bit channels) is
// the primary one because the HUD, engine fill calls and the triangle API all speak
// bytes; ColorF (normalized floats) is for shader uniforms and blends. Byte<->float
// conversion truncates (matches the codebase's (int)(x*255) convention).
namespace ncl_math
{
    struct ColorF;

    struct Color
    {
        std::uint8_t r{}, g{}, b{}, a{255};

        constexpr Color() = default;
        // Channels are ints (0..255) so callers can pass the loose r/g/b/a they
        // already have without a narrowing brace-init error.
        constexpr Color(int red, int green, int blue, int alpha = 255) :
            r(static_cast<std::uint8_t>(red)),
            g(static_cast<std::uint8_t>(green)),
            b(static_cast<std::uint8_t>(blue)),
            a(static_cast<std::uint8_t>(alpha))
        {
        }

        // Packed 0x00RRGGBB (alpha defaults to opaque).
        static Color FromPacked(std::uint32_t rgb)
        {
            return {(std::uint8_t)((rgb >> 16) & 0xFF), (std::uint8_t)((rgb >> 8) & 0xFF), (std::uint8_t)(rgb & 0xFF), 255};
        }

        Color WithAlpha(int alpha) const
        {
            return {r, g, b, (std::uint8_t)Clamp((float)alpha, 0.0f, 255.0f)};
        }

        bool operator==(const Color& o) const { return r == o.r && g == o.g && b == o.b && a == o.a; }
        bool operator!=(const Color& o) const { return !(*this == o); }

        ColorF ToFloat() const;
    };
    static_assert(sizeof(Color) == 4);
    static_assert(std::is_standard_layout_v<Color>);

    struct ColorF
    {
        float r{}, g{}, b{}, a{1.0f};

        float* data() { return &r; }
        const float* data() const { return &r; }

        ColorF WithAlpha(float alpha) const { return {r, g, b, alpha}; }

        bool operator==(const ColorF& o) const { return r == o.r && g == o.g && b == o.b && a == o.a; }
        bool operator!=(const ColorF& o) const { return !(*this == o); }

        Color ToBytes() const
        {
            return {(std::uint8_t)Clamp(r * 255.0f, 0.0f, 255.0f),
                    (std::uint8_t)Clamp(g * 255.0f, 0.0f, 255.0f),
                    (std::uint8_t)Clamp(b * 255.0f, 0.0f, 255.0f),
                    (std::uint8_t)Clamp(a * 255.0f, 0.0f, 255.0f)};
        }
    };
    static_assert(sizeof(ColorF) == sizeof(float[4]));
    static_assert(std::is_standard_layout_v<ColorF>);

    inline ColorF Color::ToFloat() const
    {
        return {r / 255.0f, g / 255.0f, b / 255.0f, a / 255.0f};
    }

    // Per-channel linear blend. The byte overload blends in float space then
    // truncates back, matching the hand-rolled (int)lerp(r, ...) fade code.
    inline Color Lerp(const Color& a, const Color& b, float t)
    {
        return {(std::uint8_t)Clamp(Lerp((float)a.r, (float)b.r, t), 0.0f, 255.0f),
                (std::uint8_t)Clamp(Lerp((float)a.g, (float)b.g, t), 0.0f, 255.0f),
                (std::uint8_t)Clamp(Lerp((float)a.b, (float)b.b, t), 0.0f, 255.0f),
                (std::uint8_t)Clamp(Lerp((float)a.a, (float)b.a, t), 0.0f, 255.0f)};
    }

    inline ColorF Lerp(const ColorF& a, const ColorF& b, float t)
    {
        return {Lerp(a.r, b.r, t), Lerp(a.g, b.g, t), Lerp(a.b, b.b, t), Lerp(a.a, b.a, t)};
    }
} // namespace ncl_math
