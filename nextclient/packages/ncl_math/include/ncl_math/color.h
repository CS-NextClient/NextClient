#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <type_traits>

#include "scalar.h"

// RGBA color value types: Color with 8-bit channels and ColorF with normalized
// float channels. Float-to-byte conversion truncates.
namespace ncl_math
{
    struct ColorF;

    struct Color
    {
        std::uint8_t r{}, g{}, b{}, a{255};

        constexpr Color() = default;
        // Channels are taken as int (0..255) so that int arguments
        // brace-initialize without a narrowing error.
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

    // Per-channel linear blend; the byte overload blends in float space and
    // truncates back.
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

    // h, s and v in [0, 1]. Alpha is 1.
    inline ColorF HsvToRgb(float h, float s, float v)
    {
        float sector = std::fmod(h, 1.0f) * 6.0f;
        int index = static_cast<int>(sector);
        float fraction = sector - index;

        float p = v * (1.0f - s);
        float q = v * (1.0f - s * fraction);
        float t = v * (1.0f - s * (1.0f - fraction));

        switch (index % 6)
        {
        case 0: return {v, t, p};
        case 1: return {q, v, p};
        case 2: return {p, v, t};
        case 3: return {p, q, v};
        case 4: return {t, p, v};
        default: return {v, p, q};
        }
    }

    // Inverse of HsvToRgb over the color's rgb; alpha is ignored. A grey color has no hue,
    // and h is then left as passed in.
    inline void RgbToHsv(const ColorF& rgb, float& h, float& s, float& v)
    {
        float high = std::max({rgb.r, rgb.g, rgb.b});
        float low = std::min({rgb.r, rgb.g, rgb.b});
        float span = high - low;

        v = high;
        s = high > 0.0f ? span / high : 0.0f;

        if (span <= 0.0f)
        {
            return;
        }

        if (high == rgb.r)
        {
            h = (rgb.g - rgb.b) / span;
        }
        else if (high == rgb.g)
        {
            h = 2.0f + (rgb.b - rgb.r) / span;
        }
        else
        {
            h = 4.0f + (rgb.r - rgb.g) / span;
        }

        h /= 6.0f;

        if (h < 0.0f)
        {
            h += 1.0f;
        }
    }
} // namespace ncl_math
